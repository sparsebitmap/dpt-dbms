
#include "stdafx.h"

#include "dbf_field.h"

//Utils
#include "parsing.h"
#include "charconv.h"
#include "dataconv.h"
#include "rsvwords.h"
#include "lineio.h"
#include "winutil.h"
#include "const_util.h"
//API tiers
#include "update.h"
#include "molecerr.h"
#include "page_f.h" //#include "page_F.h" : V2.24 case is less interchangeable on *NIX - Roger M.
#include "page_a.h" //#include "page_A.h"
#include "fastunload.h"
#include "fastload.h"
#include "loaddiag.h"
#include "fieldname.h"
#include "dbf_tabled.h"
#include "dbf_index.h"
#include "dbf_data.h"
#include "dbfile.h"
#include "dbctxt.h"
#include "dbserv.h"
#include "seqserv.h"
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

//*************************************************************************************
std::string DatabaseFileFieldManager::StandardizeFieldName(const std::string& infn)
{
	//The standardization aspect of this function is currently just to remove multiple
	//embedded spaces.  This is a M204 compatibility requirement, and could have been
	//done in the language parsers, but I decided to put it down at the API level.
	std::string fname(infn);
	util::UnBlank(fname, false);

	std::string reason;
	if (fname.length() > 255)
		reason = "exceeds 255 characters in length";
	else if (fname.length() < 1)
		reason = "null name";
	else if (!strchr(BB_CAPITAL_LETTERS, fname[0]))
		reason = "does not begin with a capital letter";

	//For the benefit of the UL parser the first word must not be a reserved word
	//V1.1: 18/6/06 - Now only object if it's a stand-alone reserved word.
	else if (ReservedWords::FindAnyWord(fname))
		reason = "stand-alone reserved word";
//	else if (ReservedWords::FindAnyWord(util::GetWord(fname, 1)))
//		reason = "starts with a reserved word";

	//Various "reserved characters" such as arithmetic operators are valid in the file 
	//(but require special parsing in UL).  0-31 are however invalid.  Especially 0 since
	//we store the field name as a zero terminated string.  Erase/flush are also invalid.
	else {
		static std::string badchars;
		if (badchars.length() == 0) {
			badchars.append(BB_ASCII_1_31);
			badchars.append(1, '\0');
			badchars.append("@#");
		}

		size_t pos = fname.find_first_of(badchars);
		if (pos != std::string::npos) {
			reason = "contains a disallowed character at position ";
			reason.append(util::IntToString(pos + 1));
		}
	}

	if (reason.length() > 0) {
		throw Exception(DBA_FLDNAME_INVALID, std::string
			("Invalid field name '")
			.append(fname)
			.append("' (")
			.append(reason)
			.append(1, ')'));
	}

	return fname;
}

//*************************************************************************************
void DatabaseFileFieldManager::RefreshFieldInfo(SingleDatabaseFileContext* context)
{
	//V2.07 - Sep 07.
	LockingSentry ls(&lookup_table_lock);
	if (lookup_tables_valid)
		return;

	DestroyFieldInfoNoLock();

	file->CheckFileStatus(false, true, true, false);

	DatabaseServices* dbapi = context->DBAPI();
	DatabaseFileTableDManager* tdm = file->GetTableDMgr();
	FCTPage_A fctpage(dbapi, file->fct_buff_page);

	//Get the start page of the field atts structure
	int pagenum = fctpage.GetFattHead();

	//Retrieve pages until there are no more
	while (pagenum != -1) {
		BufferPageHandle ha = tdm->GetTableDPage(dbapi, pagenum);
		FieldAttributePage pa(ha);

		//Retrieve all field info off the page and insert into the ID lookup table
		short offset = 0;
		PhysicalFieldInfo* info = NULL;

		for (;;) {
			info = pa.GetFieldInfo(offset);
			if (!info)
				break;

			try {
				std::pair<int, PhysicalFieldInfo*> element;
				element.first = info->id;
				element.second = info;

				idlookup.insert(idlookup.end(), element);
			}
			catch (...) {
				delete info;
				throw;
			}
		}

		pagenum = GetNextAttPageNum(pa, false);
	}

	//Build the name lookup from the ID lookup now the latter is finished
	std::map<FieldID, PhysicalFieldInfo*>::const_iterator i;
	for (i = idlookup.begin(); i != idlookup.end(); i++)
		namelookup[i->second->name] = i->second;

	lookup_tables_valid = true;
}

//*************************************************************************************
void DatabaseFileFieldManager::DestroyFieldInfo()
{
	//V2.07 - Sep 07.
	LockingSentry ls(&lookup_table_lock);
	DestroyFieldInfoNoLock();
}

//*************************************************************************************
int DatabaseFileFieldManager::GetNextAttPageNum(FieldAttributePage& pa, bool throwit)
{
	int nextfattpage = pa.GetChainPtr();
	if (nextfattpage == -1 && throwit)
		throw Exception(DB_STRUCTURE_BUG, 
			"Bug: Field attribute page chain is corrupt");
	return nextfattpage;
}


//*************************************************************************************
//V2.14 Jan 09.  Handy if calling code wants to initialize an array indexed by field ID.
//Even though initially DeleteField is not allowed (qv) this caters for the eventual
//case where field IDs might not be contiguous and so NumFields() isn't enough info.
//*************************************************************************************
FieldID DatabaseFileFieldManager::HighestFieldID(SingleDatabaseFileContext* c)
{
	RefreshFieldInfo(c);

	FieldID highest = -1;
	std::map<FieldID, PhysicalFieldInfo*>::iterator i;
	for (i = idlookup.begin(); i != idlookup.end(); i++) {
		if (i->second->id > highest)
			highest = i->second->id;
	}

	return highest;
}

