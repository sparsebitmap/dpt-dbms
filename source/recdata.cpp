
#include "stdafx.h"

#include "recdata.h"

//Utils
#include "dataconv.h"
//API Tiers
#include "reccopy.h"
#include "record.h"
#include "dbf_tableb.h"
#include "dbf_tabled.h" //V3. BLOBs
#include "dbf_data.h"
#include "dbf_field.h"
#include "dbfile.h"
#include "dbctxt.h"
#include "page_b.h"
#include "page_l.h" //V3. BLOBs
#include "dbserv.h"
#include "core.h"
#include "msgroute.h"
//Diagnostics
#include "except.h"
#include "msg_db.h"

namespace dpt {

//*************************************************************************************
void RecordDataAccessor::DelayedConstruct(Record* r) 
{
	record = r;

	//See related lengthy comments elsewhere (start with Record::CheckEBM())
	if (!noregister)
		record->HomeFileContext()->GetDBFile()->GetDataMgr()->RegisterRecordMRO(this);
}

//*************************************************************************************
RecordDataAccessor::~RecordDataAccessor()
{
	DatabaseFile* file = record->HomeFileContext()->GetDBFile();
	if (!noregister)
		file->GetDataMgr()->DeregisterRecordMRO(this);
}


//*************************************************************************************
//See also notes in Record::CheckEBM().
//*************************************************************************************
void RecordDataAccessor::ThrowUnlockedRecordDeleted(bool updating) const 
{
	SingleDatabaseFileContext* sfc = record->HomeFileContext();
	DatabaseFile* file = sfc->GetDBFile();
	DatabaseServices* dbapi = sfc->DBAPI();

	std::string msg = std::string("Record # ")
		.append(util::IntToString(record->RecNum()))
		.append(" in file ")
		.append(file->FileName(sfc));
	
	if (deleter == dbapi)
		msg.append(" has recently been deleted - it can no longer be accessed");
	else
		msg.append(" has been deleted elsewhere - review your locking strategy");

	//Since we check this before any changes, we should be able to TBO with 
	//no problems, so issue a message here and then trigger it.
	if (updating) {
		dbapi->Core()->GetRouter()->Issue(DML_SICK_RECORD, msg);
		throw Exception(TXN_BENIGN_ATOM, "Deleted record");
	}
	else {
		//Same user did it - we'll handle this further out
		if (deleter == dbapi)
			throw Exception(DML_NONEXISTENT_RECORD, msg);
		//Different user - someone is accessing the record unlocked
		else
			throw Exception(DML_SICK_RECORD, msg);
	}
}

//*************************************************************************************
//We cache the last-used buffer page, which therefore means that a record
//MRO will always have at least one page marked in-use once you start using it.
//This is essential to stop DKPR clocking up one per field access, which is not what
//M204 does. 
//V3.0.  For fields with BLOB data on table E pages they will however clock up
//DKPR for every field access, for the table E page.
//*************************************************************************************
void RecordDataAccessor::CachePageForRec(int recnum) const
{
	SingleDatabaseFileContext* sfc = record->HomeFileContext();
	DatabaseFile* file = sfc->GetDBFile();
	DatabaseFileTableBManager* tbm = file->GetTableBMgr();

	int p = tbm->BPageNumFromRecNum(recnum);

	//Don't reread unless necessary
	if (p != cached_buffer_pagenum) {
		buffcache = tbm->GetTableBPage(sfc->DBAPI(), p);
		cached_buffer_pagenum = p;
	}
}

//*************************************************************************************
bool RecordDataAccessor::CursorAdvanceFieldOccurrence(PhysicalFieldInfo* pfi) const
{
	try {
		pai_cursor_fvpix = INT_MAX;
		DatabaseFile* file = record->HomeFileContext()->GetDBFile();

		//Pick up where we left off last time
		int occ = data_cursor.Occ();
		int recnum = data_cursor.RecNum();
		short recoffset = data_cursor.RecOffset();

		//This is unlikely but would cause problems for some algorithms
		if (occ >= INT_MAX - 2)
			throw Exception(DB_ALGORITHM_BUG, 
				"Bug: Maximum number of field occurrences per record reached");

		//Chain through extensions if required to reach the next occ
		for (;;) {

			CachePageForRec(recnum);
			RecordDataPage pb(buffcache);

			short slot = file->GetTableBMgr()->BPageSlotFromRecNum(recnum);

			bool gotnext = pb.SlotAdvanceField(slot, recoffset, pfi);

			//Next field found
			if (gotnext) {
				if (pfi)
					//We were looking for a particular field - cache occ # and its offset
					data_cursor.SetOcc(pfi->id, occ+1, recnum, recoffset);
				else
					//We were looking for any next field - just cache offset
					data_cursor.SetPos(recnum, recoffset);

				return true;
			}
			
			//End of this extent
			if (pfi)
				//Cache pos of the end of the record, ready for append if required
				data_cursor.SetPos(recnum, recoffset);
			else
				//PAI is finished - no need to keep any cached info
				data_cursor.ResetPos();

			//Go to next extent if there is one
			int ern = pb.SlotGetExtensionPtr(slot);
			if (ern == -1)
				break;

			recnum = ern;
			recoffset = 0;
		}

		return false;
	}
	//All field accesses come through here, so we can use it to catch this exception.
	//See comments in record.cpp about how this handling has changed.
	catch (Exception& e) {
		if (e.Code() == DML_NONEXISTENT_RECORD)
			record->ThrowNonexistent();
		throw;
	}
}

//*************************************************************************************
//V3.0.  A trimmed down version of the above.  For this we don't want to mess up the
//cursor position we work so hard to preserve, just check the slot exists.
void RecordDataAccessor::PreUpdatePeek() const
{
	try {
		int recnum = data_cursor.RecNum();

		CachePageForRec(recnum);
		RecordDataPage pb(buffcache);

		short slot = record->HomeFileContext()->GetDBFile()->GetTableBMgr()->BPageSlotFromRecNum(recnum);

		int offset = pb.MapRecordOffset(slot, false);
	}
	catch (Exception& e) {
		if (e.Code() == DML_NONEXISTENT_RECORD)
			record->ThrowNonexistent();
		throw;
	}
}


//*************************************************************************************
bool RecordDataAccessor::LocateFieldOccurrenceByNumber
(PhysicalFieldInfo* pfi, const int required_occ, int* inctr) const
{
	int dummy;
	int& occ_counter = (inctr) ? *inctr : dummy;

	occ_counter = 0;

	if (required_occ <= 0)
		return false;
	
	//Is the desired occ after the last place we left off?
	if (pfi->id == data_cursor.Fid() && required_occ >= data_cursor.Occ())
		occ_counter = data_cursor.Occ();

	//Otherwise we have to scan from the start
	else
		data_cursor.ResetPos();

	int advance_occs = required_occ - occ_counter;

	//Skip along the record to the requested occ
	for (; advance_occs > 0; advance_occs--) {
		if (CursorAdvanceFieldOccurrence(pfi))
			occ_counter++;
		else
			//It doesn't exist
			return false;
	}

	return true;
}

//*************************************************************************************
bool RecordDataAccessor::LocateFieldOccurrenceByValue
(PhysicalFieldInfo* pfi, const FieldValue& required_val, int* pocc) const
{
	//Scan the entire record - if we're lucky we might already be at occ 1
	*pocc = 0;
	while (LocateFieldOccurrenceByNumber(pfi, *pocc+1)) {
		(*pocc)++;

		FieldValue v;
		RetrieveFVPair(NULL, &v);

		if (v == required_val)
			return true;
	}
	
	return false;
}

//*************************************************************************************
//void RecordDataAccessor::RetrieveFVPair(FieldID* fid, FieldValue* fval) const
void RecordDataAccessor::RetrieveFVPair
(FieldID* fid, FieldValue* gotvalue, bool get_blob_data, 
 FieldValue* separate_blob_value, int* ecount) const
{
	SingleDatabaseFileContext* sfc = record->HomeFileContext();
	DatabaseFile* file = record->HomeFileContext()->GetDBFile();
	DatabaseFileTableBManager* tbmgr = file->GetTableBMgr();

	//It is assumed that the cursor is positioned before calling this
	int recnum = data_cursor.RecNum();
	short recoffset = data_cursor.RecOffset();

	CachePageForRec(recnum);
	RecordDataPage pb(buffcache);

	//First get the table B value
	short slot = tbmgr->BPageSlotFromRecNum(recnum);
	pb.SlotGetFVPair(slot, recoffset, fid, gotvalue);

	//V3.0.  Then go to table E if necessary
	if (gotvalue && get_blob_data && gotvalue->IsBLOBDescriptor()) {

		DatabaseFileTableDManager* tdmgr = file->GetTableDMgr();
		DatabaseServices* dbapi = sfc->DBAPI();
		
		int epage = gotvalue->BLOBDescTableEPage();
		short eslot = gotvalue->BLOBDescTableESlot();		
		std::string blobstring;

		//Might be zero or several extents
		if (ecount)
			*ecount = 0;

		while (epage != -1) {
			BufferPageHandle bh = tdmgr->GetTableDPage(dbapi, epage);
			BLOBPage p(bh);

			p.GetBLOBExtentData(epage, eslot, blobstring);

			if (ecount)
				(*ecount)++;
		}

		//Caller may wish to retain separate value and descriptor
		if (separate_blob_value)
			*separate_blob_value = blobstring;
		else
			//The default behaviour is just return the value like a STRING field
			*gotvalue = blobstring;
	}
}

//*************************************************************************************
//void RecordDataAccessor::DeleteFVPair(bool during_change, bool uae, FieldID* nextfid) //V3.0.
void RecordDataAccessor::DeleteFVPair
(PhysicalFieldInfo* pfi, bool during_change, bool uae, FieldID* nextfid) 
{	
	SingleDatabaseFileContext* sfc = record->HomeFileContext();
	DatabaseFile* file = sfc->GetDBFile();
	DatabaseFileTableBManager* tbmgr = file->GetTableBMgr();
	DatabaseServices* dbapi = sfc->DBAPI();

	//It is assumed that the cursor is positioned before calling this
	int recnum = data_cursor.RecNum();
	short recoffset = data_cursor.RecOffset();

	CachePageForRec(recnum);
	RecordDataPage pb(buffcache);

	short slot = tbmgr->BPageSlotFromRecNum(recnum);

	//V3.0.  Before the field goes, if it's a BLOB descriptor use it to delete the table E data.
	if (pfi->atts.IsBLOB()) {
		FieldValue descriptor;
		pb.SlotGetFVPair(slot, recoffset, NULL, &descriptor);

		int epage = descriptor.BLOBDescTableEPage();
		short eslot = descriptor.BLOBDescTableESlot();

		file->GetTableDMgr()->DeleteBLOB(dbapi, epage, eslot);
	}

	//Now remove the table B item as normal
	bool extent_empty = pb.SlotDeleteFVPair(slot, recoffset, nextfid);

	//See tech doc for some discussion on the pros and cons of deleting empty extents.
	if (!extent_empty)
		return;				//this extent still contains data
	if (during_change) {
		if (!uae)
			return;			//update-in-place new value will be here soon
		if (pb.SlotGetExtensionPtr(slot) == -1)
			return;			//update-at-end new value will be here soon
	}
	if (record->RecNum() == recnum)
		return;				//this is the primary extent (always leave)

	//OK, get rid of the empty extent
	tbmgr->DeleteExtensionRecordExtent(dbapi, &pb, recnum, record->RecNum());
}

//*************************************************************************************
//V3.0.  Extra wrapper func to put the BLOB data in table E.  Use a separate function
//because the regular inserter for table B values may call itself (e.g. when fields
//have to be moved along to extensions).  The table E part does not need recursion.
int RecordDataAccessor::StoreBLOB
(
 DatabaseServices* dbapi,
 DatabaseFileTableDManager* tdmgr,
 const FieldValue& fullvalue,
 FieldValue& descriptor,
 const FieldValue** insval,
 bool& maybe_benign)
{
	//Note that this FieldValue object may just contain a short string value
	int blob_remaining = fullvalue.StrLen();
	const char* pblobdata = fullvalue.StrChars();

	//Even indeed zero length, in which case no table E data is required
	descriptor.MakeBLOBDescriptor();
	*insval = &descriptor;

	bool primary = true;
	int extents = 0;
	BufferPageHandle bhprev;
	short prev_eslot = -1;

	//Possible multiple extents
	while (blob_remaining) {

		int epage;
		short eslot;
		BufferPageHandle bh = tdmgr->StoreBLOBExtent
			(dbapi, &pblobdata, blob_remaining, epage, eslot, maybe_benign);

		//Table B will get the descriptor of the primary extent, plus the full BLOB length
		if (primary) {
			descriptor.MakeBLOBDescriptor(epage, eslot, fullvalue.StrLen());

			maybe_benign = false;
			primary = false;
		}

		//Subsequent extents' descriptors go on the previous extent
		else {
			BLOBPage pprev(bhprev);
			pprev.SlotSetExtensionPointers(prev_eslot, epage, eslot);
		}

		//Hang on to the database page for possible extension ptr next time round,
		//(quicker and also we don't want to clock up an extra DKPR).
		bhprev = bh;
		prev_eslot = eslot;

		extents++;
	}

	return extents;
}

//*************************************************************************************
void RecordDataAccessor::InsertFVPair_S(FieldID fid, bool blob, const FieldValue& fullvalue, 
	int recnum, short recoffset, int inserting_occ, bool maybe_benign) 
{
	FieldValue descriptor;
	const FieldValue* insval = &fullvalue;

	if (blob) {
		SingleDatabaseFileContext* sfc = record->HomeFileContext();
		StoreBLOB(sfc->DBAPI(), sfc->GetDBFile()->GetTableDMgr(), 
			fullvalue, descriptor, &insval, maybe_benign);
	}

	InsertFVPair_SRecurse(fid, *insval, recnum, recoffset, inserting_occ, maybe_benign);
}

//*************************************************************************************
void RecordDataAccessor::InsertFVPair_SRecurse(FieldID fid, const FieldValue& insval,
	int recnum, short recoffset, int inserting_occ, bool maybe_benign) 
{
	SingleDatabaseFileContext* sfc = record->HomeFileContext();
	DatabaseFile* file = sfc->GetDBFile();
	DatabaseFileTableBManager* tbmgr = file->GetTableBMgr();
	DatabaseServices* dbapi = sfc->DBAPI();

	CachePageForRec(recnum);

	//Take a local handle to the base page since recursion may happen on different pages
	BufferPageHandle hb = buffcache;
	RecordDataPage pb(hb);

	short slot = tbmgr->BPageSlotFromRecNum(recnum);

	//Shuffle as many fields along to later extensions as necessary
	for (;;) {

		//Try to insert the field
		if (pb.SlotInsertFVPair(slot, recoffset, fid, insval)) {
			data_cursor.SetOcc(fid, inserting_occ, recnum, recoffset);

			//NB. Ultimately this is the only way out of the function
			return;
		}

		//Not enough room.  Are there any fields after this we can move to an extension?
		short last_field_offset;
		bool anyfields = pb.SlotLocateLastAnyField(slot, last_field_offset);
		if (!anyfields || last_field_offset < recoffset)
			break; //none

		//Move fields to the next extent starting with the final one on this extent.
		int moveoffset = last_field_offset;
		FieldID movefid;
		FieldValue moveval;
		pb.SlotGetFVPair(slot, moveoffset, &movefid, &moveval);

		//Is there actually a next extent to move them to?
		int ern = pb.SlotGetExtensionPtr(slot);

		//No - create one
		if (ern == -1) {
			ern = tbmgr->AllocateExtensionRecordExtent(dbapi, maybe_benign, &moveval);
			pb.SlotSetExtensionPtr(slot, ern);

			//No going back now
			maybe_benign = false;
		}

		//Copy the moving field to its new location.  Note that this may cause
		//further spills - hence the recursive call.

		int irrelevant = 0; //only the cursor pos on final exit is relevant

		InsertFVPair_SRecurse(movefid, moveval, ern, 4, irrelevant, maybe_benign);
		maybe_benign = false;

		//Delete the old copy of the moved field, and hopefully now we'll have enough space
		pb.SlotDeleteFVPair(slot, moveoffset);
	}

	//Still no room even with this field as the last on the extent, so we're going 
	//to have to try and insert it at the start of the the next extent.
	int ern = pb.SlotGetExtensionPtr(slot);

	//As above this may necessitate creating the extension
	if (ern == -1) {
		ern = tbmgr->AllocateExtensionRecordExtent(dbapi, maybe_benign, &insval);
		pb.SlotSetExtensionPtr(slot, ern);
		maybe_benign = false;
	}

	//Here it definitely means a recursive call
	InsertFVPair_SRecurse(fid, insval, ern, 4, inserting_occ, maybe_benign);
}














//*************************************************************************************
//Interface functions
//*************************************************************************************
bool RecordDataAccessor::GetFieldValue
(PhysicalFieldInfo* pfi, const int required_occ, FieldValue& result, bool get_blob_data) const
{
	UnlockedRecordDeletionCheck(false);

	if (LocateFieldOccurrenceByNumber(pfi, required_occ)) {
		RetrieveFVPair(NULL, &result, get_blob_data);
		return true;
	}
	else {
		//This is the M204 convention for when the field is missing.  Could just
		//return "" here, and it would show as 0 if requested in numeric format.
		//But the data type of the result might get queried so might as well get it right.
		if (pfi->atts.IsFloat()) 
			result = 0.0; 
		else 
			result.AssignData("", 0);

		return false;
	}
}

//*************************************************************************************
int RecordDataAccessor::CountOccurrences(PhysicalFieldInfo* pfi) const
{
	UnlockedRecordDeletionCheck(false);

	//We are effectively doing exactly the same as a FEO loop here.
	int occs = 0;
	while (LocateFieldOccurrenceByNumber(pfi, occs+1))
		occs++;

	return occs;
}

//*************************************************************************************
//bool RecordDataAccessor::GetNextFVPair
//(FieldID& fid, FieldValue& fval, int& request_fvpix) const //V3
bool RecordDataAccessor::GetNextFVPair
(FieldID* fid, FieldValue* fval, FieldValue* blobdesc, int& request_fvpix) const
{
	UnlockedRecordDeletionCheck(false);

	if (request_fvpix < 0 || request_fvpix >= INT_MAX - 1)
		request_fvpix = 0;

	int delta = request_fvpix - pai_cursor_fvpix;

	if (delta < 0) {
		data_cursor.ResetPos();
		delta = request_fvpix + 1;
	}

	//Use of all the other functions clears this - only this func really uses it
//	int temp_cursor_fvpix = pai_cursor_fvpix;

	//Advance the desired number of fv pairs
	for (int x = 0; x < delta; x++) {
		if (!CursorAdvanceFieldOccurrence(NULL)) {

			//V3.0.  These may come in as null now for a "dry" iterate
			//fid = -1;
			//fval.AssignData("", 0);
			if (fid) 
				*fid = -1;
			if (fval) 
				fval->AssignData("", 0);

			request_fvpix = 0; //next call will start again
			return false;
		}
	}

	//OK we have the desired pair

	//V3.0.  With the LOB options in PAI we may want both, one, or neither of the table B/E values
	//RetrieveFVPair(&fid, &fval);
	if (blobdesc) {
		if (fval) {
			RetrieveFVPair(fid, blobdesc, true, fval); //both, and keep separate

			//For non-blobs, populate value with descriptor
			if (!blobdesc->IsBLOBDescriptor())
				*fval = *blobdesc;
		}
		else {
			RetrieveFVPair(fid, blobdesc, false);      //just descriptor or nonblob value, don't access table E
		}
	}
	else {
		if (fval) {
			RetrieveFVPair(fid, fval, true);           //just value, go to table E if required
		}
		else {
			;                                          //neither
		}
	}

	//Save this for next time
	pai_cursor_fvpix = request_fvpix;
	request_fvpix++;

	return true;
}

//*************************************************************************************
void RecordDataAccessor::CopyAllInformation(RecordCopy& result) const
{
	UnlockedRecordDeletionCheck(false);

	result.Clear();
	result.SetRecNum(data_cursor.RecNum());

	FieldID fid;
	FieldValue fval;
	int fvpix = 0;
	
	//while (GetNextFVPair(fid, fval, fvpix)) { //V3.0
	while (GetNextFVPair(&fid, &fval, NULL, fvpix)) {
		SingleDatabaseFileContext* home = record->HomeFileContext();
		PhysicalFieldInfo* pfi = 
			home->GetDBFile()->GetFieldMgr()->GetPhysicalFieldInfo(home, fid);

		result.Append(pfi->name, fval);
	}
}

//*************************************************************************************
int RecordDataAccessor::InsertField(PhysicalFieldInfo* pfi, 
 const FieldValue& insval, int occ, bool* ix_reqd, bool storing) 
{
	UnlockedRecordDeletionCheck(true);

	int occ_inserted;

	//See tech docs for notes about this special occ 1 processing.  Mainly for TBO, so
	//a TBO-only flag to this func'll be OK if anyone dislikes the M204 incompatibility.
	if (occ == 1) {

		//Insert at the very beginning of the record (known to start at offset 4)
		occ_inserted = 1;
		data_cursor.SetOcc(pfi->id, 1, record->RecNum(), 4);
	}
	else {

		//V2.14 Feb 09.  When storing a record we will always be appending and can avoid 
		//some scanning especially if the records have lots of fields.  This function
		//is getting a bit ugly now, but the assorted shared processing for INSERT and ADD 
		//is for M204 compatibility reasons and I don't want to decouple them and then 
		//have lots of duplicate code.
		//V2.15.  Slightly rushed version, so not fixed, just commented out for now.
		//Seems to store multi-occ fields in wrong order, but not always.  Possibly to
		//do with there being other fields before the MO group on the record. See RM email.
//		if (storing) {
//
//			//First field on record
//			if (data_cursor.Occ() == 0) {
//				occ_inserted = 1;
//				data_cursor.SetOcc(pfi->id, 1, record->RecNum(), 4);
//			}
//
//			//For subsequent fields we are already positioned at the previous one
//			else {
//				CursorAdvanceFieldOccurrence(pfi);
//				data_cursor.MutateOcc(1); //just to distinguish from 1st field
//			}
//		}
//
//		//Regular insert, or store in a non-load setting
//		else {
			//Insertion will be before any current occ=N, or at the end of the record
			if (!LocateFieldOccurrenceByNumber(pfi, occ, &occ_inserted))
				occ_inserted++; //at end - so current highest + 1
//		}
	}

	int recnum = data_cursor.RecNum();
	short recoffset = data_cursor.RecOffset();

	InsertFVPair_S(pfi->id, pfi->atts.IsBLOB(), insval, recnum, recoffset, occ_inserted, true);

	//------------------------
	//Index update preparation
	if (ix_reqd) {
		//This is an opportunity for future tuning really.  We can't
			//go wrong if we just do the full index work.  At the end of the day how
			//often do you get a duplicate f=v on a record.  Not at all often.
		//There are some tech notes somewhere about possibilities for ways it could
			//be done if someone wants to do it later.
		//See also change and delete below, where the scan here is essential.
		*ix_reqd = true;
	}

	//Both ADD and INSERT in UL come through here
	record->HomeFileContext()->GetDBFile()->IncStatBADD(record->HomeFileContext()->DBAPI());
	return occ_inserted;
}

//*************************************************************************************
int RecordDataAccessor::ChangeField(bool by_value, PhysicalFieldInfo* pfi, 
 const FieldValue& newval, int occ_requested, const FieldValue* value_requested, 
 int* pocc_added, FieldValue* note_old_value, bool* ix_reqd_newval, bool* ix_reqd_oldval)
{
	UnlockedRecordDeletionCheck(true);

	bool update_at_end = pfi->atts.IsUpdateAtEnd();

	int occ_located;
	int occ_deleted;
	int occ_added;
	bool same_value;
	bool found;

	if (by_value)
		found = LocateFieldOccurrenceByValue(pfi, *value_requested, &occ_located);
	else
		found = LocateFieldOccurrenceByNumber(pfi, occ_requested, &occ_located);

	//Change works just like ADD if there aren't enough occs
	if (!found) {
		occ_deleted = -1;
		occ_added = occ_located + 1;
		same_value = false;
	}
	else {
		occ_deleted = occ_located;

		//The old value is used for TBO and can also be nice for the user
		if (!by_value)
			RetrieveFVPair(NULL, note_old_value);

		same_value = (newval == *note_old_value);

		//Minor performance tweak - only actually change if it's a different value (or UAE)
		if (!same_value || update_at_end) {

			DeleteFVPair(pfi, true, update_at_end);

			//For UAE fields, now move to the end of the record.  Note that we need to go
			//there an occurrence at a time so that we can know which occurrence to delete
			//should TBO be required.
			if (update_at_end) {
				//Also note that whilst it would in theory be doable to scan from the 
				//position on the record where we just deleted the old value, the 
				//complications of de-extension and/or exhaustion of the fields on extents
				//mean it's much simpler to start again.
				data_cursor.ResetPos();
				LocateFieldOccurrenceByNumber(pfi, INT_MAX, &occ_located);
			}
		}

		if (update_at_end)
			occ_added = occ_located + 1;
		else
			occ_added = occ_deleted;
	}

	//Used for TBO and also can be interesting for the caller
	if (pocc_added)
		*pocc_added = occ_added;

	//------------------------
	//Now put the new value in (same shortcut as above)
	if (!same_value || update_at_end) {
		InsertFVPair_S(pfi->id, pfi->atts.IsBLOB(), newval, 
			data_cursor.RecNum(), data_cursor.RecOffset(), occ_added, false);
	}

	//------------------------
	//Index update preparation
	if (ix_reqd_newval) {

		//Obviously
		if (same_value) {
			*ix_reqd_oldval = false;
			*ix_reqd_newval = false;
		}
		else {

			//With the new val it'd be tuning to avoid this (like insert - see above)
			*ix_reqd_newval = true;

			//With the old val it's essential to know of dupes (like delete - see below)
			if (occ_deleted == -1)
				*ix_reqd_oldval = false;
			else
				*ix_reqd_oldval = IxPrepFVPairNowMissing(pfi, note_old_value);
		}
	}

	//Decided to increment this even if it's the same value
	record->HomeFileContext()->GetDBFile()->IncStatBCHG(record->HomeFileContext()->DBAPI());
	return occ_deleted;
}

//*************************************************************************************
int RecordDataAccessor::DeleteField(bool by_value, PhysicalFieldInfo* pfi, 
 const int occ_requested, const FieldValue* value_requested, 
 FieldValue* note_old_value, bool* ix_reqd)
{
	UnlockedRecordDeletionCheck(true);

	int occ_located;
	bool found;
	
	if (by_value)
		found = LocateFieldOccurrenceByValue(pfi, *value_requested, &occ_located);
	else
		found = LocateFieldOccurrenceByNumber(pfi, occ_requested, &occ_located);

	if (!found)
		return -1;

	//The old value is used for TBO and can also be interesting for the user
	if (!by_value)
		RetrieveFVPair(NULL, note_old_value);

	DeleteFVPair(pfi);
	data_cursor.ResetPos();

	//Index update preparation
	//Note that unlike insert, it is essential for the correct operation of the DBMS
	//that we know whether a duplicate FV pair exists on the record, otherwise we
	//will erroneously remove the index entry.  See tech notes for ideas if desired.
	if (ix_reqd)
		*ix_reqd = IxPrepFVPairNowMissing(pfi, note_old_value);

	record->HomeFileContext()->GetDBFile()->IncStatBDEL(record->HomeFileContext()->DBAPI());
	return occ_located;
}

//*************************************************************************************
bool RecordDataAccessor::IxPrepFVPairNowMissing(PhysicalFieldInfo* pfi, const FieldValue* val)
{
	//We might have just deleted e.g. occ 2 so we would miss occ 1 if no restart
	data_cursor.ResetPos();

	int dummy;
	return !LocateFieldOccurrenceByValue(pfi, *val, &dummy);
}	



//*************************************************************************************
//V2.19 June 09.  For the DELETE FIELD command, and REDEFINE to invisible
void RecordDataAccessor::QuickDeleteEachOccurrence(PhysicalFieldInfo* pfi)
{
	for (;;) {

		//Locate first/next occ of the target field
		if (!CursorAdvanceFieldOccurrence(pfi))
			return;

		//While we're positioned at an occ of the field, delete it
		for (;;) {
			FieldID nextfid = -1;

			DeleteFVPair(pfi, false, false, &nextfid);

			//Already positioned at the next occ of the same field?
			if (nextfid == pfi->id)
				continue;

			//A couple of times we accept the overhead of rescanning from the first extent:
			//1. When an extent is deleted, maintaining a position would be tricky.
			//2. When the topmost field on a non-empty extent is deleted, we could chain 
			//   here, but it's much neater to just reset and let CursorAdvance...() do it.
			if (nextfid == -1)
				data_cursor.ResetPos();

			break;
		}
	}
}

//*************************************************************************************
//V2.19 June 09.  For REDEFINE from FLOAT to STRING or vice versa
//void RecordDataAccessor::QuickChangeEachOccurrenceFormat(PhysicalFieldInfo* oldpfi) //V3.0
void RecordDataAccessor::QuickChangeEachOccurrenceFormat(PhysicalFieldInfo* oldpfi, bool convert_numtype)
{
	//This is the only type of REDEFINE where a failed conversion to numeric 
	//could have no knock-on effects anywhere, so we can optionally allow it.
	//See also comment below about not using earlier standard code for this.
	bool throw_badnums = (record->HomeFileContext()->DBAPI()->GetParmFMODLDPT() & 1);
	throw_badnums |= oldpfi->atts.IsOrdered(); //index effects so not allowed

	for (;;) {

		//Locate first/next occ of the target field
		if (!CursorAdvanceFieldOccurrence(oldpfi))
			return;

		//Get old value
		FieldValue fval;
		RetrieveFVPair(NULL, &fval);

		//Attempt conversion to the new format.  NB during regular STORE/ADD etc. we
		//would call DatabaseFileFieldManager::ConvertValue() to get a more detailed
		//message and/or synchronize data/index work, but that function is overkill here.		
		//V3.0.  We might now be going between STRING and BLOB which requires no value conversion
		bool toblob = false;
		if (convert_numtype) {
			if (oldpfi->atts.IsFloat())
				fval.ConvertToString();
			else
				fval.ConvertToNumeric(throw_badnums);
		}
		else {
			if (oldpfi->atts.IsBLOB())
				fval.CheckStrLen255();
			else
				toblob = true;
		}

		//Delete old value as per "update in place" - no de-extension.  
		DeleteFVPair(oldpfi, true, false);

		//Reinsert new value.  Could do with del but we have funcs for both and it's not critical.
		InsertFVPair_S(oldpfi->id, toblob, fval, 
			data_cursor.RecNum(), data_cursor.RecOffset(), data_cursor.Occ(), false);
	}
}

//*************************************************************************************
//V2.19 June 09.  For REDEFINE from INVISIBLE to FLOAT or STRING
void RecordDataAccessor::AppendVisiblizedValue
(PhysicalFieldInfo* pfi, const FieldValue& val)
{
	//Just like during ADD, but skipping the stats and other irrelevancies
	LocateFieldOccurrenceByNumber(pfi, INT_MAX);
	
	InsertFVPair_S(pfi->id, pfi->atts.IsBLOB(), val, 
		data_cursor.RecNum(), data_cursor.RecOffset(), INT_MAX, false);
}

//*************************************************************************************
//July 09, in prep for V3.0. See comment in SlotAndRecordPage::SlotGetPageDataPointer.
void RecordDataAccessor::GetPageDataPointer
(int* extent_recnum, const char** pbuff, short* plen)
{
	CachePageForRec(*extent_recnum);
	RecordDataPage pb(buffcache);

	short slotnum = record->HomeFileContext()
		->GetDBFile()->GetTableBMgr()->BPageSlotFromRecNum(*extent_recnum);

	pb.SlotGetPageDataPointer(slotnum, pbuff, plen);

	//The next extent pointer is the first 4 bytes
	*extent_recnum = *(reinterpret_cast<const int*>(*pbuff));

	//The FV pair data after that is what we return a pointer to
	*pbuff += 4;
	*plen -= 4;
}







//*************************************************************************************
std::string RecordDataAccessor::ShowPhysicalInformation()
{
	UnlockedRecordDeletionCheck(false);

	SingleDatabaseFileContext* ctxt = record->HomeFileContext();
	DatabaseFile* file = ctxt->GetDBFile();

	int numpairs_total = 0;
	int numpairs_string = 0;
	int numpairs_float = 0;
	int reclen = 0;
	int recnum = record->RecNum();
	int numextents = 1;

	int numpairs_blob = 0;
	int num_blob_extents = 0;
	int blob_total_len = 0;

	data_cursor.ResetPos();
	std::string extents("#");
	extents.append(util::IntToString(recnum));

	std::string result("File ");
	result.append(file->FileName(ctxt))
		.append(", Record ")
		.append(extents)
		.append(": ");

	FieldID fid;
	FieldValue fval;

	while (CursorAdvanceFieldOccurrence(NULL)) {
		numpairs_total++;

		//Get some field info
		int ecount = -1;
		RetrieveFVPair(&fid, &fval, true, NULL, &ecount);

		if (fval.CurrentlyNumeric()) {
			numpairs_float++;
			reclen += 10;
		}
		//V3.0. BLOBs
		else if (ecount >= 0) {
			numpairs_blob++;
			num_blob_extents += ecount;
			reclen += (3 + FV_BLOBDESC_LEN);
			blob_total_len += (fval.StrLen() + (ecount * PAGE_L_EXTPTR_LEN));
		}
		else {
			numpairs_string++;
			reclen += (3 + fval.StrLen());
		}

		//Table B extent info
		if (data_cursor.RecNum() != recnum) {
			numextents++;
			recnum = data_cursor.RecNum();
			extents.append(",#").append(util::IntToString(recnum));
		}
	}

	result.append(util::IntToString(numpairs_total));
	result.append(" FV pairs (");

	if (numpairs_float) {
		result.append(util::IntToString(numpairs_float));
		result.append("f ");
	}
	if (numpairs_string) {
		result.append(util::IntToString(numpairs_string));
		result.append("s ");
	}
	//V3.0
	if (numpairs_blob) {
		result.append(util::IntToString(numpairs_blob));
		result.append("b ");
	}
	
	util::DeBlank(result);

	result.append(") on ")
		.append(util::IntToString(numextents))
		.append(" table B extents: ")
		.append(extents);

	result.append(", bytes=")
		.append(util::IntToString(reclen + 4 * numextents));


	//V3.0
	if (num_blob_extents) {
		result.append(", and ")
			.append(util::IntToString(num_blob_extents))
			.append(" table E extents, bytes=")
			.append(util::IntToString(blob_total_len));
	}

	return result;
}


} //close namespace


