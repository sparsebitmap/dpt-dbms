
#include "stdafx.h"

#include "dbctxt.h"

//Utils
#include "dataconv.h"
#include "handles.h"
#include "winutil.h"
#include "bbstdio.h"
//API tiers
#include "ctxtspec.h" 
#include "ctxtdef.h" 
#include "recset.h"
#include "foundset.h"
#include "findspec.h"
#include "reclist.h"
#include "sortset.h"
#include "valset.h"
#include "valdirect.h"
#include "dbserv.h"
#include "dbf_tableb.h"
#include "dbf_field.h"
#include "dbf_index.h"
#include "cfr.h"
#include "molecerr.h"
#include "update.h"
#include "dbfile.h"
#include "statview.h"
#include "core.h"
#include "msgroute.h"
#include "file.h"
//Diagnostics
#include "except.h"
#include "msg_db.h"
#include "msg_file.h"

//For tableB command and analyze3
#ifdef _BBHOST
#include "iodev.h"
#else
#include "lineio.h"
#endif

namespace dpt {

//*****************************************************************************************
DatabaseFileContext::DatabaseFileContext(DatabaseServices* ds)
: dbapi(ds)
{
#ifdef _BBHOST
	RemoveRequestLock();
#endif
}

//*****************************************************************************************
void SingleDatabaseFileContext::EnsureNoParentGroups() const 
{
	//Simplifies things on one or two places, such as with the GFT when defining fields 
	//and when initializing the file.
	if (GroupOpenCount() > 0)
		throw Exception(CONTEXT_SINGLE_FILE_ONLY, 
			"File is open as part of a group - close group(s) before issuing command");
}

//*****************************************************************************************
void SingleDatabaseFileContext::EnsureNoChildren() const 
{
	if (HasChildren())
		throw Exception(DB_IN_USE, 
			"Close all open sets, cursors etc. before issuing command.");
}

//*****************************************************************************************
void GroupDatabaseFileContext::SingleOnly() const 
{
	throw Exception(CONTEXT_SINGLE_FILE_ONLY, "Function is invalid in group context");
}
	









//*****************************************************************************************
//Information
//*****************************************************************************************
std::string DatabaseFileContext::GetCURFILE() const {return DC()->GetCURFILE();}
std::string DatabaseFileContext::GetShortName() const {return DC()->GetShortName();}
std::string DatabaseFileContext::GetFullName() const {return DC()->GetFullName();}

Group* GroupDatabaseFileContext::GetGroup() const {return DC()->GetGroup();}

std::string SingleDatabaseFileContext::GetFullFilePath() const {return af_handle.GetDSN();}

//V3.03 for access control. I held off on exposing this for a long time but there's
//no reason not to use it really. It will remain valid while the context is open.
int SingleDatabaseFileContext::GetFileID() {return af_handle.FileID();}

//V2.19 Jun 09.  Finally had enough of stepping through this 100 times a day!  Cached
//dbfile pointer in Open().
//Type was verified when opening the context
//DatabaseFile* SingleDatabaseFileContext::GetDBFile()
//{
//	return static_cast<DatabaseFile*>(af_handle.GetFile());
//}

//*****************************************************************************************
int GroupDatabaseFileContext::GetGroupIndex
(SingleDatabaseFileContext* candidate, const char* usage, bool throwit) const 
{
	//This is here for performance during e.g. big loops of PLACE RECORD calls
	if (candidate == last_verified_member)
		return last_verified_membid;

	for (size_t x = 0; x < member_contexts.size(); x++) {
		if (candidate == GetMemberContextByGroupOrder(x)) {
			last_verified_member = candidate;
			last_verified_membid = x;
			return x;
		}
	}

	if (!throwit)
		return false;

	std::string msg("Context mismatch");
	if (usage)
		msg.append(" ").append(usage);
	msg.append(": ");

	if (candidate)
		msg.append(candidate->GetFullName());
	else
		msg.append("<null context>");

	msg.append(" is not a member of ");
	msg.append(GetFullName());

	throw Exception(CONTEXT_MISMATCH, msg);
}














//*****************************************************************************************
//Opening and closing
//*****************************************************************************************
void DatabaseFileContext::Close_S(bool closing_completely, bool force)
{
	//Are there any sets etc.?
	if (closing_completely && HasChildren()) {

		if (!force) {
			//This is most likely to cause a problem when the user has global found
			//sets.  I'm not sure what to do there and indeed whether M204 allows it.
			throw Exception (DB_IN_USE, 
			std::string("Database file context ")
			.append(GetFullName())
				.append(" not closed: One or more sets, cursors etc. remains in existence"));
		}

		//This force processing would probably only be invoked in a serious error situation,
		//by the database services destructor.
		else {
			dbapi->Core()->GetRouter()->Issue(DB_IN_USE, 
				std::string("Forcing close of database file context ")
				.append(GetFullName())
				.append(", including at least one still-open set, cursor etc."));

			DestroyAllRecordSets();
			DestroyAllValueSets();
		}
	}
}

//*****************************************************************************************
//This is where the file is marked open
//*****************************************************************************************
bool SingleDatabaseFileContext::Open(const GroupDatabaseFileContext* parent, 
const std::string& du_parm1, const std::string& du_parm2, int du_parm3, int du_parm4)
{
	//Look to see if the DD is allocated already
	std::string ddname = GetShortName();
	af_handle = AllocatedFile::FindAllocatedFile(ddname, BOOL_SHR, FILETYPE_DB);

	if (af_handle.GetFile() == NULL)
		throw Exception(SYSFILE_NOT_ALLOCATED, std::string
			("Database file is not allocated: ").append(ddname));

	if (af_handle.GetType() != FILETYPE_DB)
		throw Exception(SYSFILE_BAD_TYPE, std::string
			("File is allocated, but not a database file: ").append(ddname));

	//V2.19.  Cache this for cleaner, inline GetDBFile() function.
	dbfile = static_cast<DatabaseFile*>(af_handle.GetFile());

	//We always start off in SHR mode, and exclusive-requiring functions will
	//upgrade this lock as required.  Therefore it's simpler if we only enqueue the file once
	//per thread, rather than having a thread "share with itself" if a file is opened in 
	//e.g. several groups.
	if (!IsOpenAsFile() && GroupOpenCount() == 0)
		GetDBFile()->Open(this, du_parm1, du_parm2, du_parm3, du_parm4);

	//Increment usage flags
	return SingleFileOpenableContext::Open(parent);
}

//*****************************************************************************************
//The reverse of the above
//*****************************************************************************************
bool SingleDatabaseFileContext::Close(const GroupDatabaseFileContext* parent, bool force) 
{
	//As per the M204 file manager's guide (regardless of which file is closing)
	dbapi->Commit();

	//This is done in two stages as Close_S may throw
	bool closing_completely = SingleFileOpenableContext::PreClose(parent);
	Close_S(closing_completely, force);
	SingleFileOpenableContext::Close(parent);
	
	//Release enqueue on the file if that was the last context using it (see comment
	//in Open() above).
	if (closing_completely)
		GetDBFile()->Close(this);

	return closing_completely;
}

//*****************************************************************************************
_int64 SingleDatabaseFileContext::ApplyDeferredUpdates(int forgiveness) 
{
	return GetDBFile()->ApplyDeferredUpdates(this, forgiveness);
}

//*****************************************************************************************
int SingleDatabaseFileContext::RequestFlushSingleStepDeferredUpdates() 
{
	return GetDBFile()->RequestFlushOneStepDUInfo(this);
}

//*****************************************************************************************
//Will end up calling the child functions below too
//*****************************************************************************************
bool GroupDatabaseFileContext::Open(const GroupDatabaseFileContext*, 
const std::string& du_parm1, const std::string&, int, int) 
{
	if (du_parm1.length() != 0)
		throw Exception(CONTEXT_SINGLE_FILE_ONLY, 
			"Groups may not be opened in deferred update mode");

	bool result = GroupOpenableContext::Open();

	//Populate the amalgamated field info table for e.g. UL compile time checks.
	//Note that while the group remains open, field changes will be disallowed
	//for all files in the group (see later), so the GFT remains valid after this.
	group_field_table.clear();

	//Loop on each file in the group
	try {
		for (size_t x = 0; x != member_contexts.size(); x++) {

			FieldAttributeCursorHandle fah(GetMemberContextByGroupOrder(x));
			for (fah.GotoFirst(); fah.CanEnterLoop(); fah.Advance(1)) {

				//Try and insert an entry in the GFT
				std::string name = *fah.Name();
				FieldAttributes atts = *fah.Atts();

				std::pair<std::string, FieldAttributes> p;
				p = std::make_pair<std::string, FieldAttributes>(name, atts);

				std::pair<std::map<std::string, FieldAttributes>::iterator, bool> chk;
				chk = group_field_table.insert(p);

				//Attributes must (mostly) match for like-named fields
				if (!chk.second) {

					if (!atts.GroupConsistencyCheck(chk.first->second)) {
						throw Exception(DBA_GRP_FIELD_MISMATCH, std::string
							("Group field attributes are not consistent for field: ")
							.append(name));
					}
				}
			}
		}
	}
	catch (...) {
		GroupOpenableContext::Close(true);
		throw;
	}

	return result;
}

//*****************************************************************************************
bool GroupDatabaseFileContext::Close(const GroupDatabaseFileContext*, bool force) 
{
	Close_S(true, force);
	GroupOpenableContext::Close(force);
	return true;
}

//*****************************************************************************************
//Group version functions for opening and closing.
//These functions are here because opening a group also means making available all the
//individual files as contexts in their own right.  The code somewhat echoes that in
//ProcedureServices for opening/closing contexts directly.
//The functions are overrides from GroupOpenableContext, and allow the generic group
//opening/closing code there to manipulate proc-services-specific single file contexts. 
//*****************************************************************************************
SingleFileOpenableContext* GroupDatabaseFileContext::OpenSingleFileSecondary
(const std::string& filename) const
{
	DatabaseFileContext* result;

	//First decide if the single file context's open already either as part of another
	//group, or in its own right.
	std::string specification = std::string("FILE ").append(filename);
	DatabaseFileContext* existing_context = dbapi->FindOpenContext(specification);

	//If not, create a new one
	if (existing_context)
		result = existing_context;
	else 
		result = dbapi->CreateContext(new DefinedSingleFileContext(filename));

	//Then open it as normal, except that we specify a parent group.
	try {
		dbapi->OpenContext_Single(result, std::string(), std::string(), -1, 0, this);
	}
	catch (Exception& e) {
		if (!existing_context) dbapi->DeleteContext(result);

		throw Exception(DB_OPEN_FAILED, 
			std::string("Open failed for database file group member: ")
			.append(filename)
			.append(" (")
			.append(e.What())
			.append(1, ')'));
	}
	catch (...) {
		if (!existing_context) dbapi->DeleteContext(result);

		throw Exception(DB_OPEN_FAILED, 
			std::string("Open failed for database file group member: ")
			.append(filename)
			.append(" (unknown reason - memory?)"));
	}

	//We have to cast here, as this is an override to a function shared with ProcServices.
	//First downcast (OK because we said "FILE " above) then cast up the other side of
	//the hierarchy (upcast is always OK)
	return static_cast<SingleFileOpenableContext*>(result->CastToSingle());
}

//*****************************************************************************************
//This function works similarly to the above, in that it calls DatabaseServices to close
//the single file context.
//*****************************************************************************************
bool GroupDatabaseFileContext::CloseSingleFileSecondary
(SingleFileOpenableContext* sec_in, bool force) const
{
	//Downcast from the generic parameter to echo the fact we're in an override here
	SingleDatabaseFileContext* sec = 
		static_cast<SingleDatabaseFileContext*>(sec_in);

	try {
		if (dbapi->CloseContext_Single(sec, this, force)) {
			dbapi->DeleteContext(sec);
			return true;
		}
		else
			return false;
	}
	//Open associated objects remain.  Write message here since we have an output context.
	//A normal group close will then continue.
	catch (Exception& e) {
		dbapi->Core()->GetRouter()->Issue(e.Code(), e.What());
		throw e;
	}
}















//*****************************************************************************************
//DBA stuff
//*****************************************************************************************
void SingleDatabaseFileContext::Initialize(bool leave_fields)
{
	EnsureNoChildren();
	EnsureNoParentGroups();

	GetDBFile()->Initialize(this, leave_fields, false);
}

//*****************************************************************************************
void SingleDatabaseFileContext::Increase(int amount, bool tableb)
{
	EnsureNoChildren();
	EnsureNoParentGroups();

	//Same comments as in DatabaseServices::Create about the file size
	FileOpenLockSentry fols(GetDBFile(), BOOL_EXCL, true);
	dbapi->Checkpoint(-1);

	GetDBFile()->Increase(this, amount, tableb);
}

//*****************************************************************************************
void SingleDatabaseFileContext::ShowTableExtents(std::vector<int>* result)
{
	if (!result)
		return;

	GetDBFile()->ShowTableExtents(this, result);
}

//**************************************
void SingleDatabaseFileContext::Defrag()
{
	EnsureNoChildren();
	EnsureNoParentGroups();

	FileOpenLockSentry fols(GetDBFile(), BOOL_EXCL, true);

	GetDBFile()->Defrag(this);
}



//*****************************************************************************************
//V3.0.  These two no longer stubs.
void SingleDatabaseFileContext::Unload(const FastUnloadOptions& opts, 
	const BitMappedRecordSet* baseset, std::vector<std::string>* fnames, const std::string& dir)
{
//	throw Exception(FUNC_UNDER_CONSTRUCTION, "The unload function is nearly ready, but not quite yet!");
	GetDBFile()->Unload(this, opts, baseset, fnames, dir, false);
}

//***********************************
void SingleDatabaseFileContext::Load
(const FastLoadOptions& opts, int eyeball, BB_OPDEVICE* eyeball_altdest, const std::string& dir)
{
//	throw Exception(FUNC_UNDER_CONSTRUCTION, "The load function is nearly ready, but not quite yet!");
	GetDBFile()->Load(this, opts, eyeball, eyeball_altdest, dir, false);

}



//*****************************************************************************************
//V3.0. Aug 2010.
//This code would normally go mostly in the DatabaseFile class, but it's so convenient
//to just call the existing Unload(), Initialize() etc. with minimal changes to those
//functions that a small amount of control logic out here is fine.
//*****************************************************************************************
void SingleDatabaseFileContext::Reorganize(bool oi, const std::string& oi_fieldname)
{
//	throw Exception(FUNC_UNDER_CONSTRUCTION, "The reorg function is nearly ready, but not quite yet!");

	EnsureNoChildren();
	EnsureNoParentGroups();

	DatabaseFile* f = GetDBFile();
	MsgRouter* router = dbapi->Core()->GetRouter();

	f->OperationDelimitingCommit(DBAPI());

	//Make the whole thing atomic
	FileOpenLockSentry fols(f, BOOL_EXCL, true);

	//Generate unique temp directory for the unload files
	std::string dir = std::string(util::StartupWorkingDirectory()).append("\\#REORGS\\");
	dir.append(util::IntToString(*(reinterpret_cast<int*>(&fols))));
	util::StdioEnsureDirectoryExists(dir.c_str());

	//OK let's go. It's all done with existing functions - despite what the doc may
	//suggest there is currently no super tuning with the all-in-one operation.
	try {

		//Narrow down to a single field's index?
		std::vector<std::string> fnames;
		std::vector<std::string>* pfnames = NULL;
		if (oi && oi_fieldname.length() > 0) {
			PhysicalFieldInfo* pfi = f->GetFieldMgr()->GetPhysicalFieldInfo(this, oi_fieldname);
			
			//May as well throw this now to save breaking the file
			if (!pfi->atts.IsOrdered())
				throw Exception(DML_INDEX_REQUIRED, std::string("Field is not indexed: ").append(oi_fieldname));

			fnames.push_back(oi_fieldname);
			pfnames = &fnames;
		}

		//Make unloads (replace in case of recent aborted attempt)
		FastUnloadOptions opts = FUNLOAD_REPLACE | FUNLOAD_WHAT_F | FUNLOAD_WHAT_I;
		if (!oi) 
			opts |= FUNLOAD_WHAT_D;

		f->Unload(this, opts, NULL, pfnames, dir, true);

		//The rest is updating
		f->StartNonBackoutableUpdate(this, true);

		//Next clear down either one or more indexes..
		if (oi) {

			//Collect list of field names if we're doing all
			if (oi_fieldname.length() == 0) {
				FieldAttributeCursorHandle c(this);
				for (c.GotoFirst(); c.CanEnterLoop(); c.Advance(1))
					fnames.push_back(*c.Name());
			}

			try {
				for (size_t x = 0; x < fnames.size(); x++) {
					PhysicalFieldInfo* pfi = f->GetFieldMgr()->GetPhysicalFieldInfo(this, fnames[x]);
					if (pfi->atts.IsOrdered())
						f->GetIndexMgr()->DeleteFieldIndex(this, pfi);
				}
			}
			MOLECULE_CATCH(f, dbapi->GetUU());
		}

		//...or the entire file
		else {
			f->Initialize(this, false, true);
		}

		//Whack the temp unloads back in again (deleting them afterwards)
		f->Load(this, FLOAD_CLEANAFTER, 0, NULL, dir, true);

		router->Issue(DBA_REORG_INFO, "Fast reorg complete");
	}
	catch (Exception& e) {

		//Cleanup temporary files
		if (! (DatabaseServices::GetParmLOADCTL() & LOADCTL_REORG_ERR_KEEPFILES) ) {
			try {
				win::RemoveDirectoryTree(dir.c_str());}
			catch (...) {}
		}
 
		router->Issue(e.Code(), e.What());
	}
	catch (...) {
		//Cleanup temporary files
		if (! (DatabaseServices::GetParmLOADCTL() & LOADCTL_REORG_ERR_KEEPFILES) ) {
			try {
				win::RemoveDirectoryTree(dir.c_str());}
			catch (...) {}
		}
 
		router->Issue(MISC_CAUGHT_UNKNOWN, "Unknown error in reorg");
	}
}











//*****************************************************************************************
//Field info
//*****************************************************************************************
void SingleDatabaseFileContext::DefineField(const std::string& fname, 
	bool flt, bool inv, bool uae, bool ord, bool ordnum, 
	unsigned char spc, bool nomerge, bool blob)
{	
	EnsureNoChildren();
	EnsureNoParentGroups();

	GetDBFile()->GetFieldMgr()->DefineField
		(this, fname, flt, inv, uae, ord, ordnum, spc, nomerge, blob);

	RequestRepositionCursors(1);
}

//*****************************************************************************************
void SingleDatabaseFileContext::RedefineField
(const std::string& fname, const FieldAttributes& atts)
{	
	EnsureNoChildren();
	EnsureNoParentGroups();

	GetDBFile()->GetFieldMgr()->RedefineField(this, fname, atts);

	RequestRepositionCursors(1);
}

//*****************************************************************************************
void SingleDatabaseFileContext::DeleteField(const std::string& fname)
{	
	EnsureNoChildren();
	EnsureNoParentGroups();

	GetDBFile()->GetFieldMgr()->DeleteField(this, fname);

	RequestRepositionCursors(1);
}

//*****************************************************************************************
void SingleDatabaseFileContext::RenameField
(const std::string& oldname, const std::string& newname)
{	
	EnsureNoChildren();
	EnsureNoParentGroups();

	GetDBFile()->GetFieldMgr()->RenameField(this, oldname, newname);

	RequestRepositionCursors(1);
}

//*****************************************************************************************
FieldAttributes SingleDatabaseFileContext::GetFieldAtts(const std::string& fname)
{
	return GetDBFile()->GetFieldMgr()->GetFieldAtts(this, fname);
}

//*****************************************************************************************
FieldID SingleDatabaseFileContext::GetFieldID(const std::string& fname)
{
	return GetDBFile()->GetFieldMgr()->GetPhysicalFieldInfo(this, fname, false)->id;
}

//*****************************************************************************************
FieldAttributeCursor* SingleDatabaseFileContext::OpenFieldAttCursor(bool gotofirst)
{
	FieldAttributeCursor* result = new FieldAttributeCursor_Single(this, gotofirst);
	RegisterNewCursor(result);
	return result;
}

//*****************************************************************************************
//The group version just delivers the info from the GFT
//*****************************************************************************************
FieldAttributes GroupDatabaseFileContext::GetFieldAtts(const std::string& fname)
{
	std::map<std::string, FieldAttributes>::iterator i = group_field_table.find(fname);
	if (i == group_field_table.end()) {
		std::string lit = fname;
		if (lit.length() == 0)
			lit = "<null>"; //useful as often %%V is undeclared or uninitialized

		std::string msg("Field ");
		msg.append(lit).append(" does not exist in group ").append(GetShortName());

#ifdef _BBHOST
		if (fname.length() > 0 && fname[0] == '"')
			msg.append(" (use single quotes round string instead of double?)");
#endif

		throw Exception(DBA_NO_SUCH_FIELD, msg);
	}

	return i->second;
}

//*****************************************************************************************
FieldAttributeCursor* GroupDatabaseFileContext::OpenFieldAttCursor(bool gotofirst)
{
	FieldAttributeCursor* result = new FieldAttributeCursor_Group(this, gotofirst);
	RegisterNewCursor(result);
	return result;
}
















//*****************************************************************************************
//Found sets
//*****************************************************************************************
FoundSet* DatabaseFileContext::CreateFoundSet() 
{
	FoundSet* result = new FoundSet(this);
	RegisterRecordSet(result);
	return result;
}

//*****************************************************************************************
void DatabaseFileContext::RegisterRecordSet(RecordSet* set) 
{
	//This lock is required to handle the dirty delete cross-thread notify processing
	LockingSentry ls(&record_sets_lock);
	record_sets.insert(set);
}

//*****************************************************************************************
FoundSet* SingleDatabaseFileContext::FindRecords
(const FindSpecification* pfs, const FindEnqueueType& locktype, 
 const BitMappedRecordSet* baseset)
{
	//The context of the base set must match
	if (baseset) {
		if (baseset->Context() != this)
			throw Exception(CONTEXT_MISMATCH, 
				"Context mismatch: refer-back set is from a different context");
	}

	FoundSet* f = CreateFoundSet();
	dbapi->IncStatFINDS();

	//No sense doing anything if a referback set is empty
	BitMappedFileRecordSet* fbaseset = NULL;
	if (baseset) {
		fbaseset = baseset->GetFileSubSet(0);
		if (!fbaseset)
			return f;
	}

	//All records if no spec given
	FindSpecification dummy;
	const FindSpecification* findspec = &dummy;
	if (pfs != NULL)
		findspec = pfs;

	try {
		GetDBFile()->FindRecords(0, this, f, *findspec, locktype, fbaseset);	
	}
	catch (...) {
		DestroyRecordSet(f);
		throw;
	}

	return f;
}

//*****************************************************************************************
FoundSet* GroupDatabaseFileContext::FindRecords_S
(SingleDatabaseFileContext* singlemem, const FindSpecification* pfs, 
 const FindEnqueueType& locktype, const BitMappedRecordSet* baseset)
{
	//The context of the base set must match
	if (baseset) {
		if (baseset->Context() != this)
			throw Exception(CONTEXT_MISMATCH, 
				"Context mismatch: refer-back set is from a different context");
	}

	//The member, if given, must exist
	if (singlemem)
		GetGroupIndex(singlemem, "finding group member records");

	FoundSet* f = CreateFoundSet();
	dbapi->IncStatFINDS();

	//All records if no spec given
	FindSpecification dummy;
	const FindSpecification* findspec = &dummy;
	if (pfs != NULL)
		findspec = pfs;

	try {
		for (size_t x = 0; x < member_contexts.size(); x++) {
			SingleDatabaseFileContext* sfc = GetMemberContextByGroupOrder(x);

			if (singlemem && sfc != singlemem)
				continue;

			//No sense doing anything if a referback set has no recs in this member file
			BitMappedFileRecordSet* fbaseset = NULL;
			if (baseset) {
				fbaseset = baseset->GetFileSubSet(x);
				if (!fbaseset)
					continue;
			}

			sfc->GetDBFile()->FindRecords(x, sfc, f, *findspec, locktype, fbaseset);
		}
	}
	catch (...) {
		DestroyRecordSet(f);
		throw;
	}

	return f;
}














//*****************************************************************************************
//Lists
//*****************************************************************************************
RecordList* DatabaseFileContext::CreateRecordList() 
{
	RecordList* result = new RecordList(this);
	RegisterRecordSet(result);
	return result;
}
















//*****************************************************************************************
//Misc record functions
//*****************************************************************************************
SortRecordSet* DatabaseFileContext::CreateSortRecordSet(bool ptf) 
{
	SortRecordSet* result = new SortRecordSet(this, ptf);
	RegisterRecordSet(result);
	return result;
}

//*****************************************************************************************
void SingleDatabaseFileContext::DirtyDeleteRecords(BitMappedRecordSet* goners)
{
	if (!goners)
		return;

	if (goners->Context() != this)
		throw Exception(CONTEXT_MISMATCH, 
			"Context mismatch: trying to delete set from a different context");

	//No sense doing anything if the supplied set is empty
	BitMappedFileRecordSet* fgoners = goners->GetFileSubSet(0);
	if (!fgoners)
		return;

	//The input set may well be locked but we need to take EXCL on it for this operation.  
	//Apart from this there is no need anywhere in the system to either upgrade record 
	//locks from SHR to EXCL or take more than one lock on a set.  Like dirty delete
	//handling elsewhere I'm not going to go out of my way to make it super-efficient,
	//so let's take the hit of creating a duplicate set to maintain the lock.  What's
	//more it doesn't have to be like an LPU lock which will be held till the end of
	//the update unit, since the records will be deleted and inaccessible.  So using
	//a stack object here is cool.
	FoundSet fs(this);
	fs.BitOr(goners);
	fs.DirtyDeleteAdhocLockExcl(dbapi);

	GetDBFile()->DirtyDeleteRecords(this, fgoners);	

	//Force EBM check by records accessed via current unlocked sets
	DatabaseServices::NotifyAllUsersContextsOfDirtyDelete(goners);
}

//*****************************************************************************************
void GroupDatabaseFileContext::DirtyDeleteRecords(BitMappedRecordSet* goners)
{
	if (!goners)
		return;

	if (goners->Context() != this)
		throw Exception(CONTEXT_MISMATCH, 
			"Context mismatch: trying to delete set from a different context");

	//See comments in single file version of this function
	FoundSet fs(this);
	fs.BitOr(goners);
	fs.DirtyDeleteAdhocLockExcl(dbapi);

	//Process each member in turn
	for (size_t x = 0; x < member_contexts.size(); x++) {

		//No sense doing anything if the set has no recs in this member file
		BitMappedFileRecordSet* fgoners = goners->GetFileSubSet(x);
		if (!fgoners)
			continue;

		SingleDatabaseFileContext* sfc = GetMemberContextByGroupOrder(x);
		sfc->GetDBFile()->DirtyDeleteRecords(sfc, fgoners);	
	}

	//Force EBM check by records accessed via current unlocked sets
	DatabaseServices::NotifyAllUsersContextsOfDirtyDelete(goners);
}

//*****************************************************************************************
void DatabaseFileContext::NotifyRecordSetsOfDirtyDelete(BitMappedRecordSet* goners)
{
	//Prevent thread from creating any more sets while this is called by another thread
	LockingSentry ls(&record_sets_lock);

	std::set<RecordSet*>::iterator i;
	for (i = record_sets.begin(); i != record_sets.end(); i++)
		(*i)->NotifyOfDirtyDelete(goners);
}














//*****************************************************************************************
//Destroying record sets MROs.  No type-specific functions - should there be?
//*****************************************************************************************
void DatabaseFileContext::DestroyRecordSet(RecordSet* rs)
{
	LockingSentry ls(&record_sets_lock); //see dirty delete

	if (record_sets.erase(rs) == 0)
		throw Exception(MRO_NONEXISTENT_CHILD, 
			"Bug: this context has no such record set object (any more?)");
	delete rs;
}

//*****************************************************************************************
void DatabaseFileContext::DestroyAllRecordSets()
{
	LockingSentry ls(&record_sets_lock);

	std::set<RecordSet*>::iterator i;
	for (i = record_sets.begin(); i != record_sets.end(); i++)
		delete *i;
	record_sets.clear();
}














//*****************************************************************************************
//Values and value sets
//*****************************************************************************************
ValueSet* DatabaseFileContext::CreateValueSet() 
{
	ValueSet* result = new ValueSet(this);
	value_sets.insert(result);
	return result;
}

//*****************************************************************************************
void DatabaseFileContext::DestroyValueSet(ValueSet* vs)
{
	if (value_sets.erase(vs) == 0)
		throw Exception(MRO_NONEXISTENT_CHILD, 
			"Bug: this context has no such value set object (any more?)");
	delete vs;
}

//*****************************************************************************************
void DatabaseFileContext::DestroyAllValueSets()
{
	std::set<ValueSet*>::iterator i;
	for (i = value_sets.begin(); i != value_sets.end(); i++)
		delete *i;
	value_sets.clear();
}

//*****************************************************************************************
ValueSet* SingleDatabaseFileContext::FindValues(const FindValuesSpecification& spec)
{
	ValueSet* result = CreateValueSet();
	try {
		GetDBFile()->GetIndexMgr()->FindOrCountValues(this, result, spec);
		return result;
	}
	catch (...) {
		DestroyValueSet(result);
		throw;
	}
}

//*****************************************************************************************
unsigned int SingleDatabaseFileContext::CountValues(const FindValuesSpecification& spec)
{
	return GetDBFile()->GetIndexMgr()->FindOrCountValues(this, NULL, spec);
}


//*****************************************************************************************
void SingleDatabaseFileContext::FileRecordsUnder
(BitMappedRecordSet* set, const std::string& fieldname, const FieldValue& value)
{
	if (set->Context() != this)
		throw Exception(CONTEXT_MISMATCH, 
			"Context mismatch: trying to file set from a different context");

	//NB no record is required for this.
	BitMappedFileRecordSet* fset = set->GetFileSubSet(0);
	GetDBFile()->FileRecordsUnder(this, fset, fieldname, value);	
}


//*****************************************************************************************
//Group versions
//*****************************************************************************************
ValueSet* GroupDatabaseFileContext::FindValues_S
(SingleDatabaseFileContext* singlemem, const FindValuesSpecification& spec)
{
	ValueSet* result = CreateValueSet();

	//The member, if given, must exist
	if (singlemem)
		GetGroupIndex(singlemem, "finding group member values");

	try {
		//Loop on each file in the group
		for (size_t x = 0; x < member_contexts.size(); x++) {
			SingleDatabaseFileContext* sfc = GetMemberContextByGroupOrder(x);

			if (singlemem && sfc != singlemem)
				continue;

			sfc->GetDBFile()->GetIndexMgr()->FindOrCountValues(sfc, result, spec);
		}
		return result;
	}
	catch (...) {
		DestroyValueSet(result);
		throw;
	}
}

//*****************************************************************************************
void GroupDatabaseFileContext::FileRecordsUnder
(BitMappedRecordSet* set, const std::string& fieldname, const FieldValue& value)
{
	if (set->Context() != this)
		throw Exception(CONTEXT_MISMATCH, 
			"Context mismatch: trying to file set from a different context");

	//Process each member in turn
	for (size_t x = 0; x < member_contexts.size(); x++) {

		//Note that if the set doesn't contain any records in a particular member
		//file that's fine - we just go ahead and remove the index entry in that file.
		//Note also it follows from this that if a member does not possess the field
		//there is going to be a runtime error even if the set does not contain any
		//records for that member, since the nature of FILE RECORDS is that we will
		//want to try and remove and existing index entry in that case.
		BitMappedFileRecordSet* fset = set->GetFileSubSet(x);

		SingleDatabaseFileContext* sfc = GetMemberContextByGroupOrder(x);
		sfc->GetDBFile()->FileRecordsUnder(sfc, fset, fieldname, value);	
	}
}















//*****************************************************************************************
//Direct value cursor, as used by FRV
//*****************************************************************************************
DirectValueCursor* SingleDatabaseFileContext::OpenDirectValueCursor(const std::string& fname)
{
	DirectValueCursor* result = new DirectValueCursor(this, fname);

	//Note: DVCs are used internally for convenience in a number of places such as 
	//searches, FindValues, the =PAV command. And in all such cases a stack object
	//is used, which does not get registered here.  This is OK at the moment since
	//the reposition functionality is not used with DVCs (leaf page timestamp used
	//instead) so we don't need to maintain a list of them here. 
	//And since they are stack objects the auto-close functionality is not required either.
	RegisterNewCursor(result);
	return result;
}

//*****************************************************************************************
DirectValueCursor* SingleDatabaseFileContext::OpenDirectValueCursor
(const FindValuesSpecification& fvspec)
{
	DirectValueCursor* result = new DirectValueCursor(this, fvspec);
	//See comments above.
	RegisterNewCursor(result);
	return result;
}

//Used by CreateClone to save looking up the PFI again
DirectValueCursor* SingleDatabaseFileContext::OpenDirectValueCursor(PhysicalFieldInfo* pfi)
{
	DirectValueCursor* result = new DirectValueCursor(this, pfi, NULL);
	//See comments above.
	RegisterNewCursor(result);
	return result;
}

//*****************************************************************************************
//Group versions.  (Only in member context)
//*****************************************************************************************
DirectValueCursor* GroupDatabaseFileContext::OpenDirectValueCursor
(SingleDatabaseFileContext* sfc, const std::string& fname)
{
	//The member must exist
	GetGroupIndex(sfc, "opening group member value cursor");

	DirectValueCursor* result = new DirectValueCursor(sfc, fname);
	RegisterNewCursor(result);
	return result;
}

//*****************************************************************************************
DirectValueCursor* GroupDatabaseFileContext::OpenDirectValueCursor
(SingleDatabaseFileContext* sfc, const FindValuesSpecification& fvspec)
{
	//The member must exist
	GetGroupIndex(sfc, "opening group member value cursor");

	DirectValueCursor* result = new DirectValueCursor(sfc, fvspec);
	RegisterNewCursor(result);
	return result;
}
















//*****************************************************************************************
//Updates: store
//*****************************************************************************************
int SingleDatabaseFileContext::StoreRecord(StoreRecordTemplate& data)
{
	return GetDBFile()->StoreRecord(this, data);
}

//*****************************************************************************************
//Group version
//*****************************************************************************************
int GroupDatabaseFileContext::StoreRecord(StoreRecordTemplate& data)
{
	//NB. The UL compiler will already have decided whether to perform the store against 
	//the group context or a single file (i.e. $UPDATE, $CURFILE etc), and called the 
	//above if appropriate.
	if (!updtfile_context)
		throw Exception(DBA_GRP_NO_UPDTFILE, 
			"Can't store record in group context - this group has no UPDTFILE");

	return UpdtfileContext()->StoreRecord(data);
}








//**************************************************************************************
//Access control - V3.03
//**************************************************************************************
void SingleDatabaseFileContext::ValidateReadDataPrivs
	(const char* statement, BitMappedRecordSet* unused)
{
	int fid = GetFileID();
	unsigned int userprivs = dbapi->Core()->GetEffectiveUserPrivs(fid);

	AccessController::EnsureCanReadData(userprivs, this, statement);
}

//*******************************************
void SingleDatabaseFileContext::ValidateWriteDataPrivs
	(const char* statement, BitMappedRecordSet* unused)
{
	int fid = GetFileID();
	unsigned int userprivs = dbapi->Core()->GetEffectiveUserPrivs(fid);
	
	AccessController::EnsureCanUpdateData(userprivs, this, statement);
}

//*******************************************
void GroupDatabaseFileContext::ValidateReadDataPrivs
	(const char* statement, BitMappedRecordSet* bmset)
{
	for (size_t x = 0; x < member_contexts.size(); x++) {

		//Only bother checking if some recs in the member, or if we're doing all
		if (bmset == NULL || bmset->GetFileSubSet(x) != NULL)
			GetMemberContextByGroupOrder(x)->ValidateReadDataPrivs(statement);
	}
}

//*******************************************
void GroupDatabaseFileContext::ValidateWriteDataPrivs
	(const char* statement, BitMappedRecordSet* bmset)
{
	for (size_t x = 0; x < member_contexts.size(); x++) {

		//Only bother checking if some recs in the member, or if we're doing all
		if (bmset == NULL || bmset->GetFileSubSet(x) != NULL)
			GetMemberContextByGroupOrder(x)->ValidateWriteDataPrivs(statement);
	}
}








//*****************************************************************************************
//Misc
//*****************************************************************************************
SingleDatabaseFileContext* GroupDatabaseFileContext::GetMemberContextByName
(const std::string& memname) const
{
	for (size_t x = 0; x < member_contexts.size(); x++) {
		SingleDatabaseFileContext* sfc = GetMemberContextByGroupOrder(x);

		if (sfc->GetShortName() == memname)
			return sfc;
	}
	return NULL;
}

//*****************************************************************************************
SingleDatabaseFileContext* GroupDatabaseFileContext::GetMemberContextByGroupOrder
(const int x) const
{
	return static_cast<SingleDatabaseFileContext*>(member_contexts[x]);
}

//*****************************************************************************************
void SingleDatabaseFileContext::Analyze1
(BTreeAnalyzeInfo* btinfo, InvertedListAnalyze1Info* ilinfo, 
 const std::string& fname, bool descending)
{
	GetDBFile()->GetIndexMgr()->Analyze1(btinfo, ilinfo, this, fname, descending);
}

//*****************************************************************************************
void SingleDatabaseFileContext::Analyze2(InvertedListAnalyze2Info* ilinfo)
{
	GetDBFile()->GetIndexMgr()->Analyze2(ilinfo, this);
}

//*****************************************************************************************
void SingleDatabaseFileContext::TableB
(BB_OPDEVICE* op, bool list, bool reclen, int pagefrom, int pageto)
{
	GetDBFile()->GetTableBMgr()->Dump(DBAPI(), op, list, reclen, pagefrom, pageto);
}

//*****************************************************************************************
void SingleDatabaseFileContext::Analyze3
(BB_OPDEVICE* op, const std::string& fname, bool leaves_only, bool descending)
{
	GetDBFile()->GetIndexMgr()->Analyze3(this, op, fname, leaves_only, descending);
}


//*****************************************************************************************
//Are there any still-open objects associated with the context?  We shouldn't allow it
//to be closed if so.
//*****************************************************************************************
bool DatabaseFileContext::HasChildren() const
{
	int rss = record_sets.size();
	RecordSet* rs;
	if (rss > 0)
		rs = *(record_sets.begin());
	int vss = value_sets.size();
	bool ac = AnyCursors();
	if (rss > 0 || vss > 0 || ac)
		return true;

#ifdef _BBHOST
	if (locked_by_request)
		return true;
#endif

	return false;
}



} //close namespace