//******************************
//V2.07 - Sep 07.
//If multiple users opened a file for the first time in the same instant (typically e.g. 
//at the start of a run by daemons) they would collectively build a corrupt lookup table
//for the obvious reason - oops!  This is threadsafe now (see also above).
//******************************
void DatabaseFileFieldManager::DestroyFieldInfoNoLock()
{
	lookup_tables_valid = false;

	std::map<FieldID, PhysicalFieldInfo*>::iterator i;
	for (i = idlookup.begin(); i != idlookup.end(); i++)
		delete i->second;

	idlookup.clear();
	namelookup.clear();
}


//**********************************************************************************************
//Oct 2009, in prep for V3.0.  During a fast load we're already in an update unit, so factored out
//this code for call from regular DefineField commands as well as during a load.
int DatabaseFileFieldManager::DefineField_S1
(DatabaseServices* dbapi, const std::string& fname, BufferPageHandle& ha)
{
	DatabaseFileTableDManager* tdm = file->GetTableDMgr();
	FCTPage_A fctpage(dbapi, file->fct_buff_page);

	//This is for compatibility with M204, and also serves a useful purpose here on Baby204
	//(possibly the same) in that we can use the 4 extra bits of a 16-bit value in the data
	//and indexes to save on repeated look-ups of field attributes.
	//V2.03.  Also during deferred updates.
	if (fctpage.GetAtrfld() == 4000)
		throw Exception(DBA_TOO_MANY_FIELDS, 
			"4000 fields (the maximum) are already defined to this file");

	//Chain to the last page of info.
	//NB1. There is no attempt in the current scheme to reuse space or field ID slots
	//freed by DeleteField.  This means that new field atts always go at the end of 
	//the chain, and that the new field is always the highest ID in the file.  
	//NB2. The dupe field name check is done during this, to save continually building 
	//and deleting the lookups during many consecutive Defines.  See one-off insert below.
	int pagenum = fctpage.GetFattHead();
	if (pagenum != -1) {
		ha = tdm->GetTableDPage(dbapi, pagenum);

		for (;;) {
			FieldAttributePage pa(ha);

			int nextfattpage = pa.GetChainPtr();
			if (nextfattpage == -1)
				break;

			pa.VerifyFieldMissing(fname);

			pagenum = nextfattpage;
			ha = tdm->GetTableDPage(dbapi, pagenum);
		}

		FieldAttributePage pa(ha);
		pa.VerifyFieldMissing(fname);

		//See earlier.  Field IDs cannot exceed 4000 either, even if less total # of
		//fields in the file.
		if (pa.GetNextFieldID() > 4000)
			throw Exception(DBA_TOO_MANY_FIELDS, 
				"The maximum field code of 4000 has been reached in this file");
	}

	return pagenum;
}

//**********************************************************************************************
//Oct 2009, in prep for V3.0.  Second part of the shared code without transactional wrappings
PhysicalFieldInfo* DatabaseFileFieldManager::DefineField_S2
(DatabaseServices* dbapi,
 const std::string& fname, FieldAttributes& newatts, BufferPageHandle& ha, int& pagenum)
{
	DatabaseFileTableDManager* tdm = file->GetTableDMgr();
	int start_field_id = -1;
	FCTPage_A fctpage(dbapi, file->fct_buff_page);

	//No fields defined yet - allocate a new page
	if (pagenum == -1) {
		pagenum = tdm->AllocatePage(dbapi, 'A', true);

		fctpage.SetFattHead(pagenum);
		fctpage.IncAtrpg();

		start_field_id = 0;
		ha = tdm->GetTableDPage(dbapi, pagenum, true); //empty buffer
	}

	PhysicalFieldInfo* info = NULL;

	//Try to append the new field atts to the page
	FieldAttributePage pa(ha, start_field_id);
	short newattpos = pa.AppendFieldAtts(fname, newatts);

	//OK
	if (newattpos != -1)
		info = pa.GetFieldInfo(newattpos);

	//Not enough room - allocate a new page
	else {

		int newpagenum = tdm->AllocatePage(dbapi, 'A', true);
		fctpage.IncAtrpg();

		//Extend the chain
		pa.SetChainPtr(newpagenum);

		//Format the fresh page
		BufferPageHandle newha = tdm->GetTableDPage(dbapi, newpagenum, true);
		FieldAttributePage newpa(newha, pa.GetNextFieldID());

		//Empty page so this should work now
		newattpos = newpa.AppendFieldAtts(fname, newatts);
		if (newattpos == -1)
			throw Exception(DB_STRUCTURE_BUG, 
				"Bug: fresh fatt page had insufficient space");
		else
			info = newpa.GetFieldInfo(newattpos);
	}

	//Add to memory-resident info as well.  This used to be a complete refresh for
	//every new field but that was wasteful and slow during a long sequence of defines
	//which is the typical situation where this function is called.
	try {
		std::pair<int, PhysicalFieldInfo*> element;
		element.first = info->id;
		element.second = info;

		//See comments in define and delete - the new field ID is always the highest.
		idlookup.insert(idlookup.end(), element);
	}
	catch (...) {
		delete info;
		throw;
	}
	namelookup[info->name] = info;

	fctpage.IncAtrfld();

	return info;
}

