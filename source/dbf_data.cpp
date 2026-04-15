
#include "stdafx.h"

#include "dbf_data.h"

//Utils
#include "dataconv.h"
#include "charconv.h"
#include "lineio.h"
#include "handles.h"
#include "winutil.h"
//API tiers
#include "cfr.h" //#include "CFR.h" : V2.24 case is less interchangeable on *NIX - Roger M.
#include "update.h"
#include "btree.h"
#include "foundset.h"
#include "recdata.h"
#include "record.h"
#include "dbctxt.h"
#include "du1step.h"
#include "dbf_ebm.h"
#include "dbf_field.h"
#include "dbf_tableb.h"
#include "dbf_tabled.h"
#include "loaddiag.h"
#include "fastload.h"
#include "fastunload.h"
#include "page_b.h" //#include "page_B.h" : V2.24 case is less interchangeable on *NIX - Roger M.
#include "page_f.h" //#include "page_F.h"
#include "page_l.h" //#include "page_F.h"
#include "dbserv.h"
#include "core.h"
#include "msgroute.h"
//Diagnostics
#include "except.h"
#include "msg_db.h"

//For eyeball mode
#ifdef _BBHOST
#include "iodev.h"
#endif

namespace dpt {

//**********************************************************************************************
DatabaseFileDataManager::DatabaseFileDataManager(DatabaseFile* f) 
: file(f)
{
	std::pair<int, RecordDataAccessor*> p;
	p.first = -1;
	p.second = NULL;

	record_mros.resize(100, p);
	record_mros_hwm = -1;
}

//**********************************************************************************************
void DatabaseFileDataManager::RegisterRecordMRO(RecordDataAccessor* da) 
{
	LockingSentry ls(&record_mro_lock);

	int rn = da->record->RecNum();

	//Look for a free slot (recnum = -1)
	size_t x;
	for (x = 0; x < record_mros.size(); x++) {
		if (record_mros[x].first == -1)
			break;
	}

	//Otherwise increase the size of the array
	if (x == record_mros.size()) {
		std::pair<int, RecordDataAccessor*> p;
		p.first = -1;
		p.second = NULL;

		int newsize = (record_mros.size() == 0) ? 1 : record_mros.size() * 2;
		record_mros.resize(newsize, p);
	}

	record_mros[x] = std::make_pair<int, RecordDataAccessor*>(rn, da);

	if ((int)x > record_mros_hwm)
		record_mros_hwm = x;
}

//**********************************************************************************************
void DatabaseFileDataManager::DeregisterRecordMRO(RecordDataAccessor* da) 
{
	LockingSentry ls(&record_mro_lock);

	int rn = da->record->RecNum();

	for (int x = 0; x <= record_mros_hwm; x++) {
		std::pair<int, RecordDataAccessor*>& p = record_mros[x];

		if (p.first == rn && p.second == da) {
			p.first = -1;
			p.second = NULL;
			return;
		}
	}

	throw Exception(DB_MRO_MGMT_BUG, "Bug: Missing registered record MRO pointer");
}

//**********************************************************************************************
//This is the whole point of keeping the set of record MROs.  If thread A has one on an
//unlocked set and thread B updates the same record, thread A can start its scan again
//when it returns for e.g. the next iteration of an occurrence loop.  Note also that
//whether or not the current thread has the record locked is of no use as a way of avoiding
//having to do this, since you can have several MROs open for the same record on the same
//thread, and a thread can not lock itself out.
//Note also for future programming reference that the RecordAccessor algorithms in one or 
//two places assume that the following functions get called and flag the SAME record too.
//In other words any future attempt to skip the registering of record MROs here for 
//performance reasons, for example to implement some kind of fast single-user mode, will 
//have to address this issue.  The RecordAccessor code works well but is quite fiddly and 
//I don't fancy touching it right now.
//See also TBO comment on this in DeInsertFieldData::Perform below.
//**********************************************************************************************
void DatabaseFileDataManager::FlagUpdatedRecordMROs(RecordDataAccessor* da, bool del) 
{
	LockingSentry ls(&record_mro_lock);

	int rn = da->record->RecNum();

	//Flag all MROs that are open on the same record
	for (int x = 0; x <= record_mros_hwm; x++) {
		std::pair<int, RecordDataAccessor*>& p = record_mros[x];

		if (p.first == rn)
			p.second->ForceRescan(da, del);
	}
}

//**********************************************************************************************
//Similar to the above, if thread B deletes the record it's even more serious for thread A.
//**********************************************************************************************
void DatabaseFileDataManager::FlagDeletedRecordMROs(RecordDataAccessor* deleter) 
{
	Record* deleter_record = deleter->record;
	DatabaseServices* deleter_thread = deleter_record->HomeFileContext()->DBAPI();

	LockingSentry ls(&record_mro_lock);

	int rn = deleter_record->RecNum();

	//Flag all MROs that are open on the same record
	for (int x = 0; x <= record_mros_hwm; x++) {
		std::pair<int, RecordDataAccessor*>& p = record_mros[x];

		if (p.first == rn)
			p.second->MarkDeleted(deleter_thread);
	}
}






//*************************************************************************************
//Atomic updates all under the protection of DIRECT
//*************************************************************************************
int DatabaseFileDataManager::Atom_StoreEmptyRecord(DatabaseServices* dbapi, int tborecnum) 
{
	CFRSentry s(dbapi, file->cfr_direct, BOOL_EXCL);
	int newrecnum;

	if (tborecnum == -1)
		newrecnum = file->GetTableBMgr()->AllocatePrimaryRecordExtent(dbapi, true);

	//During TBO of delete the recnum must be the same as the one deleted, so that
	//its invisible indexes will still point to it correctly.
	else {
		newrecnum = tborecnum;
		file->GetTableBMgr()->RestoreDeletedPrimaryExtent(dbapi, tborecnum);
	}

	file->IncStatRECADD(dbapi);

	return newrecnum;
}

//*************************************************************************************
void DatabaseFileDataManager::Atom_DeleteEmptyRecord(Record* record) 
{
	DatabaseServices* dbapi = record->HomeFileContext()->DBAPI();
	int recnum = record->RecNum();

	CFRSentry s(dbapi, file->cfr_direct, BOOL_EXCL);

	record->data_access.UnlockedRecordDeletionCheck(true);

	try {
		file->GetTableBMgr()->DeletePrimaryExtent(dbapi, recnum);
	}
	catch (Exception& e) {
		if (e.Code() == DML_NONEXISTENT_RECORD)
			record->ThrowNonexistent();
		throw;
	}

	file->IncStatRECDEL(dbapi);

	//Other MROs on the same record are in trouble now
	FlagDeletedRecordMROs(&record->data_access);
}

//*************************************************************************************
bool DatabaseFileDataManager::ReadFieldValue
(const Record* record, PhysicalFieldInfo* pfi, int occ, FieldValue& retval, bool get_blob_data)
{
	CFRSentry s(record->HomeFileContext()->DBAPI(), file->cfr_direct, BOOL_SHR);
	return record->data_access.GetFieldValue(pfi, occ, retval, get_blob_data);
}

//*************************************************************************************
bool DatabaseFileDataManager::GetNextFVPair
(const Record* record, FieldID* fid, FieldValue* fval, FieldValue* blobdesc, int& fvpix)
//(const Record* record, FieldID& fid, FieldValue& fval, int& fvpix) //V3.0
{
	CFRSentry s(record->HomeFileContext()->DBAPI(), file->cfr_direct, BOOL_SHR);
	return record->data_access.GetNextFVPair(fid, fval, blobdesc, fvpix);
}

//*************************************************************************************
void DatabaseFileDataManager::CopyAllInformation(const Record* record, RecordCopy& result)
{
	CFRSentry s(record->HomeFileContext()->DBAPI(), file->cfr_direct, BOOL_SHR);
	record->data_access.CopyAllInformation(result);
}

//*************************************************************************************
int DatabaseFileDataManager::CountOccurrences
(const Record* record, PhysicalFieldInfo* pfi)
{
	CFRSentry s(record->HomeFileContext()->DBAPI(), file->cfr_direct, BOOL_SHR);
	return record->data_access.CountOccurrences(pfi);
}

//*************************************************************************************
int DatabaseFileDataManager::Atom_InsertField(Record* record, PhysicalFieldInfo* pfi, 
 const FieldValue& val, int occ, bool* ix_reqd, bool du_mode) 
{
	//V2.03: Skip some stuff in the deferred updates load situation
	CFRSentry s;
	if (!du_mode)
		s.Get(record->HomeFileContext()->DBAPI(), file->cfr_direct, BOOL_EXCL);

	//V2.14 Feb 09.  New flag into here to avoid repeated full record scans during a load.
	int occ_inserted = record->data_access.InsertField(pfi, val, occ, ix_reqd, du_mode);

	if (!du_mode)
		FlagUpdatedRecordMROs(&record->data_access, false);

	return occ_inserted;
}

//*************************************************************************************
//Change
int DatabaseFileDataManager::Atom_ChangeField
(bool by_value, Record* record, PhysicalFieldInfo* pfi, 
 const FieldValue& newval, const int occ, const FieldValue* oldval, 
 int* pocc_added, FieldValue* poldval, bool* ix_reqd_newval, bool* ix_reqd_oldval) 
{
	CFRSentry s(record->HomeFileContext()->DBAPI(), file->cfr_direct, BOOL_EXCL);

	int occ_deleted = record->data_access.ChangeField(
		by_value, pfi, newval, occ, oldval, 
		pocc_added, poldval, ix_reqd_newval, ix_reqd_oldval);

	FlagUpdatedRecordMROs(&record->data_access, false);
	return occ_deleted;
}

//*************************************************************************************
int DatabaseFileDataManager::Atom_DeleteField
(bool by_value, Record* record, PhysicalFieldInfo* fieldinfo, 
 const int occ, const FieldValue* oldval, FieldValue* poldval, bool* ix_reqd) 
{
	CFRSentry s(record->HomeFileContext()->DBAPI(), file->cfr_direct, BOOL_EXCL);

	int occ_deleted = record->data_access.DeleteField
		(by_value, fieldinfo, occ, oldval, poldval, ix_reqd);

	FlagUpdatedRecordMROs(&record->data_access, true);
	return occ_deleted;
}

//*************************************************************************************
std::string DatabaseFileDataManager::ShowPhysicalInformation(Record* record)
{
	CFRSentry s(record->HomeFileContext()->DBAPI(), file->cfr_direct, BOOL_SHR);
	return record->data_access.ShowPhysicalInformation();
}








//*************************************************************************************
//V2.19 June 09.  For the DELETE FIELD command, and REDEFINE to invisible
void DatabaseFileDataManager::DeleteFieldFromEveryRecord
(SingleDatabaseFileContext* sfc, PhysicalFieldInfo* pfi)
{
	RecordSetHandle h(((DatabaseFileContext*)sfc)->FindRecords());

	//Just a basic for each record loop
	for (RecordSetCursorHandle cursor(h.GetSet()); cursor.Accessible(); cursor.Advance(1)) {

		Record* record = cursor.GetLiveCursor()->AccessCurrentRealRecord_B(true);

		//No need to worry about TBO or associated index work on a field-by-field
		//basis (compared to a regular DeleteEachOccurrence).
		record->data_access.QuickDeleteEachOccurrence(pfi);
	}
}

//*************************************************************************************
//V2.19 June 09.  For REDEFINE from FLOAT to STRING or vice versa
void DatabaseFileDataManager::ChangeFieldFormatOnEveryRecord
(SingleDatabaseFileContext* sfc, PhysicalFieldInfo* pfi, bool convert_numtype)
{
	RecordSetHandle h(((DatabaseFileContext*)sfc)->FindRecords());

	//A straightforward record loop, as per the above function
	for (RecordSetCursorHandle cursor(h.GetSet()); cursor.Accessible(); cursor.Advance(1)) {
		Record* record = cursor.GetLiveCursor()->AccessCurrentRealRecord_B(true);
		record->data_access.QuickChangeEachOccurrenceFormat(pfi, convert_numtype);
	}
}

//*************************************************************************************
//V2.19 June 09.  For REDEFINE from INVISIBLE to FLOAT or STRING
void DatabaseFileDataManager::VisiblizeFieldFromIndex
(SingleDatabaseFileContext* sfc, PhysicalFieldInfo* pfi, bool store_float)
{
	DatabaseServices* dbapi = sfc->DBAPI();
	DatabaseFileEBMManager* ebmmgr = sfc->GetDBFile()->GetEBMMgr();

	//The full DVC rigmarole is undesirable here
	BTreeAPI bt(sfc, pfi);	
	for (bt.LocateLowestValueEntryPreBrowse();
			bt.LastLocateSuccessful();
			bt.WalkToNextValueEntry()) 
	{
		//Here's the value
		FieldValue val;
		bt.GetLastValueLocated(val);
		if (store_float)
			val.ConvertToNumeric(true); //it *must* convert
		else
			val.ConvertToString();

		//No need to crank up the find engine - the inverted list is right here
		FoundSet* valset = sfc->CreateEmptyFoundSet();
		RecordSetHandle h(valset);
		BitMappedFileRecordSet* fileset = bt.InvertedList().AssembleRecordSet(NULL);
		fileset = ebmmgr->MaskOffNonexistentInSet(dbapi, fileset);
		valset->AppendFileSet(0, fileset);

		//Append value to every record that currently "has" it invisible
		for (RecordSetCursorHandle cursor(valset); cursor.Accessible(); cursor.Advance(1)) {

			Record* record = cursor.GetLiveCursor()->AccessCurrentRealRecord_B(true);

			record->data_access.AppendVisiblizedValue(pfi, val);
		}
	}	
}









//*************************************************************************************
void FastUnloadDataParallelSubTask::Perform()
{
	request->Context()->GetDBFile()->GetDataMgr()->Unload(request);
}

//*************************************************************************************
void DatabaseFileDataManager::Unload(FastUnloadRequest* request)
{
	SingleDatabaseFileContext* context = request->Context();
	DatabaseServices* dbapi = context->DBAPI();
	LoadDiagnostics* diags = request->Diags();
	DatabaseFileTableDManager* tdmgr = file->GetTableDMgr();

	FastUnloadOutputFile* taped = request->TapeD();
	taped->Initialize(request->Context(), "data records");

	diags->AuditMed("Starting data unload");

	int totrecs = 0;
	int totextents = 0;
	int totblobs = 0;
	int totblobextents = 0;
	int fvpinc = 0;
	int fvpexc = 0;
	double stime = win::GetFTimeWithMillisecs();

	bool chunk_xfer = (request->AllFields() && 
						!request->AnyDataReformatOptions() &&
						!request->AnyBLOBs());

	bool nofloat_option = request->NofloatOption();
	bool fnames_option = request->FnamesOption();
	bool pai_mode = request->PaiMode();
	bool crlf_option = request->CrlfOption();
	const std::vector<PhysicalFieldInfo*>& fieldinfos = request->FieldInfos();

	//Record loop
	RecordSetHandle h(context->FindRecords(NULL, FD_LOCK_SHR, request->BaseSet()));

	for (RecordSetCursorHandle cursor(h.GetSet()); cursor.Accessible(); cursor.Advance(1)) {
		Record* record = cursor.GetLiveCursor()->AccessCurrentRealRecord_B(true);

		totrecs++;
		totextents++; //primary counts even if empty

		//Output record number
		int primary_recnum = record->RecNum();
		if (pai_mode)
			taped->AppendTextLine(util::IntToString(primary_recnum));
		else
			taped->AppendBinaryInt32(primary_recnum);

		const char* recdata;
		short reclen;
		int extent_recnum = primary_recnum;

		//For each extent of the record
		while (extent_recnum != -1) {
			bool this_is_an_extension = (extent_recnum != primary_recnum);

			//We can skip a fair amount of repeated page-access admin by reading the page
			//directly here.  Faster than going via 2 intermediate layers of code full of 
			//checks and balances for TBO, enqueing etc.
			record->data_access.GetPageDataPointer(&extent_recnum, &recdata, &reclen);

			//No need to output anything for empty extents
			if (reclen == 0)
				continue;

			if (this_is_an_extension)
				totextents++;

			//Sometimes FV pairs can be done en masse straight off the table B page.
			if (chunk_xfer) {

				//This will speed up I/O in a later reload of this same data (common case).
				taped->AppendBinaryInt16(reclen | 0x8000);

				//Note that the field IDs here retain their "numeric" x'1000' bit.  ATOW
				//I'm not complicating the doc with this, just recommending FNAMES option.
				taped->AppendRawData(recdata, reclen);

				continue;
			}

			//But we have to go one by one to do any field selection or data reformatting.
			while (reclen) {
				FieldID pagefid = *(reinterpret_cast<const FieldID*>(recdata));
				recdata += 2;
				reclen -= 2;

				bool is_float = RecordDataPage::RealFieldIsFloat(pagefid);
				bool is_blob = RecordDataPage::RealFieldIsBLOB(pagefid);

				FieldID realfid = RecordDataPage::RealFieldCode(pagefid);
				PhysicalFieldInfo* pfi = fieldinfos[realfid];

				unsigned _int8 slen;
				short vallen;
				FieldValue blobdesc;

				if (is_float)
					vallen = 8;
				else if (is_blob) {
					totblobs++;
					blobdesc.MakeBLOBDescriptor(recdata+1);
					vallen = FV_BLOBDESC_LEN + 1;
				}
				else {
					slen = *(reinterpret_cast<const unsigned _int8*>(recdata));
					vallen = slen + 1;
				}

				//Select or skip the field as required
				if (!pfi->extra)
					fvpexc++;

				else {
					fvpinc++;

					//Output field name/code
					if (pai_mode) {
						taped->AppendString(pfi->name, false);

						//Alternate PAI format for BLOBs
						if (is_blob) {
							taped->AppendString(" =", 2, false);
							taped->AppendString(util::IntToString(blobdesc.BLOBDescLen()), false);
							taped->AppendString("=", 1, false);
						}
						else {
							taped->AppendString(" = ", 3, false);
						}
					}
					else {
						if (fnames_option)
							taped->AppendString(pfi->name, true);

						//FID here is inconsistent with the block xfer case above in that
						//the numeric flag is not on it here.  No prob for now.  Maybe ever.
						else
							taped->AppendBinaryInt16(pfi->id);
					}

					//Output field value
					if (is_float) {
						if (pai_mode || nofloat_option) {
							double d = *(reinterpret_cast<const double*>(recdata));
							std::string fval = util::PlainDoubleToString(d);
							taped->AppendString(fval.c_str(), fval.length(), !pai_mode);
						}
						else {
							taped->AppendBinaryDouble(*(double*)recdata);
						}
					}
					else if (is_blob) {
						int   epage   = blobdesc.BLOBDescTableEPage();
						short eslot   = blobdesc.BLOBDescTableESlot();		

						if (!pai_mode)
							taped->AppendBinaryInt32(blobdesc.BLOBDescLen());

						while (epage != -1) {
							totblobextents++;

							BufferPageHandle bh = tdmgr->GetTableDPage(dbapi, epage);
							BLOBPage p(bh);

							const char* blob_extent_data;
							short blob_extent_len;
							p.GetBLOBExtentData(epage, eslot, &blob_extent_data, &blob_extent_len);

							taped->AppendString(blob_extent_data, blob_extent_len, false);
						}
					}
					else {
						taped->AppendString(recdata+1, slen, !pai_mode);
					}

					if (pai_mode)
						taped->AppendCRLF();
				}

				recdata += vallen;
				reclen -= vallen;

			} //Each FVP on extent

		} //Each extent

		//EOR markers.  xFFFF is invalid both as a field code and as (length byte +
		//first byte of field name).  Field names must begin with a letter.
		if (!pai_mode)
			taped->AppendBinaryUint16(0xFFFF);

		if (crlf_option || pai_mode)
			taped->AppendCRLF();
	
	} //Each record

	taped->Finalize();

	double etime = win::GetFTimeWithMillisecs();
	double elapsed = etime - stime;

	diags->AuditMed(std::string("  Data unload complete (")
		.append(util::IntToString(totrecs)).append(" records on ")
		.append(util::IntToString(totextents)).append(" extents). Elapsed: ")
		.append(RoundedDouble(elapsed).ToStringWithFixedDP(1)).append("s"));

	if (chunk_xfer)
		diags->AuditHigh("  Data: record level I/O was used");
	else {
		std::string msg = std::string("  Data: field level I/O was used - Excluded: ")
			.append(util::IntToString(fvpexc)).append(", Included: ")
			.append(util::IntToString(fvpinc));
		if (totblobs) {
			msg.append(", of which BLOBs: ").append(util::IntToString(totblobs))
			.append(", on table E extents: ").append(util::IntToString(totblobextents));
		}
		diags->AuditHigh(msg);
	}
}

//*************************************************************************************
int DatabaseFileDataManager::Load(FastLoadRequest* request, BB_OPDEVICE* eyedest)
{
	SingleDatabaseFileContext* context = request->Context();
	DatabaseFileTableBManager* tbmgr = file->GetTableBMgr();
	DatabaseFileTableDManager* tdmgr = file->GetTableDMgr();
	DatabaseServices* dbapi = context->DBAPI();
	CoreServices* core = dbapi->Core();
	MsgRouter* router = core->GetRouter();
	LoadDiagnostics* diags = request->Diags();
	DeferredUpdate1StepInfo* du_1step_info = file->du_1step_info;

	int eyeball = request->EyeballRecs();

	if (eyeball)
		eyedest->WriteLine(request->Sep());
	else
		diags->AuditMed("Starting data load");

	try {
		FastLoadInputFile* taped = request->TapeD();

		if (taped->Initialize()) {
			if (eyeball) {
				eyedest->Write("Record Data (");
				eyedest->Write(taped->MakeOptsDiagSummary(0));
				eyedest->WriteLine(")");
				eyedest->WriteLine(request->Sep());
			}
			else {
				diags->AuditHigh(taped->MakeOptsDiagSummary(2));
			}
		}
		else {
			if (eyeball) {
				eyedest->WriteLine("Record Data");
				eyedest->WriteLine("-----------");
				eyedest->WriteLine("  Nothing to load");
			}
			else {
				diags->AuditHigh("  Nothing to load");
			}

			return 0;
		}

		bool pai_mode = taped->PaiOption();
		bool crlf_option = taped->CrlfOption();
		bool fnames_option = taped->FnamesOption();
		bool nofloat_option = taped->NofloatOption();
		
		char minus_one_char = -1;
		char filelevel_equals_char = '=';
		if (taped->EbcdicOption()) {
			//This one is a binary number that would not have been translated, which we
			//want to test against a translated value.
			minus_one_char = util::EbcdicToAscii(minus_one_char);
			//This a character value that would have been translated which we
			//want to test for its EBCDIC version.
			filelevel_equals_char = util::AsciiToEbcdic(filelevel_equals_char);
		}

		bool any_dynamic_index_build = request->AnyDynamicIndexBuild();
		bool any_fid_changes = request->AnyTapeFIDChanges();
		bool any_format_changes = (request->NumDataFormatChanges() > 0);
		bool any_blobs = request->AnyBLOBs();

		bool throw_bad_numbers = (dbapi->GetParmFMODLDPT() & 1) ? true : false;

		try {
			if (!fnames_option && !request->GotAllTapeFIDs())
				request->ThrowBad("TAPED trying to use field codes but TAPEF codes were missing or incomplete");

			//Enter deferred update mode if we have indexed fields with no TAPEI
			if (request->AnyDynamicIndexBuild() && !eyeball) {
				file->EnterOneStepDUMode(dbapi, diags->GetLevel(), DatabaseFile::DU1_TAPED);
				du_1step_info = file->du_1step_info;
			}

			int chunk_xfer_recs = 0;
			int totrecs = 0;
			int totfvpi = 0;
			int totblobs = 0;
			int totblobextents = 0;
			double stime = win::GetFTimeWithMillisecs();

			//Read and load records till we hit EOF
			while (!eyeball || totrecs < eyeball) {
				bool eof = false;
				bool thisrec_chunk_xfer = false;
				int recnum_in;
				int recnum_stored;

				//--- Read input record -------------------------------------------------------

				//Record number is first
				if (pai_mode)
					recnum_in = util::StringToInt(taped->ReadTextLine(&eof));
				else
					recnum_in = taped->ReadBinaryInt32(&eof);
				if (eof)
					break;

				totrecs++;

				if (eyeball)
					eyedest->WriteLine(std::string("#").append(util::IntToString(recnum_in)));

				//We may as well store the primary extent now.  It saves having to do two runs through 
				//the F=V pairs in many cases below, and the file's broken if any of this fails anyway.
				else {
					recnum_stored = tbmgr->AllocatePrimaryRecordExtent(dbapi, false);

					//For use later in EBP and TAPEI processing
					request->NoteStoredRecNum(recnum_in, recnum_stored);
				}

				//Next read the list of FV pairs, prepped for table B in this buffer
				FastLoadRecordBuffer recbuff;

				for (;;) {
					FastLoadFieldInfo* fieldinfo = NULL;
					FieldID taped_fid = -1;
					FieldValue taped_fval;
					FieldID tableb_fid = -1;

					//FV pairs on the same line ('PAI' form)
					if (pai_mode) {

						//This is done in line parts to cater for the alternate format
						std::string fname = taped->ReadTextLine(NULL, filelevel_equals_char);

						//EOR
						if (fname.length() == 0)
							break;

						int fnlen = fname.length();
						if (fname[fnlen-1] != '=' || fnlen < 3)
							request->ThrowBad("'PAI' mode: missing field name and/or equals sign");

						//Get the field code for the name
						fname.resize(fnlen-2);
						fieldinfo = request->GetLoadFieldInfoByName(fname);
						tableb_fid = fieldinfo->TableA_ID();

						//Now read in the second part of the line
						std::string paipart2 = taped->ReadTextLine(NULL, filelevel_equals_char);

						if (paipart2.length() == 0)
							request->ThrowBad("'PAI' mode: missing value on line");

						//Regular PAI format: F = V
						if (paipart2[0] == ' ')
							taped_fval.AssignData(paipart2.c_str() + 1, paipart2.length() - 1);

						//Alternate format: F =len=V
						else {
							paipart2.resize(paipart2.length()-1);
							int altpai_vallen = util::StringToInt(paipart2);

							//A third read, by length rather than CRLF
							char* valbuff = taped_fval.AllocateBuffer(altpai_vallen);
							taped->ReadChars(valbuff, altpai_vallen);

							//But also read past a cosmetic CRLF
							taped->ReadCRLF();
						}
					}

					//F and V in compressed/encoded form
					else {

						//Field name in text form
						if (fnames_option) {
							char fname[256];

							unsigned _int8 fnamelen = taped->ReadBinaryUint8();
							taped->ReadChars(fname, 1);

							//0xFFFF means EOR.  NB this convention is primarily for the fieldcodes
							//case where the hi bit cannot be set, but with field names it's also OK
							//because field names, even if len 255, must begin with a capital letter,
							//so 0xFF, or the standard EBCDIC equivalent 0xDF, are still distinctive.
							if (fnamelen == 255 && *fname == minus_one_char)
								break;

							taped->ReadChars(fname+1, fnamelen-1);
							fname[fnamelen] = 0;

							fieldinfo = request->GetLoadFieldInfoByName(fname);
							tableb_fid = fieldinfo->TableA_ID();
						}

						//Field code
						else {
							short tryfid = taped->ReadBinaryInt16();

							//Special values of FID in block Xfer mode:
							if (tryfid == -1)
								break;

							//Field codes can't have hi bit set, so it triggers record block Xfer
							if (tryfid < 0) {

								//But no need to chunk xfer if just eyeball
								if (eyeball)
									tryfid = taped->ReadBinaryInt16();              //ignore chunklen
								
								else {
									if (!thisrec_chunk_xfer) {
										if (any_blobs)
											//Sanity check, although it could work with small ones
											request->ThrowBad("Block Xfer mode is invalid for BLOB fields");
										thisrec_chunk_xfer = true;
										chunk_xfer_recs++;
									}

									short chunklen = tryfid & 0x7FFF;               //(See unload 8000 bit use)
									char* buff = recbuff.AllocateChunk(chunklen);
									taped->ReadRawData(buff, chunklen);

									//We may need look at the items within the chunk, and also maybe reformat them
									ProcessChunkXferBuffer(context, request, du_1step_info, recbuff, recnum_stored,
										any_dynamic_index_build, any_fid_changes, any_format_changes, throw_bad_numbers);

									//Might be more chunks (extended records)
									continue;
								}
							}

							//Must be a regular FID then
							taped_fid = RecordDataPage::RealFieldCode(tryfid); //reuse the block xfer readahead
							fieldinfo = request->GetLoadFieldInfoByTapeFID(taped_fid);
							tableb_fid = fieldinfo->TableA_ID();
						}

						//Then field value
						const FieldAttributes& fatts = fieldinfo->TAPEDI_Atts();

						if (fatts.IsFloat() && !nofloat_option)
							taped_fval = taped->ReadBinaryDouble();

						else {
							//Value length is 1/4 bytes depending on NBLOB/BLOB respectively
							int vallen = (fatts.IsBLOB()) ? taped->ReadBinaryInt32() : taped->ReadBinaryUint8();
							char* valbuff = taped_fval.AllocateBuffer(vallen);
							taped->ReadChars(valbuff, vallen);
						}

					} //Ways of getting FVP

					if (eyeball) {
						eyedest->Write(fieldinfo->Name());
						eyedest->Write(" = ");
						eyedest->WriteLine(taped_fval.ExtractAbbreviatedString(255));
						continue;
					}

					//Make up table B format FVP and append to buffer

					const PhysicalFieldInfo* pfi = fieldinfo->GetActualPFI();
					bool tableb_visible_reqd = pfi->atts.IsVisible();
	
					if (tableb_visible_reqd) {
						bool tableb_float_reqd = pfi->atts.IsFloat();
						bool tableb_blob_reqd = pfi->atts.IsBLOB();

						//FID
						if (tableb_float_reqd)
							RecordDataPage::MakeNumericPageCode(tableb_fid);
						else if (tableb_blob_reqd)
							RecordDataPage::MakeBLOBPageCode(tableb_fid);

						recbuff.AppendItem(&tableb_fid, 2);

						//Value
						FieldValue& tableb_fval = taped_fval;

						if (tableb_float_reqd) {
							tableb_fval.ConvertToNumeric(throw_bad_numbers);
							recbuff.AppendItem(taped_fval.RDData(), 8);
						}
						else {
							tableb_fval.ConvertToString();

							//Store BLOBs in table E (if so this pointer will be set to the descriptor instead)
							const FieldValue* pbval = &tableb_fval;
							FieldValue descriptor;

							if (tableb_blob_reqd) {
								totblobs++;
								bool maybe_benign = false;
								totblobextents += RecordDataAccessor::StoreBLOB(dbapi, tdmgr, 
									tableb_fval, descriptor, &pbval, maybe_benign);
							}
							else {
								tableb_fval.CheckStrLen255();
							}

							//Add table B value to buffer
							unsigned _int8 slen = pbval->StrLen();
							recbuff.AppendItem(&slen, 1);
							recbuff.AppendItem(pbval->StrChars(), slen);
						}
					}

					//While we have the FV pair parsed and available, store index entry if reqd
					if (fieldinfo->DynamicIndexBuild()) {
						FieldValue& tableb_fval = taped_fval;
						AddIndexEntryFromTapeDItem(context, du_1step_info, pfi, tableb_fval, recnum_stored);
					}

					totfvpi++;

				} //Each FVP that comes in

				//Extra CRLF at EOR
				if (crlf_option && !pai_mode)
					taped->ReadCRLF();

				if (eyeball)
					continue;

				//--- Whack the record data buffer out to table B -----------------------------

				int extent_recnum = recnum_stored;				
				int remaining = recbuff.RemainingToExtract();
				int fvp_boundary = -1;
				short max_extent_size = -1;

				//Divide up and write the record onto table B page extents
				while (remaining > 0) {

					int bpagenum = tbmgr->BPageNumFromRecNum(extent_recnum);
					BufferPageHandle hb = tbmgr->GetTableBPage(dbapi, bpagenum);
					RecordDataPage pb(hb);
					short slotnum = tbmgr->BPageSlotFromRecNum(extent_recnum);

					//Fill page or write the whole record, whichever is larger
					short pageavail = pb.NumFreeBytes();
					short extent_len = remaining;

					if (pageavail > 0) {

						//(...bearing in mind we can only split extents at FVP boundaries)
						if (remaining > pageavail) {
							if (fvp_boundary == -1)
								fvp_boundary = recbuff.FindHighestFVPBoundaryAtOrBelow(pageavail);
							extent_len = fvp_boundary;
						}

						//Prepare space and write data into it
						char* dest = pb.SlotPrepareSpaceForLoadData(slotnum, extent_len);
						recbuff.Extract(dest, extent_len);

						remaining = recbuff.RemainingToExtract();
					}

					//If we have more data, allocate a fresh extent, requesting a whole page.
					if (remaining > 0) {
						max_extent_size = pb.MaxFreeBytes() - 4;
						extent_len = remaining;

						//(...or again as close as we can at an FVP boundary)
						if (remaining > max_extent_size) {
							fvp_boundary = recbuff.FindHighestFVPBoundaryAtOrBelow(max_extent_size);
							extent_len = fvp_boundary; 
						}

						extent_recnum = tbmgr->AllocateExtensionRecordExtent(dbapi, false, true, extent_len);
						pb.SlotSetExtensionPtr(slotnum, extent_recnum);
					}

				} //writing each extent

				context->GetDBFile()->IncStatRECADD(dbapi);

			} //each input record

			//------------------------------------------
			//TAPED processing finished
			//------------------------------------------
			double etime = win::GetFTimeWithMillisecs();
			double elapsed = etime - stime;
			diags->AuditMed(std::string("  Data load complete in ")
					.append(RoundedDouble(elapsed).ToStringWithFixedDP(1)).append("s"));
			diags->AuditHigh(std::string("  Number of records loaded: ").append(util::IntToString(totrecs)));

			if (chunk_xfer_recs == totrecs)
				diags->AuditHigh("  Block transfer was used for all records");
			else {
				if (chunk_xfer_recs > 0)
					diags->AuditHigh("  Block transfer was used for some records");

				if (totfvpi > 0) {
					std::string msg = std::string("  Field level transfer F=V pairs input: ")
										.append(util::IntToString(totfvpi));
					
					if (totblobs) {
						msg.append(", of which BLOBs: ").append(util::IntToString(totblobs))
						.append(", onto table E extents: ").append(util::IntToString(totblobextents));
					}

					diags->AuditHigh(msg);
				}
			}

			if (eyeball && totrecs == 0)
				eyedest->WriteLine("  Nothing to load");

			taped->CloseFile();
			return totrecs;
		}
		catch (Exception& e) {
			if (eyeball)
				eyedest->WriteLine("");
			std::string msg("Error processing TAPED before offset ");
			msg.append(taped->FilePosDetailed());
			msg.append(": ").append(e.What());
			router->Issue(e.Code(), msg);

			//Errors that would normally be "benign" in a single update situation are 
			//serious here as the whole load is one big physical update.
			throw Exception(DBA_LOAD_ERROR, "Serious load error");
		}
		catch (...) {
			if (eyeball)
				eyedest->WriteLine("");
			std::string msg("Unknown error processing TAPED before offset ");
			msg.append(taped->FilePosDetailed());
			router->Issue(DBA_LOAD_ERROR, msg);
			throw;
		}
	}
	catch (...) {
		if (file->IsInDeferredUpdateMode())
			file->ExitOneStepDUMode(dbapi);

		//In eyeball mode there's no need to leave the file physically inconsistent
		if (!eyeball)
			throw;
		return -1;
	}
}

//*****************************************************************************************
//This function deals with the cases where the TAPED info is supplied such that we can read 
//it in chunks, but we may still need to go though and look at the chunk contents if:
// a) Incoming FIDs are not the same as the table A ones needed now
// b) Table B storage format (flt/str/invis) is not the same as what we want
// c) We have to build index entries from the individual data items
//Note that the user can spoof part (a) by providing a TAPEF which says a field matches
//the current table A format (thus not triggering the fieldwise reformatting below) but
//supplying TAPED blocks with the field in a different format.  However, the blockwise
//method is undocumented and primarily intended for DPT internal use.
//*****************************************************************************************
void DatabaseFileDataManager::ProcessChunkXferBuffer
(SingleDatabaseFileContext* context, FastLoadRequest* request, DeferredUpdate1StepInfo* du_1step_info,
 FastLoadRecordBuffer& recbuff, int recnum_stored, bool any_dynamic_index_build, bool any_fid_changes, 
 bool any_format_changes, bool throw_bad_numbers)
{
	if (!any_dynamic_index_build && !any_fid_changes && !any_format_changes)
		return;

	bool reformatting = any_fid_changes || any_format_changes;

	if (reformatting)
		recbuff.InitReformattingBuffer(throw_bad_numbers);

	//Loop through the buffer
	while (recbuff.RemainingToExtract()) {

		FieldID taped_fid;
		FieldValue taped_fval;
		recbuff.GetFVPair(taped_fid, taped_fval);

		//Locate the table A code and required format for field
		FastLoadFieldInfo* fieldinfo = request->GetLoadFieldInfoByTapeFID(taped_fid);
		const PhysicalFieldInfo* pfi = fieldinfo->GetActualPFI();

		if (reformatting)
			recbuff.PutReformattedFVPairFromChunk(pfi, taped_fval);

		//Build index entry if required
		if (fieldinfo->DynamicIndexBuild()) {
			FieldValue& tableb_fval = taped_fval;
			AddIndexEntryFromTapeDItem(context, du_1step_info, pfi, tableb_fval, recnum_stored);
		}
	}

	if (reformatting)
		recbuff.CommitReformatting();
	else
		recbuff.ResetExtraction();
}

//*****************************************************************************************
void DatabaseFileDataManager::AddIndexEntryFromTapeDItem
(SingleDatabaseFileContext* context, DeferredUpdate1StepInfo* du_1step_info, 
 const PhysicalFieldInfo* pfi, const FieldValue& tableb_fval, int recnum_in)
{
	if (!du_1step_info->IsInitialized())
		du_1step_info->Initialize(context);

	//NB. string values *must* convert if going into ORD NUM index here
	//NB2.  The record number added here is the one in TAPED.  It will be converted
	//to the actual stored table B record number during the index load by lookup table.
	//NB3. Unconst the PFI at this stage so DU1 can store new btree root etc. if need be.
	PhysicalFieldInfo* ucpfi = const_cast<PhysicalFieldInfo*>(pfi);

	if (du_1step_info->AddEntry(context, ucpfi, tableb_fval, recnum_in))
		du_1step_info->Flush(context, DeferredUpdate1StepInfo::MEMFULL);
}







	
	
	

	
//*************************************************************************************
//Atom manager objects.  These basically just call the above, usually for the forward
//udpates, but if TBO is invoked they call the appropriate atomic functions to do 
//the reverse updates too.
//*************************************************************************************

//*************************************************************************************
//Store record
//*************************************************************************************
AtomicBackout* AtomicUpdate_StoreEmptyRecord::CreateCompensatingUpdate()
{
	backout = new AtomicBackout_DeStoreEmptyRecord(context);
	return backout;
}

//*************************************************************************************
int AtomicUpdate_StoreEmptyRecord::Perform()
{
	RegisterAtomicUpdate();

	recnum = context->GetDBFile()->GetDataMgr()->Atom_StoreEmptyRecord(context->DBAPI());

	if (backout) {
		static_cast<AtomicBackout_DeStoreEmptyRecord*>(backout)->SetRecNum(recnum); 
		backout->Activate();
	}

	return recnum;
}

//*************************************************************************************
void AtomicBackout_DeStoreEmptyRecord::Perform()
{
	//See comments in DeInsertFieldData::Perform()
	Record r(context, recnum, true, false);

	context->GetDBFile()->GetDataMgr()->Atom_DeleteEmptyRecord(&r);
}



//*************************************************************************************
//Atom manager objects: delete record
//*************************************************************************************
AtomicUpdate_DeleteEmptyRecord::AtomicUpdate_DeleteEmptyRecord(Record* r) 
: AtomicUpdate(r->HomeFileContext()), record(r)
{}

//*************************************************************************************
AtomicBackout* AtomicUpdate_DeleteEmptyRecord::CreateCompensatingUpdate()
{
	backout = new AtomicBackout_DeDeleteEmptyRecord(record);
	return backout;
}

//*************************************************************************************
void AtomicUpdate_DeleteEmptyRecord::Perform()
{
	RegisterAtomicUpdate();

	context->GetDBFile()->GetDataMgr()->Atom_DeleteEmptyRecord(record);

	if (backout) {
		context->DBAPI()->GetUU()->InsertConstrainedRecNum(record->RecNum());
		backout->Activate();
	}
}

//*************************************************************************************
AtomicBackout_DeDeleteEmptyRecord::AtomicBackout_DeDeleteEmptyRecord(Record* r)
: AtomicBackout(r->HomeFileContext()), recnum(r->RecNum())
{}

//*************************************************************************************
void AtomicBackout_DeDeleteEmptyRecord::Perform()
{
	context->GetDBFile()->GetDataMgr()->Atom_StoreEmptyRecord(context->DBAPI(), recnum);
	context->DBAPI()->GetUU()->RemoveConstrainedRecNum(recnum);
}




//*************************************************************************************
//Atom manager objects: insert field
//*************************************************************************************
AtomicUpdate_InsertFieldData::AtomicUpdate_InsertFieldData
(Record* r, PhysicalFieldInfo* fi, const FieldValue& v, int o, bool* i, bool fr, bool du) 
: AtomicUpdate(r->HomeFileContext()), record(r), fieldinfo(fi), 
	fieldval(v), occ(o), index_reqd(i), file_registered(fr), du_mode(du)
{}


//*************************************************************************************
AtomicBackout* AtomicUpdate_InsertFieldData::CreateCompensatingUpdate()
{
	backout = new AtomicBackout_DeInsertFieldData(record, fieldinfo);
	return backout;
}

//*************************************************************************************
int AtomicUpdate_InsertFieldData::Perform()
{
	RegisterAtomicUpdate(file_registered);

	int occ_inserted = context->GetDBFile()->GetDataMgr()->Atom_InsertField
		(record, fieldinfo, fieldval, occ, index_reqd, du_mode);

	if (backout) {
		static_cast<AtomicBackout_DeInsertFieldData*>(backout)->SetInsertedOcc(occ_inserted);
		backout->Activate();
	}

	return occ_inserted;
}

//*************************************************************************************
AtomicBackout_DeInsertFieldData::AtomicBackout_DeInsertFieldData
(Record* r, PhysicalFieldInfo* fi)
: AtomicBackout(r->HomeFileContext()), recnum(r->RecNum()),
	fieldinfo(fi), occ_actual(-1)
{}

//*************************************************************************************
void AtomicBackout_DeInsertFieldData::Perform()
{
	//Nothing to do if the forward update didn't actually insert.  Note that in this 
	//case (also with change and delete) we could have skipped storing the backout 
	//entry altogether.  However, for memory safety reasons we always create the
	//tbo entry before performing the forward update, as it then minimises the chance
	//of memory failure with a forward update in place but no TBO entry.
	if (occ_actual <= 0)
		return;

	//TBO happens some time after the original update, so the original Record MRO will
	//not exist.  Therefore create a temporary one here.  

	//* * * Note re. registering for unlocked record scanning.  * * * 
	//We can skip the registering of this MRO here (see also comment on 
	//FlagUpdatedRecordMROs() above) because:
	//1. We know this MRO will be on a locked record so we don't need to be notified
	//		by other threads if they update it, since they can't.	
	//2. We know this thread will not have any other MROs itself because TBO is purely
	//		sequential, so we don't need to self-notify in similar situations.
	//3. This MRO is being created for a one-off operation, so the issue with flagging
	//		itself so the accessor cursor is reset for the next call is not a problem.
	//Even though this is a performance tweak and TBO is not high priority efficiency-wise, 
	//I couldn't see any reason not to put it in as it's such a trivial thing.  
	Record r(context, recnum, true, false);
	context->GetDBFile()->GetDataMgr()->Atom_DeleteField
		(false, &r, fieldinfo, occ_actual, NULL, NULL, NULL);
}





//*************************************************************************************
//Atom manager objects: change field
//*************************************************************************************
AtomicUpdate_ChangeFieldData::AtomicUpdate_ChangeFieldData
(Record* r, PhysicalFieldInfo* fi, const FieldValue& v, int o, 
	bool* in, bool* io) 
: 
AtomicUpdate(r->HomeFileContext()), record(r), fieldinfo(fi), newval(v),
	by_value(false), oldval(NULL), occ(o),
	index_reqd_newval(in), index_reqd_oldval(io)
{}

//By value
AtomicUpdate_ChangeFieldData::AtomicUpdate_ChangeFieldData
(Record* r, PhysicalFieldInfo* fi, const FieldValue& v, const FieldValue* ov, 
	bool* in, bool* io) 
: 
AtomicUpdate(r->HomeFileContext()), record(r), fieldinfo(fi), newval(v),
	by_value(true), oldval(ov), occ(-1),
	index_reqd_newval(in), index_reqd_oldval(io)
{}


//*************************************************************************************
AtomicBackout* AtomicUpdate_ChangeFieldData::CreateCompensatingUpdate()
{
	backout = new AtomicBackout_DeChangeFieldData(record, fieldinfo);
	return backout;
}

//*************************************************************************************
int AtomicUpdate_ChangeFieldData::Perform(FieldValue* note_old_value)
{
	RegisterAtomicUpdate();

	int occ_added;
	int occ_deleted = context->GetDBFile()->GetDataMgr()->Atom_ChangeField(
			by_value, record, fieldinfo, newval, 
			occ, oldval, &occ_added, note_old_value,
			index_reqd_newval, index_reqd_oldval);

	//Possible tune here - could skip the backout if it was the same value (see 
	//RecordDataAccessor::ChangeField) since nothing would have been touched.
	if (backout) {
		static_cast<AtomicBackout_DeChangeFieldData*>(backout)->SetDetails
			(occ_deleted, occ_added, note_old_value);
		backout->Activate();
	}

	return occ_deleted;
}

//*************************************************************************************
AtomicBackout_DeChangeFieldData::AtomicBackout_DeChangeFieldData
(Record* r, PhysicalFieldInfo* fi)
: AtomicBackout(r->HomeFileContext()), recnum(r->RecNum()),
//V2.24 gcc notes NULL=0.  The default constructor would be faster, so let's humour it.  
//This came about because oldval was a pointer once (see next func).
//	fieldinfo(fi), occ_deleted(-1), occ_added(-1), oldval(NULL) 
	fieldinfo(fi), occ_deleted(-1), occ_added(-1)
{}

//*************************************************************************************
void AtomicBackout_DeChangeFieldData::SetDetails(int d, int a, FieldValue* v)
{
	occ_deleted = d; 
	occ_added = a; 
//	oldval = new FieldValue(*v);
	oldval = *v;
}

//*************************************************************************************
//AtomicBackout_DeChangeFieldData::~AtomicBackout_DeChangeFieldData()
//{
//	if (oldval)
//		delete oldval;
//}

//*************************************************************************************
void AtomicBackout_DeChangeFieldData::Perform()
{
	//Nothing to do if the forward update didn't do anything.  See comments in insert.
	if (occ_deleted <= 0 && occ_added <= 0)
		return;

	//See comments in de-insert
	Record r(context, recnum, true, false);

	//UIP fields for which the old occurrence existed can be changed back in place. 
	//(Also UAE fields where the occ changed was the last anyway).
	if (occ_added == occ_deleted) {
		FieldValue dummy;
		context->GetDBFile()->GetDataMgr()->Atom_ChangeField
			(false, &r, fieldinfo, oldval, occ_deleted, NULL, NULL, &dummy, NULL, NULL);
	}

	//Otherwise (field is UAE or an add was performed) do it in 2 stages.  This saves
	//vastly overcomplicating the change field function.
	else {
		if (occ_added > 0)
			context->GetDBFile()->GetDataMgr()->Atom_DeleteField
				(false, &r, fieldinfo, occ_added, NULL, NULL, NULL);

		if (occ_deleted > 0)
			context->GetDBFile()->GetDataMgr()->Atom_InsertField
				(&r, fieldinfo, oldval, occ_deleted, NULL);
	}
}





//*************************************************************************************
//Atom manager objects: delete field
//*************************************************************************************
AtomicUpdate_DeleteFieldData::AtomicUpdate_DeleteFieldData
(Record* r, PhysicalFieldInfo* fi, int o, bool* i) 
: 
AtomicUpdate(r->HomeFileContext()), record(r), fieldinfo(fi), 
	by_value(false), occ(o), oldval(NULL), index_reqd(i)
{}

//By value
AtomicUpdate_DeleteFieldData::AtomicUpdate_DeleteFieldData
(Record* r, PhysicalFieldInfo* fi, const FieldValue* ov, bool* i) 
: 
AtomicUpdate(r->HomeFileContext()), record(r), fieldinfo(fi), 
	by_value(true), occ(-1), oldval(ov), index_reqd(i)
{}


//*************************************************************************************
AtomicBackout* AtomicUpdate_DeleteFieldData::CreateCompensatingUpdate()
{
	backout = new AtomicBackout_DeDeleteFieldData(record, fieldinfo);
	return backout;
}

//*************************************************************************************
int AtomicUpdate_DeleteFieldData::Perform(FieldValue* note_old_value)
{
	RegisterAtomicUpdate();

	int occ_deleted = context->GetDBFile()->GetDataMgr()->Atom_DeleteField(
			by_value, record, fieldinfo, occ, oldval, note_old_value, index_reqd);

	if (backout && occ_deleted != -1) {
		static_cast<AtomicBackout_DeDeleteFieldData*>(backout)->SetDetails
			(occ_deleted, note_old_value);
		backout->Activate();
	}

	return occ_deleted;
}

//*************************************************************************************
AtomicBackout_DeDeleteFieldData::AtomicBackout_DeDeleteFieldData
(Record* r, PhysicalFieldInfo* fi)
: AtomicBackout(r->HomeFileContext()), recnum(r->RecNum()), 
//	fieldinfo(fi), oldval(NULL), occ_deleted(-1) //see dechange above
	fieldinfo(fi), occ_deleted(-1)
{}

//*************************************************************************************
void AtomicBackout_DeDeleteFieldData::SetDetails(int d, FieldValue* v)
{
	occ_deleted = d; 
//	oldval = new FieldValue(*v);
	oldval = *v;
}

//*************************************************************************************
//AtomicBackout_DeDeleteFieldData::~AtomicBackout_DeDeleteFieldData()
//{
//	if (oldval)
//		delete oldval;
//}

//*************************************************************************************
void AtomicBackout_DeDeleteFieldData::Perform()
{
	//Nothing to do if the forward update didn't do anything. See comments in insert.
	if (occ_deleted <= 0)
		return;

	//See comments in de-insert
	Record r(context, recnum, true, false);
	context->GetDBFile()->GetDataMgr()->Atom_InsertField
		(&r, fieldinfo, oldval, occ_deleted, NULL);
}



} //close namespace