//**********************************************************************************************
void DatabaseFileFieldManager::DefineField
(SingleDatabaseFileContext* context, const std::string& infn, 
	bool flt, bool inv, bool uae, bool ord, bool ordnum, 
	unsigned char spc, bool nomerge, bool blob)
{
	DatabaseServices* dbapi = context->DBAPI();

	file->CheckFileStatus(true, true, true, false);

	//See doc - we only commit open UL updates here
	file->OperationDelimitingCommit(dbapi, true, false);

	//Ensure we have the file in EXCL (will downgrade back to SHR on exit)
	FileOpenLockSentry fols(file, BOOL_EXCL, true);

	std::string fname = StandardizeFieldName(infn);
//	FieldAttributes newatts(flt, inv, uae, ord, ordnum, spc); //V2.14 Jan 09
//	FieldAttributes newatts(flt, inv, uae, ord, ordnum, spc, nomerge); //V3.0 Nov 10.
	FieldAttributes newatts(flt, inv, uae, ord, ordnum, spc, nomerge, blob);

	//Other pre-update preliminaries
	BufferPageHandle ha;
	int pagenum = DefineField_S1(dbapi, fname, ha);

	//Now we have a reasonable expectation of success, perform the update
	file->StartNonBackoutableUpdate(context);

	try {
		DefineField_S2(dbapi, fname, newatts, ha, pagenum);

		//V3.0.  Comment only.  The three functions below (redef,del,rename) have all been
		//changed to commit at this stage since they are more likely to be issued in one-off
		//situations from the terminal.  DEFINE on the other hand typically comes in large
		//numbers from a proc, and returning to command level implies commit anyway.
		dbapi->GetUU()->EndOfMolecule(); 
	}
	MOLECULE_CATCH(file, dbapi->GetUU());
}

//**********************************************************************************************
void DatabaseFileFieldManager::RedefineField
(SingleDatabaseFileContext* context, const std::string& infn, const FieldAttributes& newatts)
{
	DatabaseServices* dbapi = context->DBAPI();

	file->CheckFileStatus(true, true, true, false);

	//See doc
	file->OperationDelimitingCommit(dbapi);

	//Ensure we have the file in EXCL (will downgrade back to SHR on exit)
	FileOpenLockSentry fols(file, BOOL_EXCL, true);

	std::string fname = StandardizeFieldName(infn);

	newatts.ValidityCheck();
	PhysicalFieldInfo* pfi = GetPhysicalFieldInfo(context, fname);
	FieldAttributes oldatts = pfi->atts;
	oldatts.RedefineValidityCheck(newatts);

	file->StartNonBackoutableUpdate(context);

	try {
		try {

			MakeFileChanges_TableB(context, pfi, oldatts, newatts);
			MakeFileChanges_TableD1(context, pfi, oldatts, newatts);

			MakeFileChanges_TableA(context, pfi, oldatts, newatts);

			MakeFileChanges_TableD2(context, pfi, oldatts, newatts);

			//Ensure local cache refresh
			DestroyFieldInfo();

			//V3.0 - arguably a minor bug, failure to commit inhibited checkpoints (but see define above)
			//dbapi->GetUU()->EndOfMolecule(); 
			dbapi->GetUU()->EndOfMolecule(true);
		}
		MOLECULE_CATCH(file, dbapi->GetUU());
	}
	//V3.0.  As with a fast load, we need not really restart the user for this.
	catch (Exception& e) {
		if (e.Code() == TXNERR_LOGICALLY_BROKEN || e.Code() == TXNERR_PHYSICALLY_BROKEN)
			throw Exception(DB_FILE_STATUS_BROKEN, e.What());
		else
			throw;
	}

	RefreshFieldInfo(context);
}

//***************************************************
void DatabaseFileFieldManager::MakeFileChanges_TableA
(SingleDatabaseFileContext* context, PhysicalFieldInfo* pfi,
 const FieldAttributes& oldatts, const FieldAttributes& newatts)
{
	DatabaseServices* dbapi = context->DBAPI();
	DatabaseFileTableDManager* tdmgr = file->GetTableDMgr();

	int pagenum = FCTPage_A(dbapi, file->fct_buff_page).GetFattHead();
	BufferPageHandle ha = tdmgr->GetTableDPage(dbapi, pagenum);

	//Look at att pages till we find the field entry on a page
	for (;;) {
		FieldAttributePage pa(ha);

		short offset = pa.LocateField(pfi->name.c_str());
		if (offset == -1) {
			pagenum = GetNextAttPageNum(pa);
			ha = tdmgr->GetTableDPage(dbapi, pagenum);
			continue;
		}

		//Flags byte
		if (newatts.Flags() != oldatts.Flags())
			pa.SetAttributeByte(offset, newatts.Flags());


		//Create/delete index information block
		if (!oldatts.IsOrdered() && newatts.IsOrdered())
			pa.InsertIndexInfoBlock(offset, pfi);

		else if (oldatts.IsOrdered() && !newatts.IsOrdered())
			pa.RemoveIndexInfoBlock(offset);


		//Index splitpct if appropriate
		if (newatts.IsOrdered() && newatts.Splitpct() != oldatts.Splitpct())
			pa.SetSplitpct(offset, newatts.Splitpct());

		break;
	}
}

//***************************************************
void DatabaseFileFieldManager::MakeFileChanges_TableB
(SingleDatabaseFileContext* context, PhysicalFieldInfo* pfi,
 const FieldAttributes& oldatts, const FieldAttributes& newatts)
{
	DatabaseFileDataManager* datamgr = file->GetDataMgr();

	//V3.0 -  Oops - FLOAT->INVIS did not work.  Also changed this to allow STRING->BLOB too.
//	if (oldatts.IsVisible() && oldatts.IsFloat() != newatts.IsFloat() ) 
	if (oldatts.IsVisible() && newatts.IsVisible() && 
		(oldatts.IsFloat() != newatts.IsFloat() || oldatts.IsBLOB() != newatts.IsBLOB()) ) 
	{ 
		//datamgr->ChangeFieldFormatOnEveryRecord(context, pfi);
		datamgr->ChangeFieldFormatOnEveryRecord(context, pfi, oldatts.IsFloat() != newatts.IsFloat());
	}
	else if (newatts.IsInvisible() && oldatts.IsVisible())
	{
		datamgr->DeleteFieldFromEveryRecord(context, pfi);
	}
	else if (newatts.IsVisible() && oldatts.IsInvisible())
	{
		datamgr->VisiblizeFieldFromIndex(context, pfi, newatts.IsFloat());
	}
}

//***************************************************
//Table D changes performed before the table A work
void DatabaseFileFieldManager::MakeFileChanges_TableD1
(SingleDatabaseFileContext* context, PhysicalFieldInfo* pfi,
 const FieldAttributes& oldatts, const FieldAttributes& newatts)
{
	DatabaseFileIndexManager* indexmgr = file->GetIndexMgr();

	if (oldatts.IsOrdered() && !newatts.IsOrdered())
		indexmgr->DeleteFieldIndex(context, pfi);

	else if (oldatts.IsOrdered() && (oldatts.IsOrdNum() != newatts.IsOrdNum()) ) {
		
		//Change cached splitpct now so new b-tree packs correctly
		pfi->atts.SetSplitPct(newatts.Splitpct());

		indexmgr->ChangeIndexType(context, pfi);
	}
}

//***************************************************
//Table D changes performed after the table A work
void DatabaseFileFieldManager::MakeFileChanges_TableD2
(SingleDatabaseFileContext* context, PhysicalFieldInfo* pfi,
 const FieldAttributes& oldatts, const FieldAttributes& newatts)
{
	DatabaseFileIndexManager* indexmgr = file->GetIndexMgr();

	if (!oldatts.IsOrdered() && newatts.IsOrdered()) {

		//Change cached splitpct now so new b-tree packs correctly
		pfi->atts.SetSplitPct(newatts.Splitpct());

		//Also since this will be going through DU processing
		if (newatts.IsNoMerge()) 
			pfi->atts.SetNoMergeFlag();
		else
			pfi->atts.ClearNoMergeFlag();

		indexmgr->CreateIndexFromData(context, pfi, newatts.IsOrdNum());
	}
}





//**********************************************************************************************
void DatabaseFileFieldManager::DeleteField
(SingleDatabaseFileContext* context, const std::string& fname)
{
	DatabaseServices* dbapi = context->DBAPI();
	DatabaseFileTableDManager* tdmgr = file->GetTableDMgr();
	DatabaseFileDataManager* datamgr = file->GetDataMgr();
	DatabaseFileIndexManager* indexmgr = file->GetIndexMgr();

	//This is OK if file is full
	file->CheckFileStatus(false, true, true, true);

	//See doc
	file->OperationDelimitingCommit(dbapi);

	//Ensure we have the file in EXCL (will downgrade back to SHR on exit)
	FileOpenLockSentry fols(file, BOOL_EXCL, true);

	//Check for field existence
	PhysicalFieldInfo* pfi = GetPhysicalFieldInfo(context, fname);

	file->StartNonBackoutableUpdate(context);

	try {
		//Remove index entries
		if (pfi->atts.IsOrdered())
			indexmgr->DeleteFieldIndex(context, pfi);

		//Remove all traces of field from table B
		if (pfi->atts.IsVisible())
			datamgr->DeleteFieldFromEveryRecord(context, pfi);
		
		//Finally remove the field attributes.
		//*NB* The field IDs of remaining fields do not change (we don't shuffle down
		//slots).  Also Define() always adds new fields as the last one and doesn't 
		//try to reuse the slots freed up by delete.  That makes possible a few neat 
		//shortcuts throughout these field maint funcs, and lots of field deletions
		//followed by defines is extremely unlikely anyway.
		int pagenum = FCTPage_A(dbapi, file->fct_buff_page).GetFattHead();
		BufferPageHandle ha = tdmgr->GetTableDPage(dbapi, pagenum);

		//Look at att pages till we find the field entry on a page
		for (;;) {
			FieldAttributePage pa(ha);

			short offset = pa.LocateField(fname.c_str());
			if (offset == -1) {
				pagenum = GetNextAttPageNum(pa);
				ha = tdmgr->GetTableDPage(dbapi, pagenum);
				continue;
			}

			pa.DeleteFieldEntry(offset);
			break;
		}

		//Ensure local cache refresh
		DestroyFieldInfo();

		//V3.0 - arguably a minor bug, failure to commit inhibited checkpoints (but see define above)
		//dbapi->GetUU()->EndOfMolecule();
		dbapi->GetUU()->EndOfMolecule(true);
	}
	MOLECULE_CATCH(file, dbapi->GetUU());

	RefreshFieldInfo(context);
}

//**********************************************************************************************
void DatabaseFileFieldManager::RenameField(SingleDatabaseFileContext* context, 
	const std::string& inon, const std::string& innn)
{
	DatabaseServices* dbapi = context->DBAPI();

	file->CheckFileStatus(true, true, true, false);

	//See doc
	file->OperationDelimitingCommit(dbapi);

	//Ensure we have the file in EXCL (will downgrade back to SHR on exit)
	FileOpenLockSentry fols(file, BOOL_EXCL, true);

	//Check for existing field existence
	std::string oldname = StandardizeFieldName(inon);
	FieldAttributes oldatts = GetFieldAtts(context, oldname);
	
	//And new name non-existence
	std::string newname = StandardizeFieldName(innn);
	try {
		FieldAttributes newatts = GetFieldAtts(context, newname);
		throw Exception(DBA_FLDATT_INVALID, std::string("Field ")
			.append(newname).append(" already exists in file ")
			.append(file->GetDDName()));
	}
	catch (Exception& e) {
		if (e.Code() != DBA_NO_SUCH_FIELD)
			throw;
	}

	DestroyFieldInfo();
	file->StartNonBackoutableUpdate(context);

	try {
		FCTPage_A fctpage(dbapi, file->fct_buff_page);
		int pagenum = fctpage.GetFattHead();

		DatabaseFileTableDManager* tdm = file->GetTableDMgr();
		BufferPageHandle ha;
		ha = tdm->GetTableDPage(dbapi, pagenum);

		//Look at att pages till we find the field
		for (;;) {
			FieldAttributePage pa(ha);

			short offset = pa.LocateField(oldname.c_str());
			if (offset == -1) {
				pagenum = GetNextAttPageNum(pa);
				ha = tdm->GetTableDPage(dbapi, pagenum);
				continue;
			}

			if (!pa.ChangeFieldName(offset, newname))
				throw Exception(TXN_BENIGN_ATOM, 
					"Field attributes page cannot expand any more "
					"(choose shorter new name?)");
			break;
		}

		//V3.0 - arguably a minor bug, failure to commit inhibited checkpoints (but see define above)
		//dbapi->GetUU()->EndOfMolecule();
		dbapi->GetUU()->EndOfMolecule(true);
	}
	MOLECULE_CATCH(file, dbapi->GetUU());

	RefreshFieldInfo(context);
}

//*************************************************************************************
//Here we make no attempt to re-code fields and reclaim space from deleted fields.  
//In other words this is not a full "reorg" of the fatt area.
//*************************************************************************************
int DatabaseFileFieldManager::SoftInitialize(SingleDatabaseFileContext* context)
{
	DatabaseServices* dbapi = context->DBAPI();

	int atrpg = 0;
	DatabaseFileTableDManager* tdm = file->GetTableDMgr();

	FCTPage_A fctpage(dbapi, file->fct_buff_page);
	int pagenum_old = fctpage.GetFattHead();

	while (pagenum_old != -1) {

		BufferPageHandle ha_old = tdm->GetTableDPage(dbapi, pagenum_old);
		FieldAttributePage pa_old(ha_old);

		//Clear anything altered after initial DEFINE of each field
		pa_old.InitializeDynamicFieldInfo();

		//Pack the fatt pages down to a contiguous set the bottom of table D.
		int chainptr_old = pa_old.GetChainPtr();
		if (chainptr_old != -1)
			pa_old.SetChainPtr(atrpg + 1);

		//If it needs moving (they usually won't) move to lower location
		int pagenum_new = atrpg;
		if (pagenum_old != pagenum_new) {
			BufferPageHandle ha_new = tdm->GetTableDPage(dbapi, pagenum_new);

			GenericPage pg_old = GenericPage(ha_old);
			GenericPage pg_new = GenericPage(ha_new);
			pg_new.Overwrite(pg_old);
		}

		//Get the next fatt page as per the old structure
		pagenum_old = chainptr_old;
		atrpg++;
	}

	if (pagenum_old != -1)
		fctpage.SetFattHead(0);

	//Ensure any cleared index root pages get loaded again if required later
	DestroyFieldInfo();

	return atrpg;
}

//*************************************************************************************
int DatabaseFileFieldManager::Initialize
(SingleDatabaseFileContext* context, bool leave_fields)
{
	//The return value is the post-initialize value for ATRPG
	if (leave_fields)
		return SoftInitialize(context);

	DestroyFieldInfo();
	return 0;
}

//*************************************************************************************
void DatabaseFileFieldManager::TakeFieldAttsTableCopy
(SingleDatabaseFileContext* context, std::vector<PhysicalFieldInfo>* caller_table)
{
	RefreshFieldInfo(context);
	caller_table->clear();

	std::map<std::string, PhysicalFieldInfo*>::const_iterator il;
	for (il = namelookup.begin(); il != namelookup.end(); il++)
		caller_table->push_back(*(il->second));
}

//*************************************************************************************
//V2.20. Useful where trusted caller wants an array for fast local lookup by field ID
void DatabaseFileFieldManager::GetIndexedAttsArray
(SingleDatabaseFileContext* context, std::vector<PhysicalFieldInfo*>* caller_table)
{
	RefreshFieldInfo(context);
	caller_table->clear();
	
	FieldID highest = HighestFieldID(context);
	caller_table->resize(highest + 1, NULL);

	std::map<FieldID, PhysicalFieldInfo*>::const_iterator il;
	for (il = idlookup.begin(); il != idlookup.end(); il++)
		(*caller_table)[il->first] = il->second;
}

//*************************************************************************************
PhysicalFieldInfo* DatabaseFileFieldManager::GetPhysicalFieldInfo
(SingleDatabaseFileContext* context, const std::string& fname, bool throwit)
{
	RefreshFieldInfo(context);

	std::map<std::string, PhysicalFieldInfo*>::const_iterator il = namelookup.find(fname);
	if (il == namelookup.end()) {
		std::string lit = fname;
		if (lit.length() == 0)
			lit = "<null>"; //useful as often %%V is undeclared or uninitialized

		std::string msg("Field ");
		msg.append(lit).append(" does not exist in file ").append(file->GetDDName());

#ifdef _BBHOST
		if (fname.length() > 0 && fname[0] == '"')
			msg.append(" (use single quotes round string instead of double?)");
#endif

		//V3.0. Handy to lookup without having to try/catch for fail
		if (throwit)
			throw Exception(DBA_NO_SUCH_FIELD, msg);
		else
			return NULL;
	}

	return il->second;
}

//*************************************************************************************
PhysicalFieldInfo* DatabaseFileFieldManager::GetPhysicalFieldInfo
(SingleDatabaseFileContext* context, FieldID fid)
{
	RefreshFieldInfo(context);

	std::map<FieldID, PhysicalFieldInfo*>::const_iterator il = idlookup.find(fid);
	if (il == idlookup.end())
		throw Exception(DBA_NO_SUCH_FIELD, 
			//V3.0 - this might reasonably occur during a fast load if the user has
			//prepared the input by hand, so not a "bug".
			std::string("File has no field info for ID: ").append(util::IntToString(fid)));
//			std::string("Bug: No field info for ID: ").append(util::IntToString(fid)));

	return il->second;
}

//***************************************************************************************
PhysicalFieldInfo* DatabaseFileFieldManager::GetAndValidatePFI
(SingleDatabaseFileContext* home_context, const std::string& fname, 
 bool changing_field_on_record, bool invisible_change_allowed, bool file_records, 
 FieldAttributes* pgrpatts, DatabaseFileContext* source_set_context)
{
	try {
		PhysicalFieldInfo* pfi = home_context->GetDBFile()->GetFieldMgr()->
			GetPhysicalFieldInfo(home_context, fname);

		const char* err = NULL;

		//Field visibility is relevant in certain circumstances
		if (pfi->atts.IsVisible()) {
			if (file_records)
				err = "FILE RECORDS required an invisible field";
		}
		else {
			if (!file_records && !(changing_field_on_record && invisible_change_allowed)) {
				if (changing_field_on_record)
					err = "the old value must be specified when changing an invisible field";
				else
					err = "requested operation requires visible field(s)"; //reading
			}
		}

		if (err) {
			std::string msg("Run-time fieldname (");
			msg.append(fname).append("): ").append(err);

			throw Exception(DML_INVALID_INVIS_FUNC, msg);
		}

		return pfi;
	}
	catch (Exception& e) {

		//This code is OK if we are reading in group context but the current 
		//member doesn't have the field.  This extra lookup is somewhat of an overhead 
		//if it's a big loop, but it serves the user right for not narrowing their record
		//set down properly.
		GroupDatabaseFileContext* gc = NULL;
		if (source_set_context)
			gc = source_set_context->CastToGroup();

		if (!gc || changing_field_on_record || file_records || e.Code() != DBA_NO_SUCH_FIELD)
			throw;

		//This will rethrow if the group doesn't have the field
		FieldAttributes ga = gc->GetFieldAtts(fname);

		//OK the group has the field
		if (pgrpatts)
			*pgrpatts = ga;
		return NULL;
	}
}

//***************************************************************************************
void DatabaseFileFieldManager::ConvertValue
(SingleDatabaseFileContext* home_context, PhysicalFieldInfo* pfi, const FieldValue& inval, 
 FieldValue* converted_val, const FieldValue** use_val,
 bool index, bool* val_was_converted, bool* invalid_val_zero)
{
	bool to_numeric = (index) ? pfi->atts.IsOrdNum() : pfi->atts.IsFloat();

	if (val_was_converted)
		*val_was_converted = true;

	*use_val = converted_val;
	*converted_val = inval;

	//This never fails
	if (!to_numeric)
		converted_val->ConvertToString();

	else {
		//We may or may not object to invalid numbers
		try {
			converted_val->ConvertToNumeric(true);
		}
		catch (...) {
			const char* usage = (index) ? "ORD NUM index" : "FLOAT data";

			//This bit disables the automatic storing of zero for invalid numeric values
			if (home_context->DBAPI()->GetParmFMODLDPT() & 1)
				throw Exception(DML_NON_FLOAT_ERROR, 
				std::string("Failed attempt to store non-numeric entry '")
					.append(inval.ExtractString())
					.append("' for ").append(usage).append(" field '")
					.append(pfi->name)
					.append("'"));

			//Store zero then
			*converted_val = 0;
			if (invalid_val_zero)
				*invalid_val_zero = true;

			home_context->DBAPI()->Core()->GetRouter()->Issue(DML_NON_FLOAT_INFO,
				std::string("Warning: non-numeric value '")
				.append(inval.ExtractString())
				.append("' treated as zero for storage in ").append(usage).append(" field '")
				.append(pfi->name)
				.append("'"));
		}
	}
}

//*************************************************************************************
void DatabaseFileFieldManager::UpdateBTreeRootPage
(DatabaseServices* dbapi, PhysicalFieldInfo* pfi)
{
	//This will happen sufficiently infrequently that it's not worth holding
	//the A page number and/or offset just to avoid a scan here.
	DatabaseFileTableDManager* tdm = file->GetTableDMgr();
	FCTPage_A fctpage(dbapi, file->fct_buff_page);

	//Simple A-page chain loop as used elsewhere
	int pagenum = fctpage.GetFattHead();

	for (;;) {
		BufferPageHandle ha = tdm->GetTableDPage(dbapi, pagenum);
		FieldAttributePage pa(ha);

		short offset = 0;
		for (;;) {
			if (pa.UpdateBTreeRootPage(offset, pfi))
				return;
			if (offset == -1)
				break;
		}

		pagenum = GetNextAttPageNum(pa);
	}
}


//*************************************************************************************
void FastUnloadFieldInfoParallelSubTask::Perform()
{
	request->Context()->GetDBFile()->GetFieldMgr()->Unload(request);
}

//*************************************************************************************
void DatabaseFileFieldManager::Unload(FastUnloadRequest* request)
{
	SingleDatabaseFileContext* context = request->Context();
	LoadDiagnostics* diags = request->Diags();

	FastUnloadOutputFile* tapef = request->TapeF();
	tapef->Initialize(context, "field definitions");

	diags->AuditMed("Starting field definitions unload");

	int totflds = 0;
	double stime = win::GetFTimeWithMillisecs();

	//Alphabetical order is the nicest to look at
	std::map<std::string, PhysicalFieldInfo*>::const_iterator i;
	for (i = namelookup.begin(); i != namelookup.end(); i++) {
		PhysicalFieldInfo* pfi = i->second;

		//Skip excluded ones
		if (!pfi->extra)
			continue;

		totflds++;

		std::string line("DEFINE FIELD ");
		if (request->FidRequiredInTapeF())
			line.append(util::IntToString(pfi->id)).append(" ");
		line.append("'").append(pfi->name).append("' (");

		line.append(MakeFieldAttsDDL(pfi->atts, false));
		
		line.append(")");
		tapef->AppendTextLine(line);
	}

	tapef->Finalize();

	double etime = win::GetFTimeWithMillisecs();
	double elapsed = etime - stime;

	diags->AuditMed(std::string("  Field definitions unload complete (")
		.append(util::IntToString(totflds)).append(" fields). Elapsed: ")
		.append(RoundedDouble(elapsed).ToStringWithFixedDP(1)).append("s"));
}


//*************************************************************************************
int DatabaseFileFieldManager::Load(FastLoadRequest* request, BB_OPDEVICE* eyedest)
{
	SingleDatabaseFileContext* context = request->Context();
	DatabaseServices* dbapi = context->DBAPI();
	CoreServices* core = dbapi->Core();
	MsgRouter* router = core->GetRouter();
	LoadDiagnostics* diags = request->Diags();

	int eyeball = request->EyeballRecs();

	if (eyeball)
		eyedest->WriteLine(request->Sep());
	else
		diags->AuditMed("Starting field definitions load");

	try {
		FastLoadInputFile* tapef = request->TapeF();
	
		if (tapef->Initialize()) {
			if (eyeball) {
				eyedest->Write("Field Definitions (");
				eyedest->Write(tapef->MakeOptsDiagSummary(0));
				eyedest->WriteLine(")");
				eyedest->WriteLine(request->Sep());
			}
			else {
				diags->AuditHigh(tapef->MakeOptsDiagSummary(2));
			}
		}
		else {
			if (eyeball) {
				eyedest->WriteLine("Field Definitions");
				eyedest->WriteLine("-----------------");
				eyedest->WriteLine("  Nothing to load");
			}
			else {
				diags->AuditHigh("  Nothing to load");
			}

			return 0;
		}

		try {
			int totentries = 0;
			int totflds_new = 0;
			double stime = win::GetFTimeWithMillisecs();

			//This file is just text lines
			for (;;) {
				bool eof = false;
				std::string line;
				tapef->ReadCommandTextLine(line, &eof);
				if (eof && line.length() == 0)
					break;

				if (line.length() < 14 || line.substr(0, 13) != "DEFINE FIELD ")
					request->ThrowBad(std::string("Expected a DEFINE FIELD command, not '")
								.append(line.substr(0,13)).append("'..."));
				line.erase(0,13);

				//If the first word's a number it's the field ID
				int ifid = -1;
				int cursor = 0;
				std::string word = util::GetWord(line, &cursor);
				if (util::IsInteger(word)) {
					ifid = util::StringToInt(word);
					line.erase(0, cursor);
				}
				else if (line[0] == 'X' && line[1] == '\'') {
					size_t quote2 = line.find('\'', 2);
					if (quote2 == std::string::npos)
						request->ThrowBad("Mismatched quotes on field code");
					std::string hex = line.substr(2, quote2-2);
					ifid = util::HexStringToUlong(hex);
					line.erase(0, quote2+1);
				}

				//Now we can parse it as a regular DEFINE FIELD command
				FieldNameParser fnparser;
				static const std::string termwords = "WITH";
				cursor = fnparser.ParseFieldName(line, &termwords);
				std::string fname = StandardizeFieldName(fnparser.FieldName());

				FieldAttsParser faparser;
				std::string sbadatts;
				faparser.ParseFieldAttributes(line, cursor, &sbadatts);
				
				//In this case to handle M204 DDL output we allow some invalid atts
				if (sbadatts.length() > 0) {
					router->Issue(DBA_UNLOAD_INFO, std::string
						("Warning: ignored DDL options not supported on DPT: ")
						.append(sbadatts));
				}

				FieldAttributes fatts = faparser.MakeAtts();

				//Locate any existing table A definition for this field
				PhysicalFieldInfo* oldpfi = GetPhysicalFieldInfo(context, fname, false);
				PhysicalFieldInfo* pfi = oldpfi;

				//Or define it new now
				bool existed;
				if (oldpfi != NULL)
					existed = true;
				else {
					existed = false;
					totflds_new++;

					//Same as regular DefineField() but with no transactional wrappings
					if (!eyeball) {
						BufferPageHandle ha;
						int pagenum = DefineField_S1(dbapi, fname, ha);
						pfi = DefineField_S2(dbapi, fname, fatts, ha, pagenum);
					}
				}
				totentries++;

				//Note all this info for use by the rest of the load
				request->AddFieldInfo(fname, pfi, (oldpfi == NULL), true, ifid, fatts);

				//Print the info out for review if requested.  NB we still go through all fields
				//even if eyeballing, since the info is required to understand TAPED & TAPEI.
				if (eyeball || diags->IsHigh()) {
					std::string line1("  ");
					std::string line2("  ");

					if (ifid != -1)
						line1.append("[").append(util::UlongToHexString(ifid, 4, true)).append("] ");
					std::string fnamepad(fname);
					if (fname.length() < 29) fnamepad.resize(29, ' ');
					line1.append(fnamepad).append(" : ").append(MakeFieldAttsDDL(fatts, true));

					if (!existed) 
						line2 = "    Defined";
					else {
						line2.append("    Not defined, field already existed with ");
						line2.append(MakeFieldAttsDDL(pfi->atts, true));
					}

					if (eyeball) {
						if (totentries <= eyeball)
							eyedest->WriteLine(line1);
					}
					else {
						diags->AuditHigh(line1);
						diags->AuditHigh(line2);
					}
				}
			}

			double etime = win::GetFTimeWithMillisecs();
			double elapsed = etime - stime;
			diags->AuditMed(std::string("  Field definitions load complete in ")
					.append(RoundedDouble(elapsed).ToStringWithFixedDP(1)).append("s"));
			diags->AuditHigh(std::string("    Number of entries processed : ")
							.append(util::IntToString(totentries)));
			diags->AuditHigh(std::string("    Number of fields defined    : ")
							.append(util::IntToString(totflds_new)));
			diags->AuditHigh(std::string("       (num already existed)    : ")
							.append(util::IntToString(totentries - totflds_new)));

			tapef->CloseFile();
			return totflds_new;
		}
		catch (Exception& e) {
			if (eyeball)
				eyedest->WriteLine("");
			std::string msg("Error processing TAPEF in line before offset ");
			msg.append(tapef->FilePosDetailed());
			msg.append(": ").append(e.What());
			router->Issue(e.Code(), msg);

			//Most of the time these will be parsing issues and we won't have even touched
			//the file yet, but there is no easy way to differentiate these from problems
			//making a file update during a field define, so sadly always phys broken now.
			throw Exception(DBA_LOAD_ERROR, "Serious load error");
		}
		catch (...) {
			if (eyeball)
				eyedest->WriteLine("");
			std::string msg("Unknown error processing TAPEF in line before offset ");
			msg.append(tapef->FilePosDetailed());
			router->Issue(DBA_LOAD_ERROR, msg);
			throw;
		}
	}
	catch (...) {
		//In eyeball mode there's no need to leave the file physically inconsistent
		if (!eyeball)
			throw;
		return -1;
	}
}

//*************************************************************************************
std::string DatabaseFileFieldManager::MakeFieldAttsDDL
(const FieldAttributes& atts, bool abbrev)
{
	std::string result;

	//This print code was lifted direct from DisplayFieldCommand::Execute(), but 
	//without the group context considerations.
	if (atts.IsInvisible())
		result.append( (abbrev) ? "INV" : "INVISIBLE" );

	else {
		result.append( (abbrev) ? "VIS" : "VISIBLE" );
		result.append( (abbrev) ? " " : ", " );

		if (atts.IsFloat())
			result.append( (abbrev) ? "FLOAT" : "FLOAT" ); //see comment in orig code
		else {
			result.append( (abbrev) ? "STR" : "STRING" );
			
			if (atts.IsBLOB())
				result.append( (abbrev) ? " BLOB" : " BINARY-LARGE-OBJECT" );
		}

		result.append( (abbrev) ? " " : ", " );

		if (atts.IsUpdateAtEnd())
			result.append( (abbrev) ? "UE" : "UPDATE AT END" );
		else
			result.append( (abbrev) ? "UP" : "UPDATE IN PLACE" );
	}

	result.append( (abbrev) ? " " : ", " );

	//-------------------
	if (atts.IsOrdered()) {
		result.append( (abbrev) ? "ORD " : "ORDERED " );

		if (atts.IsOrdNum())
			result.append( (abbrev) ? "NUM " : "NUMERIC" );
		else
			result.append( (abbrev) ? "CHAR" : "CHARACTER" );

		result.append( (abbrev) ? " " : ", " );
		result.append( (abbrev) ? "SPLT " : "SPLITPCT " );

		result.append(util::IntToString(atts.Splitpct()));

		if (atts.IsNoMerge()) {
			result.append( (abbrev) ? " " : ", " );
			result.append( (abbrev) ? "NM" : "NO MERGE" );
		}
	}
	else {
		result.append( (abbrev) ? "NORD" : "NON-ORDERED" );
	}

	return result;
}











//*************************************************************************************
std::string DatabaseFileFieldManager::ViewParm
(SingleDatabaseFileContext* context, const std::string& parmname)
{
	DatabaseServices* dbapi = context->DBAPI();

	FCTPage_A fctpage(dbapi, file->fct_buff_page);

	//Parms that require FILE EXCL to be changed, so no lock required
	if (parmname == "ATRPG") 
		return util::IntToString(fctpage.GetAtrpg());
	if (parmname == "ATRFLD") 
		return util::IntToString(fctpage.GetAtrfld());

	throw "surely shome mishtake";
}

} //close namespace


