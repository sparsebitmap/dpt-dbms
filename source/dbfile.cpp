
#include "stdafx.h"

#include "dbfile.h"

//Utils
#include "dataconv.h"
#include "charconv.h"
#include "parsing.h"
#include "lineio.h"
#include "lioshare.h"
#include "winutil.h"
#include "const_util.h"
#include "molecerr.h"
#include "rawpage.h"
#include "paged_io.h"
//API tiers
#include "atomback.h"
#include "atom.h"
#include "update.h"
#include "recread.h"
#include "reccopy.h"
#include "bmset.h"
#include "reclist.h"
#include "dbctxt.h"
#include "dbstatus.h"
#include "dbf_tableb.h"
#include "dbf_tabled.h"
#include "dbf_data.h"
#include "dbf_index.h"
#include "dbf_field.h"
#include "dbf_rlt.h"
#include "dbf_ebm.h"
#include "btree.h"
#include "loaddiag.h"
#include "du1step.h"
#include "fastunload.h"
#include "fastload.h"
#include "page_f.h" //#include "page_F.h" : V2.24 case is less interchangeable on *NIX - Roger M.
#include "buffmgmt.h"
#include "checkpt.h"
#include "dbserv.h"
#include "seqfile.h"
#include "seqserv.h"
#include "cfr.h"
#include "core.h"
#include "msgroute.h"
#include "parmvr.h"
#include "parmref.h"
#include "statview.h"
//Diagnostics
#include "assert.h"
#include "except.h"
#include "msg_db.h"
#include "msg_core.h"
#include "msg_file.h"

namespace dpt {

Sharable DatabaseFile::sys_update_start_inhibitor;
Lockable DatabaseFile::chkmsg_info_lock;
ThreadSafeLong DatabaseFile::num_du_files = 0;

//******************************************************************************************
//This function is called by DatabaseServices::Allocate
//******************************************************************************************
DatabaseFile::DatabaseFile
(const std::string& parm_dd, std::string& parm_dsn, FileDisp disp, const std::string& alias)
: AllocatedFile(parm_dd, parm_dsn, FILETYPE_DB, alias),
  cfr_direct(NULL), cfr_index(NULL), cfr_updating(NULL), cfr_exists(NULL), cfr_recenq(NULL),
  fct_buff_page(NULL), buffapi(NULL), datamgr(NULL), indexmgr(NULL), fieldmgr(NULL),
  rltmgr(NULL), ebmmgr(NULL), tablebmgr(NULL), tabledmgr(NULL),
  du_flag(false), du_numlen(0), du_num_currmaxlen(0), du_format(0), du_1step_info(NULL)
{
	//Confirm that the extension is ".dpt".  This would be done already by the ALLOCATE
	//command when it decided on sequential vs. database, but in an API situation we 
	//still want to ensure it.  Lo-tech method (can't be bothered to getpath() etc).
	size_t dotpos = parm_dsn.rfind('.');
	std::string extension;
	if (dotpos != std::string::npos && dotpos != (parm_dsn.length() - 1))
		extension = parm_dsn.substr(dotpos+1);

	util::ToUpper(extension);
	if (extension != "DPT")
		throw Exception(DB_BAD_DSN, "Database file extensions must be '.dpt'");

	//99% of disk IO is buffered (some FCT writes are direct from this class though)
	buffapi = new BufferedFileInterface(this, parm_dd, parm_dsn, disp);
	
	std::string part = std::string("CFR_").append(parm_dd);
    cfr_direct = new CriticalFileResource(this, std::string(part).append("_DIRECT"));
    cfr_index = new CriticalFileResource(this, std::string(part).append("_INDEX"));
    cfr_updating = new CriticalFileResource(this, std::string(part).append("_UPDATING"));
    cfr_exists = new CriticalFileResource(this, std::string(part).append("_EXISTS"));
    cfr_recenq = new CriticalFileResource(this, std::string(part).append("_RECENQ"));

	//Sub-objects
	datamgr = new DatabaseFileDataManager(this);
	indexmgr = new DatabaseFileIndexManager(this);
	fieldmgr = new DatabaseFileFieldManager(this);
	rltmgr = new DatabaseFileRLTManager(this);
	ebmmgr = new DatabaseFileEBMManager(this);
	tablebmgr = new DatabaseFileTableBManager(this);
	tabledmgr = new DatabaseFileTableDManager(this);

	du_file_num.Clear();
	du_file_alpha.Clear();
}

//******************************************************************************************
//This function is called by DatabaseServices::Free, which will have ensured that the file
//was exclusively locked first.
//******************************************************************************************
DatabaseFile::~DatabaseFile() 
{
	DeleteOneStepDUInfo();
	CloseDUFiles(NULL);
	if (du_flag)
		num_du_files.Dec();

	if (buffapi) delete buffapi;

	if (cfr_direct) delete cfr_direct;
	if (cfr_index) delete cfr_index;
	if (cfr_updating) delete cfr_updating;
	if (cfr_exists) delete cfr_exists;
	if (cfr_recenq) delete cfr_recenq;

	if (datamgr) delete datamgr;
	if (indexmgr) delete indexmgr;
	if (fieldmgr) delete fieldmgr;
	if (rltmgr) delete rltmgr;
	if (ebmmgr) delete ebmmgr;
	if (tablebmgr) delete tablebmgr;
	if (tabledmgr) delete tabledmgr;
}

//********************************
void DatabaseFile::CloseDUFiles(DatabaseServices* info_dbapi)
{
	bool numclosed = false;
	bool alphaclosed = false;

	if (DUSFNum()) {
		DUSFNum()->Close();
		du_file_num.Clear();
		numclosed = true;
	}
	if (DUSFAlpha()) {
		DUSFAlpha()->Close();
		du_file_alpha.Clear();
		alphaclosed = true;
	}

	//V2.08 25 Oct 07.  Added mainly to be helpful in an API situation.
	if (info_dbapi && (numclosed || alphaclosed)) {
		info_dbapi->Core()->GetRouter()->Issue(DBA_LOAD_INFO, 
			"Info: Closed TAPEA and/or TAPEN which were open for writing deferred updates.");
	}
}

//********************************
//V2.14 Jan 09.
void DatabaseFile::DeleteOneStepDUInfo()
{
	if (du_1step_info) {
//		delete du_1step_info;
		DeferredUpdate1StepInfo::DestroyObject(du_1step_info); //V2.23
		du_1step_info = NULL;
	}
}

//********************************
FileHandle DatabaseFile::Construct
(const std::string& parm_dd, std::string& parm_dsn, FileDisp disp, const std::string& alias)
{
	//Lock across the *whole* constructor, in case derived part throws
	LockingSentry ls(&allocation_lock);

	DatabaseFile* f = new DatabaseFile(parm_dd, parm_dsn, disp, alias);

	//The file will now feature in the STATUS command
	FileStatusInfo::Attach(parm_dd, f);

	return FileHandle(f, BOOL_EXCL);
}

//********************************
void DatabaseFile::Destroy(DatabaseServices* dbapi, FileHandle& h)
{
	//A necessary consequence of the above
	LockingSentry ls(&allocation_lock);

	DatabaseFile* f = static_cast<DatabaseFile*>(h.GetFile());

	//Since the destructor can't be parameterized
	f->ReleaseFCT(dbapi);

	//This just means the STATUS command will now show "not allocated"
	FileStatusInfo::Detach(f->GetDDName());

	if (f->du_num_currmaxlen > 0)
		dbapi->Core()->GetRouter()->Issue(DBA_DEFERRED_UPDATE_INFO, 
		std::string("Longest ORD NUM index value deferred was length ")
		.append(util::IntToString(f->du_num_currmaxlen)));

	delete f;
}

//********************************
const std::string& DatabaseFile::FileName(DatabaseFileContext* c)
{
	//The parameter is here merely as a sanity check at programming time, so that
	//we don't try to use this function in situations where there is no context.
	assert(c);
	return GetDDName();
}

//********************************
SequentialFile* DatabaseFile::DUSFNum()
{
	return static_cast<SequentialFile*>(du_file_num.GetFile());
}
SequentialFile* DatabaseFile::DUSFAlpha()
{
	return static_cast<SequentialFile*>(du_file_alpha.GetFile());
}








//******************************************************************************************
void DatabaseFile::InitializeBuffers(int maxbuf, bool chkp_enabled) 
{
	BufferedFileInterface::Initialize(maxbuf, chkp_enabled);
}

//********************************
void DatabaseFile::ClosedownBuffers(DatabaseServices* dbapi) 
{
	BufferedFileInterface::Closedown(dbapi);
	FileStatusInfo::DestroyStatusInfo();
}








//******************************************************************************************
void DatabaseFile::Open(SingleDatabaseFileContext* context, 
const std::string& du_parm1, const std::string& du_parm2, int du_parm3, int du_parm4)
{
	//V2.14 Jan 09.  The parameters have slightly different meanings for:

	//A. Multistep mode
	std::string duname_n;
	std::string duname_a;
	int dunl;
	DUFormat duf;

	//B. Onestep mode
	bool onestepmode = false;
	//int du_1step_mempct;
	//int du_1step_diags;

	DatabaseServices* dbapi = context->DBAPI();
	MsgRouter* router = dbapi->Core()->GetRouter();

	//Do a few checks before we get into anything messy
	if (du_parm1.length() != 0 || du_parm2.length() != 0) {
		if (DatabaseServices::TBOIsOn())
			throw Exception(TXN_BACKOUT_BADSTATE, 
				"Transaction backout must be disabled in order to use deferred update mode");

		if (du_parm1 == "1STEPLOAD") {
			if (du_1step_info)
				throw Exception(DBA_DEFERRED_UPDATE_ERROR, 
					"The one-step deferred update process has already been initiated");

			onestepmode = true;

			//V2.19.  For single-step mode these are obsolete.  Use LOADMEMP/LOADDIAG parms now.
			//du_1step_mempct = du_parm3;
			//du_1step_diags = du_parm4;
			//if (du_1step_mempct < 5 || du_1step_mempct > 95)
			//	throw Exception(DB_BAD_PARM_MISC, "Max memory % must be from 5 to 95");

			//V3.0.  This changed to a warning, and moved into EnterOneStepDUMode()
			//if (dbapi->GetParmMAXBUF() > 1000) {
			//	throw Exception(DBA_DEFERRED_UPDATE_ERROR, 
			//		"One-step mode requires a small buffer pool (MAXBUF <= 1000)");
			//}
		}
		else {
			if (DUSFNum() || DUSFAlpha())
				throw Exception(DBA_DEFERRED_UPDATE_ERROR, 
					"The deferred update tape files have already been opened");

			duname_n = du_parm1;
			duname_a = du_parm2;
			dunl = du_parm3;
			duf = du_parm4;

			//V2.10.  The NOPAD option implies stringized numbers
			if (duf & DU_FORMAT_NOPAD)
				dunl = 0;
		}
	}

	TryForOpenLock(BOOL_SHR);

	//Opening a file often involves updating the FCT - clearing FISTAT flags etc.
	//Here I'm making the generalization that it ALWAYS will, just for code simplicity.
	//Hence opening a file always involves both physical disk input and output.
	//One drawback of this is that if there are "pre-open" errors like the FCT is
	//corrupt, or the file is empty, the backout can generate an alarming error message,
	//since backing out updates on a nonexistent FCT is not easy!
	StartNonBackoutableUpdate(context, false);

	bool firstopen = false;
	try {

		{ //null block - see later

		LockingSentry ls(&fistat_and_misc_lock);

		//Some things happen the first time any user opens a file.  
		//The main part of this is to load the FCT.
		if (!fct_buff_page) {
			firstopen = true;

			buffapi->ValidateFileSize();

			AcquireFCT(dbapi);

			//Just eyeball the FCT to check it looks like a database file
			FCTPage_F fctpage(dbapi, fct_buff_page);
			fctpage.EyeCheck();

			//If the FCT layout changes, old files will not work properly.  See page_f.cpp.
			if (fctpage.GetFicreate() != 'B')
				throw Exception(DB_BAD_FILE_CONTENTS, "FICREATE version mismatch - "
					"file must be recreated and reloaded");

			//V3.0. For the BLOB development there is no real reason to require file recreation,
			//as we can just initialize the required items if they are currently zero.
			if (fctpage.InitializeBLOBControlFieldsIfNeeded()) {
				router->Issue(DB_BLOBCTL_INITIALIZED, 
					"Info: First open with V3.xx - file has been configured to use BLOB fields");
			}

			//We only store the first few chars (currently 12) for this check
			std::string abbrev = fctpage.FormatOSFileName(GetDSN());
			std::string oldosname = fctpage.GetLastOSFileName();

			//V2.27: Don't issue this message just for different case given on allocate call
			util::ToUpper(oldosname);
			util::ToUpper(abbrev);

			if (oldosname != abbrev) {
				router->Issue(DB_OSFILE_RENAMED, 
					std::string("Info: OS file has been renamed from ")
					.append(oldosname).append(" since it was last used by DPT"));
				fctpage.SetLastOSFileName(abbrev);
			}

			buffapi->SetSeqopt(fctpage.GetSeqopt());

			//Deferred update mode remaining from before free/allocate
			if (fctpage.GetFistat() & FISTAT_DEFERRED_UPDATES) {
				du_flag = true;
				du_numlen = fctpage.GetDULen();
				du_format = fctpage.GetDUFormat();
				num_du_files.Inc();
			}
		}

		//----------------------------------------------------
		//This stuff happens when anybody opens the file
		FCTPage_F fctpage(dbapi, fct_buff_page);

		//This is a rare case where we use the Core output destination directly
		std::string msg = fctpage.GetBroadcastMessage();
		if (msg.length() > 0) {
			util::LineOutput* op = dbapi->Core()->Output();
			op->WriteLine(std::string("Message from file ").append(GetDDName()));
			op->WriteLine(msg);
		}

		//Report FISTAT and FIFLAGS settings
		unsigned char fistat = fctpage.GetFistat();
		unsigned char fiflags = fctpage.GetFiflags();

		if (fistat & FISTAT_NOT_INIT)
			router->Issue(DB_FISTAT_OPEN_MSG, "File is not initialized");

		//This is nonstandard, so convert it into a M204 value at this point
		if (fiflags & FIFLAGS_PAGE_BROKEN) {

			//V2.23.  Often a file will be page broken if other users are updating the file,
			//so only convert to FISTAT physically broken if this is first user opening.
			if (firstopen) {
				fctpage.OrFistat(FISTAT_PHYS_BROKEN);
				fistat = fctpage.GetFistat();
				fctpage.NandFiflags(FIFLAGS_PAGE_BROKEN);
				fiflags = fctpage.GetFiflags();
			}
		}
		if (fistat & FISTAT_PHYS_BROKEN)
			router->Issue(DB_FISTAT_OPEN_MSG, "File is physically inconsistent");

		//Unlike M204 give some more info here to save them having to check FIFLAGS
		if (fistat & FISTAT_FULL) {
			std::string msg("File is full");
			const char* reason = "";
			if (fiflags & FIFLAGS_FULL_TABLEB)
				reason = " (table B)";
			else if (fiflags & FIFLAGS_FULL_TABLED)
				reason = " (table D)";
			router->Issue(DB_FISTAT_OPEN_MSG, msg);
		}

		//M204 does give the extra info here - so do we
		if (fistat & FISTAT_RECOVERED) {
			router->Issue(DB_FISTAT_OPEN_MSG, "File has been recovered");

			std::string msg("Rollback was to checkpoint of: ");
			msg.append(win::GetCTime(fctpage.GetRollbackCPTime()));
			router->Issue(DB_FISTAT_OPEN_MSG, msg);

			msg = "Last transaction was at: ";
			msg.append(win::GetCTime(fctpage.GetLastTxnTime()));
			router->Issue(DB_FISTAT_OPEN_MSG, msg);
		}

		if (fistat & FISTAT_LOG_BROKEN)
			router->Issue(DB_FISTAT_OPEN_MSG, "File may be logically inconsistent");

		if (fistat & FISTAT_DEFERRED_UPDATES) {
			//V2.14 Jan 09
			if (du_1step_info)
				msg = "File is in one-step deferred update mode";
			else
				//This can persist across free/allocate - see big chunk later
				msg = "File is in deferred update mode";
			router->Issue(DB_FISTAT_OPEN_MSG, msg);
		}

		//A custom bit, similar to the above
		if (fiflags & FIFLAGS_REORGING)
			router->Issue(DB_FISTAT_OPEN_MSG, "File is being reorganized");

		//As on M204 we reset all unused bits, which means that in fact opening a file
		//will always update it, for this reason if none of the above.  There is no real
		//reason these bits should be set as the user can't do it, but clear them anyway!
		unsigned char unuseds = FISTAT_UNUSED_X80 | FISTAT_UNUSED_X04;
		fctpage.NandFistat(unuseds);

		//-------------------------------------------------------------------------------
		//Open the deferred update destinations.
		if (du_parm1.length() != 0 || du_parm2.length() != 0) {

			//V2.15
			if (fistat & FISTAT_NOT_INIT)
				throw Exception(DB_FILE_STATUS_DEFERRED, 
					"Can't activate deferred update mode for an uninitialized file");

			//V2.15 Ensure nobody else is currently making index updates
			CFRSentry cs(dbapi, cfr_index, BOOL_EXCL);

			//V2.14.  Jan 09.  We can do deffered updates in memory now.
			if (onestepmode)
				EnterOneStepDUMode(dbapi, LOADDIAG_VERBOSE);

			//The original "full" multistep mode.
			else {

				//Check consistency with any DU info left from last open of the file
				if ( (fctpage.GetDULen() != 0 && fctpage.GetDULen() != dunl)
					//V2.10:
					|| 
					(fctpage.GetDUFormat() != 0 && fctpage.GetDUFormat() != duf) )
				{
					throw Exception(DB_FILE_STATUS_DEFERRED, 
						"Deferred update length/options do not match those used last time");
				}

				//Tape for ORD NUM field updates
				du_file_num = AllocatedFile::FindAllocatedFile(duname_n, BOOL_SHR, FILETYPE_SEQ);
				if (!DUSFNum())
					throw Exception(SYSFILE_NOT_ALLOCATED, std::string
						("Sequential file is not allocated: ").append(duname_n));

				try {
					//Depending on the allocation mode we may or may not append.  The second
					//parm here says that whoever does the initial open, anyone can free.
					dupos_num = DUSFNum()->Open(BOOL_EXCL, true);
					du_numlen = dunl;
				}
				catch (...) {
					du_file_num.Clear();
					throw;
				}

				//Tape for ORD CHAR field updates
				du_file_alpha = AllocatedFile::FindAllocatedFile(duname_a, BOOL_SHR, FILETYPE_SEQ);
				if (!DUSFAlpha()) {
					CloseDUFiles(NULL);
					throw Exception(SYSFILE_NOT_ALLOCATED, std::string
						("Sequential file is not allocated: ").append(duname_a));
				}

				try {
					dupos_alpha = DUSFAlpha()->Open(BOOL_EXCL, true);
				}
				catch (...) {
					du_file_alpha.Clear();
					CloseDUFiles(NULL);
					throw;
				}

				//Info message
				if (du_flag)
					router->Issue(DB_FISTAT_OPEN_MSG, "Deferred update tape files attached");
				else {
					du_flag = true;
					num_du_files.Inc();
					fctpage.OrFistat(FISTAT_DEFERRED_UPDATES);
					router->Issue(DB_FISTAT_OPEN_MSG, "File is now in deferred update mode");
				}

				du_numlen = dunl;
				fctpage.SetDULen(dunl);
				du_format = duf;
				fctpage.SetDUFormat(duf);
			}
		}

		//Fresh block because commit needs the fistat lock
		}
		{ 

		//Commit here otherwise we will be inhibiting checkpoints just by opening the
		//file.  It's not a "significant" update though.
		OperationDelimitingCommit(dbapi);

		}
	}
	catch (Exception& e) {
		if (firstopen)
			ReleaseFCT(dbapi);

		//Make dead sure this is released
		try {dbapi->Backout(true);}
		catch (...) {}
		try {cfr_updating->Release();}  //unnecessary if backout worked, but may not
		catch (...) {}

		ReleaseOpenLock();
		throw Exception(DB_OPEN_FAILED, std::string("Serious open error: ")
			.append(e.What()));
	}
	catch (...) {
		if (firstopen)
			ReleaseFCT(dbapi);

		//Make dead sure this is released
		try {dbapi->Backout(true);}
		catch (...) {}
		try {cfr_updating->Release();}  //unnecessary if backout worked, but may not
		catch (...) {}

		ReleaseOpenLock();
		throw Exception(DB_OPEN_FAILED, "Serious open error: (unknown reason)");
	}
}

//********************************
void DatabaseFile::Close(SingleDatabaseFileContext* context)
{
	DatabaseServices* dbapi = context->DBAPI();

	ReleaseOpenLock();

	//When the last user closes the file release the FCT and audit file close stats
	try {
		//If other users have the file open, this throws and we don't do final close.
		FileOpenLockSentry fols(this, BOOL_EXCL, false);

		//V2.14 Jan 09.  Last user to close the file flushes 1-step deferred updates.
		if (du_1step_info)
			ApplyFinalOneStepDUInfo(context);

		ReleaseFCT(dbapi);
		
		StatViewer* sv = dbapi->Core()->GetStatViewer();
		std::string statline = sv->UnformattedLine(STATLEVEL_FILE_CLOSE, this);

		std::string auditline("$$$ FILE='");
		auditline.append(GetDDName()).append(1, '\'');
		dbapi->Core()->AuditLine(auditline, "STT");
		dbapi->Core()->AuditLine(statline, "STT");

		//Clear stats, since this object remains in existence till it's opened again
		stat_backouts.Clear();
		stat_commits.Clear();
		stat_updttime.Clear();

		stat_badd.Clear();
		stat_bchg.Clear();
		stat_bdel.Clear();
		stat_bxdel.Clear();
		stat_bxfind.Clear();
		stat_bxfree.Clear();
		stat_bxinse.Clear();
		stat_bxnext.Clear();
		stat_bxrfnd.Clear();
		stat_bxspli.Clear();
		stat_dirrcd.Clear();
		stat_finds.Clear();
		stat_recadd.Clear();
		stat_recdel.Clear();
		stat_recds.Clear();
		stat_strecds.Clear();

		stat_dkpr.Clear();
		stat_dkrd.Clear();
		stat_dkwr.Clear();
		stat_fbwt.Clear();
		stat_dksfbs.Clear();
		stat_dksfnu.Clear();
		stat_dkskip.Clear();
		stat_dkskipt.Clear();
		stat_dkswait.Clear();
		stat_dkuptime.Clear();

		stat_ilmradd.Clear();
		stat_ilmrdel.Clear();
		stat_ilmrmove.Clear();
		stat_ilradd.Clear();
		stat_ilrdel.Clear();
		stat_ilsadd.Clear();
		stat_ilsdel.Clear();
		stat_ilsmove.Clear();

		stat_mrgvals.Clear();
	}
	catch (Exception& e) {
		//Lock failed is fine - it just means someebody else has the file open
		if (e.Code() != SYSFILE_OPEN_FAILED)
			//Poss this should throw - I'm scared to change it now!
			dbapi->Core()->GetRouter()->Issue(e.Code(), e.What());
	}
}

//********************************
void FileOpenLockSentry::Get(DatabaseFile* f, bool new_lock, bool ps) 
{
	//This does an atomic upgrade attempt if necessary
	f->TryForOpenLock(new_lock, ps);

	file = f;
	prev_shr = ps;

}

//********************************
FileOpenLockSentry::~FileOpenLockSentry()
{
	if (!file)
		return;

	if (prev_shr)
		//Atomic downgrade - shouldn't fail if the caller is doing things right
		file->TryForOpenLock(BOOL_SHR, true);
	else
		file->ReleaseOpenLock();
}

//********************************
bool DatabaseFile::AnybodyHasOpen()
{
	try {
		FileOpenLockSentry fols(this, BOOL_EXCL, false);
		return false;
	}
	catch (...) {
		return true;
	}
}







//******************************************************************************************
void DatabaseFile::CheckFileStatus
//V2.19 Jun 09.  Files will never remain in a "mid reorg" state.
//(bool chk_full, bool chk_notinit, bool chk_broken, bool chk_reorging, bool chk_deferred)
(bool chk_full, bool chk_notinit, bool chk_broken, bool chk_deferred)
{
	LockingSentry ls(&fistat_and_misc_lock);

	//Only reading - null api pointer is OK
	FCTPage_F fctpage(NULL, fct_buff_page);

	if (chk_full) {
		if (fctpage.GetFistat() & FISTAT_FULL)
			throw Exception(DB_FILE_STATUS_FULL, 
				"Requested operation invalid - file is marked as full");
	}
	if (chk_notinit) {
		if (fctpage.GetFistat() & FISTAT_NOT_INIT)
			throw Exception(DB_FILE_STATUS_NOTINIT, 
				"Requested operation invalid - file is not initialized");
	}
	if (chk_broken) {
		if (fctpage.GetFistat() & FISTAT_PHYS_BROKEN)
			throw Exception(DB_FILE_STATUS_BROKEN, 
				"Requested operation invalid - file is physically inconsistent");
	}
//	if (chk_reorging) {
//		if (fctpage.GetFiflags() & FIFLAGS_REORGING)
//			throw Exception(DB_FILE_STATUS_REORGING, 
//				"Requested operation invalid - file is in the middle of a reorg");
//	}
	if (chk_deferred) {
		if (fctpage.GetFistat() & FISTAT_DEFERRED_UPDATES)
			throw Exception(DB_FILE_STATUS_DEFERRED, 
				"Requested operation invalid - file is in deferred update mode");
	}
}








//******************************************************************************************
//Interface functions into the FCT
//******************************************************************************************
bool DatabaseFile::SetFistat(DatabaseServices* dbapi, unsigned char mask, bool flag)
{
	//V2.27.  This crops up trying to open a blank/otherwise unopenable file
	if (!fct_buff_page)
		return false;

	LockingSentry ls(&fistat_and_misc_lock);

	FCTPage_F fctpage(dbapi, fct_buff_page);

	//No need to set bits again
	bool current = (fctpage.GetFistat() & mask) ? true : false;
	if (flag == current)
		return false;

	if (flag)
		fctpage.OrFistat(mask);
	else
		fctpage.NandFistat(mask);

	return true;
}

//********************************
bool DatabaseFile::SetFiflags(DatabaseServices* dbapi, unsigned char mask, bool flag)
{
	//V2.27.  This crops up trying to open a blank/otherwise unopenable file
	if (!fct_buff_page)
		return false;

	LockingSentry ls(&fistat_and_misc_lock);

	FCTPage_F fctpage(dbapi, fct_buff_page);

	//The page-broken flag is written directly to disk now, as the whole purpose
	//of the flag is to reflect a physical condition of the file.  
	bool write_now = (mask == FIFLAGS_PAGE_BROKEN);

	//No need to set bits again
	bool current = (fctpage.GetFiflags() & mask) ? true : false;
	if (flag == current)
		return false;

	//Note that we still mark the page dirty when setting the flag, but not when 
	//clearing it.  The reason is to get the FCT page checkpoint logged, so that 
	//roll back will work OK and unbreak the file if required.
	if (flag)
		fctpage.OrFiflags(mask);
	else
		fctpage.NandFiflags(mask, write_now);

	if (write_now)
		buffapi->PhysicalPageWrite(dbapi, 0, fct_buff_page->PageData());

	return true;
}

//********************************
bool DatabaseFile::MarkPhysicallyBroken(DatabaseServices* d, bool b) {
	return SetFistat(d, FISTAT_PHYS_BROKEN, b);}

bool DatabaseFile::MarkLogicallyBroken(DatabaseServices* d, bool b) {
	return SetFistat(d, FISTAT_LOG_BROKEN, b);}

bool DatabaseFile::MarkPageBroken(DatabaseServices* d, bool b) {
	return SetFiflags(d, FIFLAGS_PAGE_BROKEN, b);}

void DatabaseFile::MarkTableBFull(DatabaseServices* d) 
{
	d->Core()->GetRouter()->Issue(DB_INSUFFICIENT_SPACE, 
		std::string("Table B full in file ").append(GetDDName()));

	SetFistat(d, FISTAT_FULL, true); 
	SetFiflags(d, FIFLAGS_FULL_TABLEB, true);
}

//********************************
void DatabaseFile::MarkTableDFull(DatabaseServices* d) 
{
	d->Core()->GetRouter()->Issue(DB_INSUFFICIENT_SPACE, 
		std::string("Table D full in file ").append(GetDDName()));

	SetFistat(d, FISTAT_FULL, true); 
	SetFiflags(d, FIFLAGS_FULL_TABLED, true);
}

//********************************
void DatabaseFile::SetLastTransactionTime(DatabaseServices* dbapi)
{
	//This is for informational purposes only really - does not currently participate
	//in recovery, although it might do it we ever implement roll forward.
	LockingSentry ls(&fistat_and_misc_lock);
	FCTPage_F fctpage(dbapi, fct_buff_page);
	fctpage.SetLastTransactionTime();
}

//********************************
void DatabaseFile::RollbackSetPostInfo(DatabaseServices* dbapi, time_t rb_cptimestamp)
{
	//Since the FCT will usually have been rolled back - ensure the preimage is used
	ReleaseFCT(dbapi);
	buffapi->FreeFilePages();

	AcquireFCT(dbapi);

	FCTPage_F fctpage(dbapi, fct_buff_page);

	fctpage.SetRollbackCPTime(rb_cptimestamp);
	fctpage.OrFistat(FISTAT_RECOVERED, true);

	//Written straight back since no transaction-based control during recovery
	buffapi->PhysicalPageWrite(dbapi, 0, fct_buff_page->PageData());
	ReleaseFCT(dbapi);
}

//********************************
time_t DatabaseFile::RollbackGetLastCPTime(DatabaseServices* dbapi)
{
	AcquireFCT(dbapi);

	FCTPage_F fctpage(dbapi, fct_buff_page);

	time_t lastcp = fctpage.GetLastCPTime();

	ReleaseFCT(dbapi);
	return lastcp;
}

//********************************
bool DatabaseFile::IsRRN()
{
	//Just reading - null api pointer is OK
	FCTPage_F fctpage(NULL, fct_buff_page);
	return (fctpage.GetFileorg() & FILEORG_RRN) ? true : false;
}

//********************************
void DatabaseFile::ReleaseFCT(DatabaseServices* dbapi)
{
	if (fct_buff_page) {
		BufferPage::Release(dbapi, fct_buff_page);
		fct_buff_page = NULL;
	}
}

//********************************
void DatabaseFile::AcquireFCT(DatabaseServices* dbapi)
{
	if (!fct_buff_page)
		fct_buff_page = BufferPage::Request(dbapi, buffapi, 0, false);
	CacheParms();
}

//********************************
void DatabaseFile::CacheParms()
{
	//Read only so null api parm is OK
	FCTPage_F fctpage(NULL, fct_buff_page);

	cached_bsize = fctpage.GetBsize();
	cached_dsize = fctpage.GetDsize();

	tabledmgr->CacheParms();
	tablebmgr->CacheParms();
//	indexmgr->CacheParms();
}










//**********************************************************************************************
//DBA stuff
//**********************************************************************************************
void DatabaseFile::Create(DatabaseServices* dbapi,
int bsize, int brecppg, int breserve, int breuse, 
int dsize, int dreserve, int dpgsres, int fileorg)
{
	ValidateFileName(GetDDName());

	//Use default/max/min values for parameters if appropriate. 
//* * * 
	//NB.  BSIZE and DSIZE are limited to 2G on Baby204 because of widespread careless
	//use of int instead of unsigned int in various places before I reached this point.
	//Here's a good example, where the parameter validation infrastructure holds
	//max and min values as signed int values.
//* * * 
	bsize		= ValidateCreateParmValue(dbapi, "BSIZE", bsize);
	brecppg		= ValidateCreateParmValue(dbapi, "BRECPPG", brecppg);
	breserve	= ValidateCreateParmValue(dbapi, "BRESERVE", breserve);
	breuse		= ValidateCreateParmValue(dbapi, "BREUSE", breuse);
	dsize		= ValidateCreateParmValue(dbapi, "DSIZE", dsize);
	dreserve	= ValidateCreateParmValue(dbapi, "DRESERVE", dreserve);
	fileorg		= ValidateCreateParmValue(dbapi, "FILEORG", fileorg);

	//Other checks apart from simple max or min as per above
	_int64 ds = dsize;
	_int64 bs = bsize;
	if (bs + ds > (INT_MAX - 1)) //see FCT/LPM code for why -1
		throw Exception(DB_BAD_CREATE_PARM, "DSIZE + BSIZE must not exceed 2G pages");

	//V3.01. Some time in 2011.
	//_int64 br = brecppg;
	//if (bs * br > INT_MAX)
	//	throw Exception(DB_BAD_CREATE_PARM, "BSIZE * BRECPPG must not exceed 2G records");
	tablebmgr->ValidateBsize(dbapi, bs, brecppg);

	//The default DPGSRES is calculated from DSIZE
	if (dpgsres != -1) {
		dpgsres = ValidateCreateParmValue(dbapi, "DPGSRES", dpgsres);
		if (dpgsres >= dsize)
			throw Exception(DB_BAD_CREATE_PARM, "DPGSRES must be less than DSIZE");
	}
	else {
		dpgsres = dsize/50 + 2;
		if (dpgsres > 20)
			dpgsres = 20;
	}

	if (fileorg != 0 && fileorg != 0x24)
		throw Exception(DB_BAD_CREATE_PARM, "FILEORG can only be 0 or 36 (0x24)");

	//Create a fresh (currently "floating") FCT page with these parameters
	AlignedPage ap;
	FCTPage_F pf(ap.GetRawPageData(), bsize, brecppg, breserve, breuse, 
		dsize, dreserve, dpgsres, fileorg, GetDSN());

	dbapi->Core()->GetStatViewer()->StartActivity("CREA");

	DeleteOneStepDUInfo();
	CloseDUFiles(dbapi);
	if (du_flag) {
		num_du_files.Dec();
		du_flag = false;
		du_numlen = 0;
		du_format = 0;
	}

	//Here's where the disk file gets altered
	buffapi->DirectCreateFile(dbapi, ap.GetRawPageData(), 1 + bsize + dsize);

	//Destroy any cached info
	fieldmgr->Initialize(NULL, false);

	//Release the old buffered FCT and replace it with the one just written out above
	ReleaseFCT(dbapi);
	buffapi->FreeFilePages();
	AcquireFCT(dbapi);

	dbapi->Core()->GetRouter()->Issue(DB_FILE_CREATED, std::string("File ")
		.append(GetDDName()).append(" created"));

	dbapi->Core()->GetStatViewer()->EndActivity();
}

//**********************************************************************************************
//Called by the above, but also by the CREATE command to give error before parms are entered
void DatabaseFile::ValidateFileName(const std::string& filename)
{
	if (filename.length() == 0) 
		throw Exception(DB_BAD_DD, "No file name given");

	bool invalid = false;

	//Rules as specified in the M204 file manager's guide.  
/* * * Note 
As things currently stand you can rename the OS file to anything and then you are only
limited on re-allocation by what the allocate routines will allow.  I'm leaving this
as it is for now, as perhaps there might be times when you would want to kluge in
a file with one of these names and see what happens.  CCASYS emulation or something.
With some of them like obviously FILE and OUTxxx it will cause unpredictable behaviour
in the command parsers, but you would have to accept that.
See also comments in AllocatedFile::AllocatedFile(). 
* * */	
	if (filename.length() > 8)
		invalid = true;
	if (filename == "FILE" || filename == "GROUP")
		invalid = true;
	else if (filename.length() >=4 && filename.substr(0, 4) == "TAPE")
		invalid = true;
	else if (filename.length() >=3 && util::OneOf(filename.substr(0, 3), "CCA/SYS/OUT", '/'))
		invalid = true;
	else if (!strchr(BB_CAPITAL_LETTERS, filename[0]))
		invalid = true;
	else if (strspn(filename.c_str(), BB_CAPITAL_LETTERS BB_DIGITS "@#$") != filename.length())
		invalid = true;

	if (invalid)
		throw Exception(DB_BAD_DD, "Invalid name for a database file");
}

//********************************
int DatabaseFile::ValidateCreateParmValue
(DatabaseServices* dbapi, const std::string& parmname, int val)
{
	ParmRefInfo pref = dbapi->Core()->GetViewerResetter()->GetRefTable()->GetRefInfo(parmname);

	//Take default?
	if (val == -1)
		return util::StringToInt(pref.default_value);
		
	MsgRouter* router = dbapi->Core()->GetRouter();

	//Use max or min?
	if (val > pref.maximum_value) {
		router->Issue(PARM_USEDMAXORMIN, std::string
					("Maximum parameter value used for ").append(parmname));
		return pref.maximum_value;
	}
		
	if (val < pref.minimum_value) {
		router->Issue(PARM_USEDMAXORMIN, std::string
					("Minimum parameter value used for ").append(parmname));
		return pref.minimum_value;
	}

	return val;		
}

//********************************
void DatabaseFile::Initialize
(SingleDatabaseFileContext* context, bool leave_fields, bool reorging)
{
	DatabaseServices* dbapi = context->DBAPI();

	//Added this later to workaround a nasty bug that needs fixing really.  Since table D
	//would move up the file if table B is currently fragmented, the field defs would need 
	//to move up too.  Note also that if table D is fragmented it's possible that defragging
	//table D could mess them up too.  It wouldn't be too hard to fix this but I don't
	//fancy it right now, hence this message.
	FCTPage_F oldfct(dbapi, fct_buff_page);
	if (leave_fields && (oldfct.MultiBExtents() || oldfct.MultiDExtents()) )
		throw Exception(DB_FILE_CREATE_FAILED, 
			"The NOFLD/KEEPDEDFS option is currently disallowed "
			"if there is table B/D fragmentation (which there is)");

	//Ensure we have the file in EXCL (will downgrade back to SHR on exit)
	//V3.0. Take this across the whole reorg operation.
	//FileOpenLockSentry fols(this, BOOL_EXCL, true);
	FileOpenLockSentry fols;
	if (!reorging) {
	
		//See doc
		OperationDelimitingCommit(dbapi);

		fols.Get(this, BOOL_EXCL, true);
	}

	dbapi->Core()->GetStatViewer()->StartActivity("INIT");

	//Since we will be invalidating pretty much the whole file.  Note that we don't
	//actually need to checkpoint for INITIALIZE, so doing this will probably mean
	//some pages end up getting CP logged twice, but no problem.  See recovery.cpp.
	ReleaseFCT(dbapi);
	buffapi->FreeFilePages();
	AcquireFCT(dbapi);

	if (!reorging)
		StartNonBackoutableUpdate(context);

	try {
		FCTPage_F fctpage(dbapi, fct_buff_page);

		//This will either clear existing fields or just defrag the fatt page chain
		int atrpg = fieldmgr->Initialize(context, leave_fields);

		//Clear the FCT
		fctpage.InitializeValues(atrpg);

		//Fistat is by deinition zero after initialize
		fctpage.NandFistat(0xFF);

		DeleteOneStepDUInfo();
		CloseDUFiles(dbapi);
		if (du_flag) {
			num_du_files.Dec();
			du_flag = false;
			du_numlen = 0;
			du_format = 0;
		}

		dbapi->Core()->GetRouter()->Issue(DB_FILE_INITIALIZED, std::string("File ")
			.append(context->GetShortName()).append(" initialized")
			.append( (leave_fields && atrpg > 0) ? " (field definitions remain)" : ""));

		//Commit - see doc
		if (!reorging)
			dbapi->GetUU()->EndOfMolecule(true);

		dbapi->Core()->GetStatViewer()->EndActivity();
	}
	MOLECULE_CATCH(this, dbapi->GetUU());
}

//********************************
void DatabaseFile::Increase(SingleDatabaseFileContext* context, int amount, bool tabled)
{
	CheckFileStatus(false, false, true, false);

	DatabaseServices* dbapi = context->DBAPI();

	//V3.01. Some time in 2011.  This can throw before the update starts.
	if (!tabled) {
		_int64 newbsize = cached_bsize;
		newbsize += amount;
		tablebmgr->ValidateBsize(dbapi, cached_bsize + amount);
	}

	StartNonBackoutableUpdate(context);

	//The LPM is the only thing that can impinge on the broadcast message
	LockingSentry ls(&broadcast_message_lock);
	FCTPage_F fctpage(dbapi, fct_buff_page);

	//Try and resize the file first - this is the same whichever table is being done
	int oldsize = 1 + cached_bsize + cached_dsize;
	buffapi->pagedfile->SetSize(oldsize + amount);

	if (!fctpage.Increase(amount, tabled)) {
		//Resize the file back if we can
		buffapi->pagedfile->SetSize(oldsize);
		throw Exception(DB_STRUCTURE_BUG, 
			"Too many alternating INCREASE B/D commands - the FCT (LPM) is full");
	}

	CacheParms();
}

//********************************
void DatabaseFile::ShowTableExtents
(SingleDatabaseFileContext* context, std::vector<int>* result)
{
	CheckFileStatus(false, false, true, false);

	DatabaseServices* dbapi = context->DBAPI();

	LockingSentry ls(&broadcast_message_lock);
	FCTPage_F fctpage(dbapi, fct_buff_page);
	fctpage.ShowTableExtents(result);
}








//*************************************************************************************************
//All to do with deferred update processing:
//*************************************************************************************************

//********************************
void DatabaseFile::WriteDeferredUpdateRecord
(DatabaseServices* dbapi, int recnum, short fid, const FieldValue& ixval)
{
	//This can happen normally if a file is closed and then reopened with no tapes.
	if (!DUSFAlpha() || !DUSFNum())
		throw Exception(DB_FILE_STATUS_DEFERRED, 
			"Update error: deferred update tape files are missing");

	//Use this setting if index updates will be applied in some other custom fashion
	if (du_format & DU_FORMAT_DISCARD)
		return;

	if (ixval.CurrentlyNumeric()) {

		//Numeric, fixed length, everything in internal representation
		if (du_numlen == -1) {
			char buff[14];
			*(reinterpret_cast<int*>(buff)) = recnum;
			*(reinterpret_cast<short*>(buff+4)) = fid;
			*(reinterpret_cast<RoundedDouble*>(buff+6)) = ixval.ExtractRoundedDouble();

			dupos_num += DUSFNum()->BaseIO()->WriteNoCRLF(dupos_num, buff, 14);
		}

		//Numeric, stringized
		else {

			//V2.03.  As per ORD CHAR
//			record = util::SpacePad(recnum, 10, true);
//			record.append(util::SpacePad(fid, 5, true));

			std::string sval = ixval.ExtractString();

			//V2.10. Padding to fixed length is now optional
			std::string paddedval;
			if (du_format & DU_FORMAT_NOPAD) {
				paddedval = sval;
			}
			else {
				if (sval.length() > (size_t) du_numlen)
					throw Exception(DML_INVALID_DATA_VALUE, std::string(
						"Update error: numeric field expanded value "
						"is longer than the specified maximum: ").append(sval));

				paddedval = util::PadLeft(sval, ' ', du_numlen);
			}

			dupos_num += WriteVariableLengthDURecord(true, recnum, fid, dupos_num, paddedval);

			if (sval.length() > (size_t) du_num_currmaxlen)
				du_num_currmaxlen = sval.length();
		}
	}

	//ORD CHAR entries are always variable length
	else {
		//V2.03.  Save some sort IO by reducing this from 15 to 8 bytes.  The reason
		//to use literals was to avoid CRLF, but we can tweak to avoid that.
//		record = util::SpacePad(recnum, 10, true);
//		record.append(util::SpacePad(fid, 5, true));

		std::string sval = ixval.ExtractString();
		dupos_alpha += WriteVariableLengthDURecord(false, recnum, fid, dupos_alpha, sval);
	}

	//I think do this even though it may not be "line" IO in all cases.
	//There is otherwise no stat anywhere to show how many updates were deferred.
	dbapi->Core()->IncStatSEQO();
}

//********************************
//This applies for ORD CHAR values or stringized ord num values.  As of V2.10 the
//latter are not necessarily right-aligned to a fixed column width.
int DatabaseFile::WriteVariableLengthDURecord
(bool num, int recnum, short fid, _int64 fileoffset, const std::string& fieldval)
{
	SequentialFile* dufile = (num) ? DUSFNum() : DUSFAlpha();

	std::string record;

	//V2.10.  CRLF-terminated format is now optional, but it means we have to insert
	//the value length like in table B.  There is 4+2+1 bytes of data either way though
	//(rec# + FID + either value length or CRLF-kluge control byte).
	record.reserve(fieldval.length() + 7);

	if (du_format & DU_FORMAT_NOCRLF) {

		//Just use rec# and FID raw.  Characters 0x0d/0x0a are irrelevant here (cf below).
		record.append(reinterpret_cast<const char*>(&recnum), 4);
		record.append(reinterpret_cast<const char*>(&fid), 2);

		//Value length byte
		unsigned char vlen = fieldval.length();
		record.append(1, vlen);

		//Then the value
		record.append(fieldval);

		return dufile->BaseIO()->WriteNoCRLF(fileoffset, record.c_str(), record.length());
	}

	else {
		//Here we do have to tweak the field ID and record number so no byte is CR or LF.
		//The exact values don't matter during the sort, just that they are distinct.
		//First record number - uses a special control byte.
		unsigned char rbuff[5];
		rbuff[0] = 0;
		*(reinterpret_cast<int*>(rbuff+1)) = recnum;
		util::EncodeNoCRLFValue(rbuff, 5);
		record.append(reinterpret_cast<const char*>(rbuff), 5);

		//Can't do the FID in the same way though, as e.g. FID 10 could get mixed with 11.
		//The max fid is 4000, so we have 4 bits to play with and can keep it to 2 bytes.
		//We just have to preserve uniqueness while avoiding x0D and x0A.
		unsigned char fbuff[2];
		*(reinterpret_cast<short*>(fbuff)) = fid;

		//Move x08 bit in both bytes
		fbuff[1] &= '\x0F';			//make sure high half of high byte is clear
		if (fbuff[1] & '\x08') {	//if high byte has x08
			fbuff[1] &= '\xF7';		//turn it off
			fbuff[1] |= '\x80';		//and set first bit in the clear part
		}
		if (fbuff[0] & '\x08') {	//if low byte has x08
			fbuff[0] &= '\xF7';		//turn it off
			fbuff[1] |= '\x40';		//and set second but in the clear part
		}
		record.append(reinterpret_cast<const char*>(fbuff), 2);

		//The value must not contain CR/LF either - just give up if it does.  See
		//Roger M correspondence for theoretical kluge schemes that might have been used.
		if (fieldval.find_first_of("\n\r") != std::string::npos) {
			throw Exception(DML_INVALID_DATA_VALUE, 
				"Update error: string fields cannot contain "
				"CR/LF characters in deferred update mode (without NOCRLF option)");
		}

		record.append(fieldval);
		return dufile->BaseIO()->WriteLine(fileoffset, record.c_str(), record.length());
	}
}

//********************************
_int64 DatabaseFile::ApplyDeferredUpdates(SingleDatabaseFileContext* context, int forgivingness) 
{
	CheckFileStatus(true, true, true, false);

	//V2.14 Jan 09.
	if (du_1step_info)
		throw Exception(DB_FILE_STATUS_DEFERRED, 
			"One-step mode: deferred index updates will be applied when the last user closes the file");

	//Ensure we have the file in EXCL (will downgrade back to SHR on exit).  Actually
	//CFR_INDEX would be sufficient, but I think M204 does it like this.
	FileOpenLockSentry fols(this, BOOL_EXCL, true);

	DatabaseServices* dbapi = context->DBAPI();

	FCTPage_F fctpage(dbapi, fct_buff_page);

	if (! (fctpage.GetFistat() & FISTAT_DEFERRED_UPDATES) )
		throw Exception(DB_FILE_STATUS_DEFERRED, 
			"The file is not in deferred update mode");

	//Generally this would happen when the system is closed to do the sort, but we
	//might want to just load without sort in the same run if there are few records.
	CloseDUFiles(dbapi);

	SequentialFileServices* seqserv = dbapi->SeqFiles();
	SequentialFileView* tapen = NULL;
	SequentialFileView* tapea = NULL;

	try {
		tapen = seqserv->OpenSeqFile("TAPEN", BOOL_SHR);
		tapea = seqserv->OpenSeqFile("TAPEA", BOOL_SHR);
	}
	catch (...) {
		if (tapen)
			seqserv->CloseSeqFile(tapen);
		if (tapea)
			seqserv->CloseSeqFile(tapea);
		throw;
	}

	_int64 du_recs = 0;
	try {
		try {
			OperationDelimitingCommit(dbapi);
			StartNonBackoutableUpdate(context, true);

			dbapi->Core()->GetStatViewer()->StartActivity("BLDX");

			int forgiven = 0;
			du_recs += ApplyDUFile(context, tapen, true, forgivingness, forgiven);
			du_recs += ApplyDUFile(context, tapea, false, forgivingness, forgiven);

			seqserv->CloseSeqFile(tapen);
			tapen = NULL;
			seqserv->CloseSeqFile(tapea);
			tapea = NULL;

			//The file can now leave DU mode
			fctpage.NandFistat(FISTAT_DEFERRED_UPDATES);
			du_flag = false;
			num_du_files.Dec();
			du_numlen = 0;
			fctpage.SetDULen(0);
			du_format = 0;
			fctpage.SetDUFormat(0);

			//Commit it all
			dbapi->GetUU()->EndOfMolecule(true);

			dbapi->Core()->GetStatViewer()->EndActivity();
		}
		catch (...) {
			seqserv->CloseSeqFile(tapen);
			seqserv->CloseSeqFile(tapea);
			throw;
		}
	}
	MOLECULE_CATCH(this, dbapi->GetUU());

	return du_recs;
}

//********************************
_int64 DatabaseFile::ApplyDUFile(SingleDatabaseFileContext* context, 
	SequentialFileView* tape, bool num, int forgivingness, int& forgiven) 
{
//	bool oldstyle_switch = (forgivingness == -2);

//	DatabaseServices* dbapi = context->DBAPI();

	_int64 du_recs = 0;
	std::string record;
	char buff[14];

	FieldValue ixval;
	FieldValue prevval;
	if (num)
		prevval = RangeCheckedDouble::MAXIMUM_NEGATIVE_VALUE;
	else
		prevval = "";

	PhysicalFieldInfo* prevpfi = NULL;
	int prevfid = -1;
	BTreeAPI_Load btree;
	BTreeAPI_Load* pbtree = &btree;

	BitMappedFileRecordSet fvalset(context);
	int fvalrecs = 0;

	int recnum;
	int prevrecnum;
	short fid;

	for (;;) {
		bool eof = false;

		//The record formatting convention in the file varies (see write functions above)

		//Fixed length (ORD NUM fields in binary representation)
		if (num && du_numlen == -1) {
			eof = tape->ReadNoCRLF(buff, 14);
			if (eof)
				break;

			recnum = *(reinterpret_cast<int*>(buff));
			fid = *(reinterpret_cast<short*>(buff+4));
			ixval = *(reinterpret_cast<RoundedDouble*>(buff+6));
		}

		//Variable length (ORD CHAR or stringized ORD NUM) using a length byte 
		//(as below, but with NOCRLF option).
		else if (du_format & DU_FORMAT_NOCRLF) {
			eof = tape->ReadNoCRLF(buff, 7);
			if (eof)
				break;

			//Rec# and FID are unencoded so just cast them back
			recnum = *(reinterpret_cast<int*>(buff));
			fid = *(reinterpret_cast<short*>(buff+4));

			//Followed by the value as a character string
			unsigned _int8 vlen = *(reinterpret_cast<unsigned _int8*>(buff+6));
			char vbuff[255];
			if (tape->ReadNoCRLF(vbuff, vlen)) {
				throw Exception(DBA_DEFERRED_UPDATE_ERROR, 
					"Deferred update file corruption - unexpected EOF");
			}

			ixval.AssignData(vbuff, vlen);
		}

		//Variable length, CRLF-delimited
		else {
			eof = tape->ReadLine(record);
			if (eof)
				break;

			//V2.03.  See the encoding function above for rec#/FID encoding method now.
			//All references to the number 15 changed to 7 in this chunk:
/*			if (record.length() < 15)
				throw Exception(DBA_DEFERRED_UPDATE_ERROR, 
					"Deferred update record length is < 15");

			std::string srecnum = record.substr(0, 10);
			util::DeBlank(srecnum);
			recnum = util::StringToInt(srecnum);

			std::string sfid = record.substr(10, 5);
			util::DeBlank(sfid);
			fid = util::StringToInt(sfid);
*/
			if (record.length() < 7)
				throw Exception(DBA_DEFERRED_UPDATE_ERROR, 
					"Deferred update record length is < 7");

			//Unpack tweaked record number
			unsigned char rbuff[5];
			memcpy(rbuff, record.c_str(), 5);
			util::DecodeNoCRLFValue(rbuff, 5);
			recnum = *(reinterpret_cast<int*>(rbuff+1));

			//And the tweaked FID
			unsigned char fbuff[2];
			fbuff[0] = record[5];
			fbuff[1] = record[6];

			//Replace x08 bit in both field ID bytes
			if (fbuff[1] & '\x80')		//if high byte has x80
				fbuff[1] |= '\x08';		//set x08 in high byte
			if (fbuff[1] & '\x40')		//if high byte has x40
				fbuff[0] |= '\x08';		//set x08 in low byte
			fbuff[1] &= '\x0F';			//clear high half of high byte again

			fid = *(reinterpret_cast<short*>(fbuff));

			//The field value is the rest of the line, if any
			std::string sval;
			if (record.length() > 7)
				sval = record.substr(7);

			ixval = sval;
		}

		du_recs++;

		if (num)
			ixval.ConvertToNumeric(true);

		if (recnum < 0)
			throw Exception(DBA_DEFERRED_UPDATE_ERROR, 
				"Deferred update record refers to negative database record number");

		//Feb 07.  Improved this process, as planned.  Now build sets in memory.
//		if (oldstyle_switch) {
//			prevpfi = fieldmgr->GetPhysicalFieldInfo(context, fid);
//			indexmgr->Atom_AddValRec(prevpfi, ixval, context, recnum);
//			continue;
//		}

		//Check forgivingness level if we get a value out of order
		int valcmp = (num) ? ixval.CompareNumeric(prevval) : ixval.CompareString(prevval);

		if (forgivingness >= 0 && fid == prevfid && valcmp < 0) {
			if (forgiven < forgivingness)
				forgiven++;
			else
				throw Exception(DBA_DEFERRED_UPDATE_ERROR, 
					"Too many unsorted deferred update records - aborting Z process");
		}

		//Go to disk with the set whenever either field or value changes
		if (fid != prevfid || valcmp != 0) {

			if (fvalrecs > 0) {

				//The old way ("add one record") is a quicker than setwise for 1 rec in
				//certain cases (i.e. no need to replace a whole bitmap just to flag 1 bit),
				//although in most loads indexes are built from scratch and these two 
				//functions then do pretty much the same thing.  However we can still
				//avoid creating a stub set object here and just pass the int directly in.
				if (fvalrecs == 1) {
					indexmgr->Atom_AddValRec(prevpfi, prevval, context, prevrecnum, pbtree);
					fvalrecs = 0;
				}
				else {

//* * *
//* * * 
//Aug 07/Jan 09
//Note re. potential future multithreaded Z processing.
/* 

We have the file in EXCL above, so this can not currently be done.  However Mick S
says setting up a group and multithreading loads into the members on a multi-CPU machine 
works well.  Z appears typically to be CPU bound (sounds plausible given all the 
complicated btree and list work that it does on data in buffer pages.  The evidence 
from V2.03 backs this up in that taking maybe 30-50% off the TAPEA/TAPEN record 
lengths hardly changed the run time for this phase.  It was aimed at speeding up the 
sort really anyway).

* * */

					indexmgr->Atom_AugmentValRecSet(prevpfi, prevval, context, &fvalset, pbtree);
					fvalset.ClearButNoDelete();
					fvalrecs = 0;
				}
			}

			if (fid != prevfid) {
				prevpfi = fieldmgr->GetPhysicalFieldInfo(context, fid);
				btree.Initialize(context, prevpfi);
				prevfid = fid;
			}

			prevval = ixval;
		}

		//As per above shortcut there's no need to build a set till we know there is more 
		//than one record for this field and value.
		if (fvalrecs >= 1) {
			if (fvalrecs == 1)
				fvalset.BitOr(prevrecnum);
			fvalset.BitOr(recnum);
		}

		fvalrecs++;
		prevrecnum = recnum;
	}

	//The set for the final field/value.
	if (fvalrecs > 0) {
		if (fvalrecs == 1)
			indexmgr->Atom_AddValRec(prevpfi, ixval, context, recnum, pbtree);
		else
			indexmgr->Atom_AugmentValRecSet(prevpfi, ixval, context, &fvalset, pbtree);
	}

	return du_recs;
}

//********************************
//V2.14 Jan 09 - these few 1-step functions here.
void DatabaseFile::WriteOneStepDURecord
(SingleDatabaseFileContext* context, int recnum, PhysicalFieldInfo* pfi, const FieldValue& ixval)
{
	if (!du_1step_info->IsInitialized())
		du_1step_info->Initialize(context);

	//When memory runs low, write out to table D, clear down memory, and carry on
	if (du_1step_info->AddEntry(context, pfi, ixval, recnum))
		du_1step_info->Flush(context, DeferredUpdate1StepInfo::MEMFULL);
}

//********************************
//Called by user request or when the file closes
int DatabaseFile::ApplyOneStepDUInfo(SingleDatabaseFileContext* context, bool file_closing)
{
	int rcode;
	DatabaseServices* dbapi = context->DBAPI();
	MsgRouter* router = dbapi->Core()->GetRouter();

	//It should be unless the user explicitly requests this.
	if (!du_1step_info) {
		throw Exception(DBA_DEFERRED_UPDATE_ERROR, 
			"The file is not in one-step deferred update mode");
	}

	//May as well not even start an update if there's nothing in there on a user request
	if (!file_closing && !du_1step_info->AnythingToFlush()) {
		router->Issue(DBA_DEFERRED_UPDATE_INFO, "There were no deferred index updates to flush");
		return 0;
	}

	//Here it's *not* happening as part of "add field", so start a fresh update
	try {
		OperationDelimitingCommit(dbapi);
		StartNonBackoutableUpdate(context, true);

		//The file may be broken by now, in which case there's not much we can do
		FCTPage_F fctpage(dbapi, fct_buff_page);
		unsigned char fistat = fctpage.GetFistat();

		if ( (fistat & FISTAT_LOG_BROKEN) 
			|| (fistat & FISTAT_FULL) 
			|| (fistat & FISTAT_PHYS_BROKEN) )
		{
			router->Issue(DBA_DEFERRED_UPDATE_ERROR,
				"Can't apply deferred index udpates, file is full or broken");
		}
		else {
			try {
				DeferredUpdate1StepInfo::FlushMode flush_mode = (file_closing) ?
					DeferredUpdate1StepInfo::CLOSING : 
					DeferredUpdate1StepInfo::USER;

				rcode = du_1step_info->Flush(context, flush_mode);
			}
			catch (...) {
				//If any problems, don't bother trying again at file close.
				ExitOneStepDUMode(context->DBAPI());
				throw;
			}
		}

		if (file_closing)
			ExitOneStepDUMode(dbapi);

		dbapi->GetUU()->EndOfMolecule(true);
	}
	MOLECULE_CATCH(this, dbapi->GetUU());

	return rcode;
}

//********************************
//V2.19 June 2009.  Factored this out so that we can use the DU1 process under the
//covers for REDEFINE and reorgs.  In those cases we're not necessarily concerned
//about TBO being on since lower APIs are used, and it's all non-backoutable anyway.
void DatabaseFile::EnterOneStepDUMode
(DatabaseServices* dbapi, int default_diags, const Du1Reason reason)
{
	FCTPage_F fctpage(dbapi, fct_buff_page);

	//Note that unlike multi-step, one-step DU mode does *NOT* persist across free/allocate.  
	//The last user to close the file flushes all the updates.
	if (fctpage.GetFistat() & FISTAT_DEFERRED_UPDATES) {
		std::string msg = "Cannot initiate fast index build mode";
		if (du_1step_info)
			msg.append(" (it is already active)");
		else
			msg.append(" (multi-step deferred update mode is currently active)");
		throw Exception(DB_FILE_STATUS_DEFERRED, msg);
	}

	//Use parameterized diagnostics level, or default for current operation
	int diags = dbapi->GetParmLOADDIAG();
	if (diags == LOADDIAG_DEFAULT)
		diags = default_diags;

	//Create data structures
	du_1step_info = new DeferredUpdate1StepInfo
		(this, GetDDName(), cfr_index, dbapi->GetParmLOADMEMP(), diags);

	du_flag = true;
	num_du_files.Inc();
	fctpage.OrFistat(FISTAT_DEFERRED_UPDATES);

	MsgRouter* router = dbapi->Core()->GetRouter();
	std::string msg("Fast index build mode initiated");
	if (reason == DU1_REDEFINE)
		msg.append(" (for redefine)");
	else if (reason == DU1_TAPED)
		msg.append(" (for fastload: generating indexes from loaded data)");
	else if (reason == DU1_TAPEI)
		msg.append(" (for fastload TAPEI files)");

	router->Issue(DBA_DEFERRED_UPDATE_INFO, msg);

	//No memory checking in this mode
	if (reason != DU1_TAPEI) {
		msg = "Will use up to ";
		msg.append(util::IntToString(du_1step_info->MaxMemSize()/1000000));
		msg.append("Mb memory (");
		msg.append(util::IntToString(du_1step_info->MaxMemPct()));
		msg.append("% of installed RAM)");
		router->Issue(DBA_DEFERRED_UPDATE_INFO, msg);
	}

	//V3.0 Moved this test here from regular Open() and made it a warning message
	int maxbuf = dbapi->GetParmMAXBUF();
	if (maxbuf > 100) {
		router->Issue(DBA_DEFERRED_UPDATE_INFO, std::string("Info: MAXBUF=")
			.append(util::IntToString(maxbuf))
			.append(": Low values (e.g. <100) give best index build performance"));
	}

}

//********************************
void DatabaseFile::ExitOneStepDUMode(DatabaseServices* dbapi)
{
	DeleteOneStepDUInfo();
	FCTPage_F fctpage(dbapi, fct_buff_page);
	fctpage.NandFistat(FISTAT_DEFERRED_UPDATES);
	du_flag = false;
	num_du_files.Dec();
}












//*************************************************************************************************
void DatabaseFile::Defrag(SingleDatabaseFileContext* context)
{
	throw Exception(DB_API_STUB_FUNC_ONLY, "This function is currently disabled");

	CheckFileStatus(false, false, true, false);

	DatabaseServices* dbapi = context->DBAPI();

//Update DBA doc section on transaction boundaries if/when this is coded
	OperationDelimitingCommit(dbapi);

	//Ensure we have the file in EXCL (will downgrade back to SHR on exit)
	FileOpenLockSentry fols(this, BOOL_EXCL, true);

//	FCTPage_F fctpage(dbapi, fct_buff_page);


	//Certainly flush and free all pages
	//checkpoint because file size changing? No it's not, so theoretically rollbackable.
		//Make it a command option?
	//If going via buffer manager start a NBU and commit at end.  Yes go via BM so we get
		//stats done properly.  Same comment for any other cases where I might consider
		//bypassing BM.

	//commit at the end like initialize;
}








//*************************************************************************************************
//V3.0.  These two no longer stubs.
void DatabaseFile::Unload
(SingleDatabaseFileContext* context, const FastUnloadOptions& opts, 
 const BitMappedRecordSet* recset, const std::vector<std::string>* fnames, 
 const std::string& dir, bool reorging)
{
	CheckFileStatus(false, true, true, true);

	FastUnloadRequest request(context, opts, recset, fnames, dir, reorging);
	request.Perform();
}

//***************************************************
void DatabaseFile::Load
(SingleDatabaseFileContext* context, const FastLoadOptions& opts, int eyeball, 
 BB_OPDEVICE* eyeball_altdest, const std::string& dir, bool reorging)
{
	CheckFileStatus(true, true, true, true);

	if (eyeball < 0)
		eyeball = INT_MAX;
	if (opts & FLOAD_NOACTION)
		eyeball = 0;

	//Ensure we have the file in EXCL (will downgrade back to SHR on exit)
	FileOpenLockSentry fols;
	if (!eyeball && !reorging && !(opts & FLOAD_NOACTION)) {
			
		//See doc
		OperationDelimitingCommit(context->DBAPI());
		StartNonBackoutableUpdate(context, true);

		fols.Get(this, BOOL_EXCL, true);
	}

	//Physically inconsistent file is quite common but there's no need to restart the user
	//as would normally happen with that error.
	try {
		FastLoadRequest request(context, opts, eyeball, eyeball_altdest, dir, reorging);
		request.Perform();
	}
	catch (Exception& e) {
		if (e.Code() == TXNERR_LOGICALLY_BROKEN || e.Code() == TXNERR_PHYSICALLY_BROKEN)
			throw Exception(DB_FILE_STATUS_BROKEN, e.What());
		else
			throw;
	}
}










//*************************************************************************************************
void DatabaseFile::SetBroadcastMessage(SingleDatabaseFileContext* context, const std::string& s)
{
	CheckFileStatus(false, true, true, false);

	DatabaseServices* dbapi = context->DBAPI();

	//See doc
	OperationDelimitingCommit(dbapi);

	StartNonBackoutableUpdate(context, true);

	LockingSentry ls(&broadcast_message_lock);
	FCTPage_F fctpage(dbapi, fct_buff_page);
	fctpage.SetBroadcastMessage(s);

	OperationDelimitingCommit(dbapi);
}











//**********************************************************************************************
//Record processing
//**********************************************************************************************
int DatabaseFile::StoreRecord(SingleDatabaseFileContext* context, StoreRecordTemplate& data) 
{
	CheckFileStatus(true, true, true, false);

	//Commit any open non-backoutable updates such as DEFINE FIELD
	OperationDelimitingCommit(context->DBAPI(), false, true);

	DatabaseServices* dbapi = context->DBAPI();
	UpdateUnit* uu = dbapi->GetUU();
	int newrecnum;

	try {
		//First store an empty record
		AtomicUpdate_StoreEmptyRecord aus(context);
		newrecnum = aus.Perform();
		data.SetRecNum(newrecnum);

		//In the case of STORE, since nobody could see the record till the end of the 
		//molecule anyway we needn't put a record lock on here if TBO is off.  With TBO 
		//on though this lock will persist till the end of the entire update unit.
		if (dbapi->TBOIsOn())
			uu->PlaceRecordUpdatingLock(this, newrecnum);

		//Next add the fields to the record one at a time
		if (data.NumFVPairs() > 0) {

			//V2.03 - Feb 07.  We can skip a couple of things during a deferred update job.
			//One is not to take CFR_DIRECT for every field, since there will be no index
			//work between fields.  Take it once up front here.
			CFRSentry s;
			if (du_flag)
				s.Get(context->DBAPI(), cfr_direct, BOOL_EXCL);

			Record r(context, newrecnum, false, false);

			//Read through the fields in the input template
			std::string fname;
			FieldValue fval;
			int fvpix = 0;
			data.Validate();
			while (data.GetNextFVPair(fname, fval, fvpix)) {

				//These preliminaries for each field need not preclude backout of earlier ones
				PhysicalFieldInfo* pfi = NULL;
				const FieldValue* use_dataval = &fval;
				const FieldValue* use_ixval = &fval;
				FieldValue datatemp;
				FieldValue ixtemp;

				try {

					//V2.18.  Use any cached FIDs to save repeated lookups.
					if (data.got_fids)
						pfi = data.fids[fvpix-1];
					else {
						pfi = fieldmgr->GetAndValidatePFI(context, fname, true, true, false);
						data.AppendFid(pfi);
					}
	
					//V2.20.  Special case now for User Language where null val may mean no store
					if (fval.SpecialType() == UL_MAGIC_S)
						continue;

					//Validate/convert for storage in data and indexes
					r.ValidateAndConvertTypes
						(pfi, fval, &datatemp, &use_dataval, &ixtemp, &use_ixval); 
				}
				catch (Exception& e) {
					dbapi->Core()->GetRouter()->Issue(e.Code(), e.What());
					throw Exception(TXN_BENIGN_ATOM, "Field error during record store");
				}

				//Call the code shared with standalone ADD FIELD
				r.DoubleAtom_AddField(pfi, *use_dataval, *use_ixval, true, du_flag);
			}
		}

		//Finally flip bit on in the EBM
		AtomicUpdate_ExistizeStoredRecord aue(context, newrecnum);
		aue.Perform();

		uu->EndOfMolecule();
	}
	MOLECULE_CATCH(this, uu);

	return newrecnum;
}

//********************************
void DatabaseFile::FindRecords
(int groupix, SingleDatabaseFileContext* sfc, FoundSet* set,  const FindSpecification& spec, 
 const FindEnqueueType& locktype, const BitMappedFileRecordSet* fbaseset) 
{
	IncStatFINDS();
	indexmgr->FindRecords(groupix, sfc, set, spec, locktype, fbaseset);
}

//********************************
void DatabaseFile::DirtyDeleteRecords
(SingleDatabaseFileContext* context, BitMappedFileRecordSet* fgoners)
{
	DatabaseServices* dbapi = context->DBAPI();
	UpdateUnit* uu = dbapi->GetUU();

	//Commit any open non-backoutable updates such as DEFINE FIELD
	OperationDelimitingCommit(dbapi, false, true);

	try {
		AtomicUpdate_DirtyDeleteRecords audd(context, fgoners);
		
		audd.Perform();		
		
		uu->EndOfMolecule();
	}
	MOLECULE_CATCH(this, uu);
}

//********************************
void DatabaseFile::FileRecordsUnder
(SingleDatabaseFileContext* context, BitMappedFileRecordSet* fset,
 const std::string& fieldname, const FieldValue& inval)
{
	DatabaseServices* dbapi = context->DBAPI();
	UpdateUnit* uu = dbapi->GetUU();

	PhysicalFieldInfo* pfi = fieldmgr->GetAndValidatePFI
		(context, fieldname, false, false, true);

	//Ensure the correct value type for the index
	const FieldValue* use_value = &inval;
	FieldValue ixtemp;
	if (pfi->atts.IsOrdNum() != inval.CurrentlyNumeric()) {
		DatabaseFileFieldManager::ConvertValue(context, pfi,
			inval, &ixtemp, &use_value, true);
	}

	//V3.  BLOB fields can't be indexed, but the FieldValue class has been opened
	//out to allow long values, so they could make it to here.
	if (use_value->CurrentlyString())
		use_value->CheckStrLen255("used in FILE RECORDS");

	//Commit any open non-backoutable updates such as DEFINE FIELD
	OperationDelimitingCommit(dbapi, false, true);

	try {
		//NB context required as well as set, since the set may be null
		AtomicUpdate_FileRecords aufr(context, fset, pfi, *use_value);
		
		aufr.Perform();		
		
		uu->EndOfMolecule();
	}
	MOLECULE_CATCH(this, uu);
}










//**********************************************************************************************
//Transaction control
//**********************************************************************************************
void DatabaseFile::BeginUpdate(DatabaseServices* dbapi)
{
	//Possibly similar to M204.  The most common reason to wait here would be another
	//user flushing pages at EOT, not another user checkpoiting, although could be that. 
	//25 would be equally valid I guess (CFR/SHR wait).
	WTSentry ws(dbapi->Core(), 20);

	//Checkpointing prevents any new updates from starting - see CheckpointProcessing
	SharingSentry ls(&sys_update_start_inhibitor);

	//Another user may be flushing the file's pages, but we're happy to wait here.  Note
	//it must just be a normal updater since checkpointing can't be active (see above).
	cfr_updating->Get(dbapi, BOOL_SHR);
}

//********************************
void DatabaseFile::StartNonBackoutableUpdate(SingleDatabaseFileContext* context, bool real)
{
	AtomicUpdate_NonBackoutable aunb(context, real);
	aunb.Start();
}

//********************************
void DatabaseFile::CompleteUpdateAndFlush
(DatabaseServices* dbapi, bool significant, _int64 updttime)
{
	if (significant) {
		SetLastTransactionTime(dbapi);
		stat_updttime.Add(updttime);
	}

	//See if we're the last updater, and if so, prevent any other updates from starting
	if (!cfr_updating->UpgradeToExcl())
		return;

	try {
		BufferedFileInterface::FlushAllDirtyPages(dbapi, buffapi);
		cfr_updating->Release();
	}
	catch (...) {
		cfr_updating->Release();
		throw;
	}
}

//********************************
void DatabaseFile::OperationDelimitingCommit
(DatabaseServices* dbapi, bool if_backoutable, bool if_nonbackoutable)
{
	dbapi->Commit(if_backoutable, if_nonbackoutable);
}

//********************************
void DatabaseFile::CheckpointProcessing(DatabaseServices* dbapi, int cpto, FileHandle* allocee)
{
	//CHKABORT should only apply if we're actually started
	dbapi->ChkAbortAcknowledge();

	//Is this the same as M204?
	WTSentry ws(dbapi->Core(), 19);

	//No updates may start during this time.  Use a system-wide lock to cater for the 
	//possibility of new files getting allocated and updates starting on them.  Threads
	//attempting to begin an update will block in BeginUpdate above
	LockingSentry ls(&sys_update_start_inhibitor);

	std::vector<FileHandle> vh;
	std::vector<CFRSentry> updating_locks;

	//Get a list of currently-allocated files and lock them in
	AllocatedFile::ListAllocatedFiles(vh, BOOL_SHR, FILETYPE_DB);

	//Now we need to make sure any existing updates have finished, by taking UPDATING
	//EXCL for all updated files.  Depending on the situation we may be happy to 
	//wait for CPTO seconds (auto/PST checkpointing) or not (all other cases).
	int num_files = vh.size();
	int num_processed = 0;
	std::vector<bool> processed_flags;
	processed_flags.resize(num_files, false);

	//Indefinitely long loop
	for (;;) {

		//Make a pass across the files and take one stab at UPDATING for each
		for (size_t x = 0; x < vh.size(); x++) {
			LockingSentry ls(&chkmsg_info_lock);

			if (processed_flags[x])
				continue;

			DatabaseFile* f = static_cast<DatabaseFile*>(vh[x].GetFile());

			//Skip files that haven't been updated.  Not strictly necessary since
			//we would be guaranteed to get UPDATING immediately for them, but
			//why do unnecessary work.
			if (!f->IsPhysicallyUpdatedSinceCheckpoint()) {
				processed_flags[x] = true;
				f->chkmsg_info = std::string();
				num_processed++;
				continue;
			}

			//If we get UPDATING, keep hold of it and go round for more files
			if (f->cfr_updating->Try(BOOL_EXCL)) {
				//This is a "release only" sentry
				CFRSentry s(dbapi, f->cfr_updating, f->cfr_updating);
				updating_locks.push_back(s);
				processed_flags[x] = true;
				f->chkmsg_info = std::string();
				num_processed++;
			}

			//Make a list of users who are updating for use by CHKMSG
			else {
				f->chkmsg_info = std::string(vh[x].GetDD())
									.append(" (")
									.append(f->cfr_updating->Sharers())
									.append(1, ')');
			}

			//Decided not to make this bumpable.  So many places cause an implied
			//checkpoint that the USER_HEEDINGBUMP exception would need the special
			//"pass-through" processing in too many places.  If I change my mind on
			//this it might be a good time to review the overall exception strategy.
			//win::Cede();
			//dbapi->Core()->Tick("trying to quiesce updates for checkpoint");
		}

		//Got them all
		if (num_processed == num_files)
			break;

		bool timed_out = false;
		time_t now;

		//Check for timeout.  CPTO special values:
		//	0 = wait as long as necessary
		//	-1 = don't wait at all
		if (cpto != 0) {
			time(&now);

			if (now >= DatabaseServices::GetCurrentChkpTime() + cpto) 
				timed_out = true;
		}

		//According to the manual, CHKABORT triggers exactly the same thing
		if (!timed_out) {
			if (dbapi->ChkAbortAcknowledge()) {
				timed_out = true;
				time(&now);
			}
		}

		if (timed_out) {
			std::string msg("Checkpoint timed out at ");
			msg.append(win::GetCTime(now));
			msg.append(" - use MONITOR CHECKPOINT for more information");				
		
			DatabaseServices::recent_timed_out_checkpoints++;
			throw Exception(CHKP_TIMED_OUT, msg);
		}

		//Go round and try again for any remaining CFRs
		win::Cede();
	}

	//Flush dirty pages, clear down the checkpoint file and logged flags
	BufferedFileInterface::CheckpointProcessing
		(dbapi, DatabaseServices::GetCurrentChkpTime());

	//Finally write back the full current list of allocated files to the CP file
	for (size_t x = 0; x < vh.size(); x++) {
		DatabaseFile* f = static_cast<DatabaseFile*>(vh[x].GetFile());

		CheckpointFile::Object()->WriteAllocatedFileInfo
			(dbapi, vh[x].GetDD().c_str(), vh[x].GetDSN().c_str());

		f->MarkPhysicallyUpdatedSinceCheckpoint(dbapi, false);
	}

//* * * 
	//NB. This the order of the files written to the CHKP file header is
	//not file ID order because we've retrieved them from the system allocation table,
	//plus this file is just tacked on the end too.  If we want that to be the case
	//we need to sort all files by file ID or name or something.
	//Even if we wrote the file index to the CP file wo could not be sure the ids
	//would agree with the ones after allocating during rollback, since a) sequential,
	//proc files etc. would not be involved, and b) some files may fail allocation.
//* * * 

	//Plus any newly-allocated file if this function was called during ALLOCATE
	if (allocee) {
		CheckpointFile::Object()->WriteAllocatedFileInfo
			(dbapi, allocee->GetDD().c_str(), allocee->GetDSN().c_str());
	}

	CheckpointFile::Object()->EndOfAllocatedFileInfo();

	DatabaseServices::UpdateCheckpointTimes();
}

//********************************
bool DatabaseFile::CheckPointIsHappening()
{
	bool b = sys_update_start_inhibitor.AttemptShared();
	if (b)
		sys_update_start_inhibitor.ReleaseShared();

	return !b;
}

//********************************
void DatabaseFile::GetAllChkMsgInfo(std::vector<std::string>& result)
{
	std::vector<FileHandle> vh;
	AllocatedFile::ListAllocatedFiles(vh, BOOL_SHR, FILETYPE_DB);

	LockingSentry ls(&chkmsg_info_lock);

	for (size_t x = 0; x < vh.size(); x++) {
		DatabaseFile* f = static_cast<DatabaseFile*>(vh[x].GetFile());
		if (f->chkmsg_info.length() > 0)
			result.push_back(f->chkmsg_info);
	}
}

//**********************************************************************************************
//This FCT timestamp is the key control field in rollback, and here it's written 
//*directly* into the file, since it must agree with the value in the checkpoint file, 
//which is also written directly.
//**********************************************************************************************
void DatabaseFile::MarkPhysicallyUpdatedSinceCheckpoint(DatabaseServices* dbapi, bool b)
{
	//When clearing the flag it's simple
	if (!b) {
		updated_since_checkpoint.Reset();
		return;
	}

	//This is done in an atomic test-and-set - the previous value is returned
	if (updated_since_checkpoint.Set())
		return;

	FCTPage_F fctpage(dbapi, fct_buff_page);

	time_t lct = DatabaseServices::GetLastChkpTime();
	fctpage.SetLastCPTime(lct);
	buffapi->PhysicalPageWrite(dbapi, 0, fct_buff_page->PageData());
}

//********************************
int DatabaseFile::BufferTidyProcessing(int cutoff)
{
	//There is no file-specific tidying at present - just use this static function.
	return BufferedFileInterface::Tidy(cutoff);
}









//******************************************************************************************
//Parameter viewing and resetting
//******************************************************************************************
std::string DatabaseFile::ViewParm
(SingleDatabaseFileContext* context, const std::string& parmname, bool format) const
{
	DatabaseServices* dbapi = context->DBAPI();

	//----------------------------------------
	//Parms managed by the table B sub-object
	if (util::OneOf(parmname, "BRECPPG/BRESERVE/BREUSE/BHIGHPG/BQLEN/"
								"BREUSED/MSTRADD/MSTRDEL/EXTNADD/EXTNDEL", '/'))
		return tablebmgr->ViewParm(context, parmname);

	//----------------------------------------
	//Parms managed by the table D sub-object
	if (util::OneOf(parmname, "DPGSRES/DHIGHPG/DPGSUSED/DRESERVE/DACTIVE/ILACTIVE"
		"/DPGS_1/DPGS_2/DPGS_3/DPGS_4/DPGS_5/DPGS_6/DPGS_7/DPGS_8/DPGS_X", '/'))
		return tabledmgr->ViewParm(context, parmname);

	//----------------------------------------
	//Parms managed by the index sub-object
//	if (parmname == "") 
//		return indexmgr->ViewParm(dbapi, parmname);

	//----------------------------------------
	//Parms managed by the fields sub-object
	if (util::OneOf(parmname, "ATRPG/ATRFLD", '/'))
		return fieldmgr->ViewParm(context, parmname);

	//----------------------------------------
	//Parms managed here in the main DatabaseFile class
	FCTPage_F fctpage(dbapi, fct_buff_page);

	//Those requiring FILE EXCL to change and therefore not needing a lock here
	if (parmname == "BSIZE") return util::IntToString(fctpage.GetBsize());
	if (parmname == "DSIZE") return util::IntToString(fctpage.GetDsize());

	//Parms protected by the miscellaneous FCT page lock
	LockingSentry ls(&fistat_and_misc_lock);

	if (parmname == "FICREATE") {return std::string(1, fctpage.GetFicreate());}

	if (parmname == "FISTAT") {
		unsigned char fistat = fctpage.GetFistat();

		//V2.10 - Dec 07.  Should take account of the "fancy format" flag.
		if (!format) return util::IntToString(fistat);

		std::string s = util::UlongToHexString(fistat, 2, true);

		if (fistat == 0)
			s.append(" - Normal");

		if (fistat & FISTAT_LOG_BROKEN) {
			s.append(" - ");
			s.append("Logically broken");
		}
		if (fistat & FISTAT_RECOVERED) {
			s.append(" - ");
			s.append("Recovered");
		}
		if (fistat & FISTAT_DEFERRED_UPDATES) {
			s.append(" - ");
			s.append("Deferred updates");
		}
		if (fistat & FISTAT_FULL) {
			s.append(" - ");
			s.append("Full");
		}
		if (fistat & FISTAT_PHYS_BROKEN) {
			s.append(" - ");
			s.append("Physically broken");
		}
		if (fistat & FISTAT_NOT_INIT) {
			s.append(" - ");
			s.append("Not initialized");
		}

		return s;
	}

	if (parmname == "FIFLAGS") {
		unsigned char fiflags = fctpage.GetFiflags();

		//V2.10 - Dec 07.  Should take account of the "fancy format" flag.
		if (!format) return util::IntToString(fiflags);

		std::string s = util::UlongToHexString(fiflags, 2, true);
		s.append(" - ");

		//Only one of these should ever happen at the same time!
		if (fiflags == 0)
			s.append("Normal");
		if (fiflags & FIFLAGS_PAGE_BROKEN)
			//V2.23.  This isn't actually a bug (see also V2.23 change in ::open)
			//s.append("Page broken - bug!");
			s.append("Partially flushed update in progress");
		if (fiflags & FIFLAGS_FULL_TABLEB)
			s.append("Table B is full");
		if (fiflags & FIFLAGS_FULL_TABLED)
			s.append("Table D is full");
		if (fiflags & FIFLAGS_REORGING)
			s.append("Being reorganized");

		return s;
	}

	if (parmname == "FILEORG") {
		unsigned char fileorg = fctpage.GetFileorg();

		//V2.10 - Dec 07.  Should take account of the "fancy format" flag.
		if (!format) return util::IntToString(fileorg);

		//Also V2.10 append description.
		std::string s = util::UlongToHexString(fileorg, 2, true);
		s.append(" - ");

		if (fileorg & FILEORG_RRN)
			s.append("Reuse record numbers");
		else
			s.append("Entry order");

		return s;
	}

	if (parmname == "FICREATE") return util::IntToString(fctpage.GetFicreate());
	
	//V3.01. Some time in 2011.
	//if (parmname == "FIWHEN") return win::GetCTime(fctpage.GetFiwhen())
	if (parmname == "FIWHEN") {
		time_t when = fctpage.GetFiwhen();
		if (when == 0)
			return "0";
		return win::GetCTime(when);
	}
	
	if (parmname == "FIWHEN") return win::GetCTime(fctpage.GetFiwhen());
	if (parmname == "FIWHO") return fctpage.GetFiwho();
	if (parmname == "SEQOPT") return util::IntToString(fctpage.GetSeqopt());
	if (parmname == "OSNAME") return fctpage.GetLastOSFileName();

//Static parms
	//none

//Anything else passed in
	throw Exception(BUG_MISC, std::string
		("Bug: View passed incorrectly to file for parm: ").append(parmname));
}

//********************************
std::string DatabaseFile::ResetParm
(SingleDatabaseFileContext* context, const std::string& parmname, const std::string& newvalue)
{
	DatabaseServices* dbapi = context->DBAPI();

	//See doc
	OperationDelimitingCommit(dbapi);

	unsigned int inew = util::StringToUlong(newvalue);
	FCTPage_F fctpage(dbapi, fct_buff_page);

	//V3.03
	AccessController::EnsureCanChangeFileParms(dbapi->Core()->GetEffectiveUserPrivs());

	//Resettable parameters.  Take FILE EXCL for all these to simplify things (see above)
	FileOpenLockSentry fols(this, BOOL_EXCL, true);

	if (parmname == "FISTAT") {
		unsigned char i8cur = fctpage.GetFistat();
		unsigned char i8new = inew;

		//Bits can only be cleared
		if (i8new & ~i8cur)
			throw Exception(PARM_BADVALUE, "FISTAT bits can only be turned off, not on");

		unsigned char clearees = i8cur & ~i8new;

		if (clearees & FISTAT_PHYS_BROKEN) {
			if (!dbapi->Core()->InteractiveYesNo
					("Do you really want to clear the physically broken bit?", true))
				throw Exception(PARM_MISC, "Reset abandoned");
		}

		if (clearees & FISTAT_DEFERRED_UPDATES) {

			//Obviously this would be highly undesirable if any updates had been deferred
			if (!dbapi->Core()->InteractiveYesNo
					("Do you really want to clear the deferred update bit?", true))
				throw Exception(PARM_MISC, "Reset abandoned");
		}

		if (clearees & FISTAT_NOT_INIT) {
			throw Exception(PARM_NOTRESETTABLE, 
				"You can't reset the 'not initialized' bit - use the INITIALIZE command");
		}

		StartNonBackoutableUpdate(context, true);
		fctpage.ManualResetFistat(inew);

		//If they said the file's not full any more, clear the secondary flags too
		if (clearees & FISTAT_FULL) {
			fctpage.NandFiflags(FIFLAGS_FULL_TABLEB);
			fctpage.NandFiflags(FIFLAGS_FULL_TABLED);
		}

		//V2.23 Moved this down from above as usually we need the update to have started.
		if (clearees & FISTAT_DEFERRED_UPDATES) {
			DeleteOneStepDUInfo();
			CloseDUFiles(dbapi);
			if (du_flag) {
				du_flag = false;
				num_du_files.Dec();
				du_numlen = 0;
				fctpage.SetDULen(0);
				du_format = 0;
				fctpage.SetDUFormat(0);
			}
		}
	}
	else if (parmname == "SEQOPT") {
		StartNonBackoutableUpdate(context, true);
		buffapi->SetSeqopt(inew);
		fctpage.SetSeqopt(inew);
	}

	else if (util::OneOf(parmname, "BRESERVE/BREUSE", '/'))
		tablebmgr->ResetParm(context, parmname, inew);

	else if (util::OneOf(parmname, "DRESERVE/DPGSRES", '/'))
		tabledmgr->ResetParm(context, parmname, inew);

//	else if ()
//		indexmgr->ResetParm(context, parmname, inew);

//Static parms
	//none
	
//Anything else passed in
	else {
		throw Exception(BUG_MISC, std::string
			("Bug: RESET passed incorrectly to file for parm: ").append(parmname));
	}

	OperationDelimitingCommit(dbapi);
	return newvalue;
}










//******************************************************************************************
//Stats
//******************************************************************************************
_int64 DatabaseFile::ViewStat(const std::string& statname) const
{
	if (statname == "BACKOUTS")	return stat_backouts.Value();
	if (statname == "COMMITS")	return stat_commits.Value();
	if (statname == "UPDTTIME")	return (stat_updttime.Value() / 10000);

	if (statname == "BADD")		return stat_badd.Value();
	if (statname == "BCHG")		return stat_bchg.Value();
	if (statname == "BDEL")		return stat_bdel.Value();
	if (statname == "BXDEL")	return stat_bxdel.Value();
	if (statname == "BXFIND")	return stat_bxfind.Value();
	if (statname == "BXFREE")	return stat_bxfree.Value();
	if (statname == "BXINSE")	return stat_bxinse.Value();
	if (statname == "BXNEXT")	return stat_bxnext.Value();
	if (statname == "BXRFND")	return stat_bxrfnd.Value();
	if (statname == "BXSPLI")	return stat_bxspli.Value();
	if (statname == "DIRRCD")	return stat_dirrcd.Value();
	if (statname == "FINDS")	return stat_finds.Value();
	if (statname == "RECADD")	return stat_recadd.Value();
	if (statname == "RECDEL")	return stat_recdel.Value();
	if (statname == "RECDS")	return stat_recds.Value();
	if (statname == "STRECDS")	return stat_strecds.Value();

	if (statname == "DKPR")		return stat_dkpr.Value();
	if (statname == "DKRD")		return stat_dkrd.Value();
	if (statname == "DKWR")		return stat_dkwr.Value();
	if (statname == "FBWT")		return stat_fbwt.Value();
	if (statname == "DKSFBS")	return stat_dksfbs.Value();
	if (statname == "DKSFNU")	return stat_dksfnu.Value();
	if (statname == "DKSKIP")	return stat_dkskip.Value();
	if (statname == "DKSKIPT")	return stat_dkskipt.Value();
	if (statname == "DKSWAIT")	return stat_dkswait.Value();
	if (statname == "DKUPTIME")	return (stat_dkuptime.Value() / 10000);

	if (statname == "ILMRADD")	return stat_ilmradd.Value();
	if (statname == "ILMRDEL")	return stat_ilmrdel.Value();
	if (statname == "ILMRMOVE")	return stat_ilmrmove.Value();
	if (statname == "ILRADD")	return stat_ilradd.Value();
	if (statname == "ILRDEL")	return stat_ilrdel.Value();
	if (statname == "ILSADD")	return stat_ilsadd.Value();
	if (statname == "ILSDEL")	return stat_ilsdel.Value();
	if (statname == "ILSMOVE")	return stat_ilsmove.Value();

	if (statname == "MRGVALS")	return stat_mrgvals.Value();

	throw Exception(BUG_MISC, std::string
		("Bug: VIEW passed incorrectly to file for stat: ").append(statname));
	return 0;
}

//********************************
void DatabaseFile::IncStatDKPR(DatabaseServices* d) {stat_dkpr.Inc(); d->stat_dkpr.Inc();}
void DatabaseFile::IncStatDKRD(DatabaseServices* d) {stat_dkrd.Inc(); d->stat_dkrd.Inc();}
void DatabaseFile::IncStatDKWR(DatabaseServices* d) {stat_dkwr.Inc(); d->stat_dkwr.Inc(); time(&d->last_dkwr_time);}
void DatabaseFile::IncStatFBWT(DatabaseServices* d) {stat_fbwt.Inc(); d->stat_fbwt.Inc();}
void DatabaseFile::IncStatDKSFBS(DatabaseServices* d) {stat_dksfbs.Inc(); d->stat_dksfbs.Inc();}
void DatabaseFile::IncStatDKSFNU(DatabaseServices* d) {stat_dksfnu.Inc(); d->stat_dksfnu.Inc();}
void DatabaseFile::AddToStatDKSKIPT(DatabaseServices* d, int n) {
								stat_dkskipt.Add(n); d->stat_dkskipt.Add(n);}
void DatabaseFile::IncStatDKSWAIT(DatabaseServices* d) {stat_dkswait.Inc(); d->stat_dkswait.Inc();}
void DatabaseFile::HWMStatDKSKIP(DatabaseServices* dbapi, int i) {
								stat_dkskip.SetHWM(i); dbapi->stat_dkskip.SetHWM(i);}
void DatabaseFile::AddToStatDKUPTIME(DatabaseServices* dbapi, _int64 i) {
								stat_dkuptime.Add(i); dbapi->stat_dkuptime.Add(i);}

//File stats
void DatabaseFile::IncStatRECADD(DatabaseServices* d) {stat_recadd.Inc(); d->stat_recadd.Inc();}
void DatabaseFile::IncStatRECDEL(DatabaseServices* d) {stat_recdel.Inc(); d->stat_recdel.Inc();}
void DatabaseFile::IncStatBADD(DatabaseServices* d) {stat_badd.Inc(); d->stat_badd.Inc();}
void DatabaseFile::IncStatBCHG(DatabaseServices* d) {stat_bchg.Inc(); d->stat_bchg.Inc();}
void DatabaseFile::IncStatBDEL(DatabaseServices* d) {stat_bdel.Inc(); d->stat_bdel.Inc();}
void DatabaseFile::IncStatBXDEL(DatabaseServices* d) {stat_bxdel.Inc(); d->stat_bxdel.Inc();}
void DatabaseFile::IncStatBXFIND(DatabaseServices* d) {stat_bxfind.Inc(); d->stat_bxfind.Inc();}
void DatabaseFile::IncStatBXFREE(DatabaseServices* d) {stat_bxfree.Inc(); d->stat_bxfree.Inc();}
void DatabaseFile::AddToStatBXFREE(DatabaseServices* d, int n) {stat_bxfree.Add(n); d->stat_bxfree.Add(n);}
void DatabaseFile::IncStatBXINSE(DatabaseServices* d) {stat_bxinse.Inc(); d->stat_bxinse.Inc();}
void DatabaseFile::IncStatBXNEXT(DatabaseServices* d) {stat_bxnext.Inc(); d->stat_bxnext.Inc();}
void DatabaseFile::IncStatBXRFND(DatabaseServices* d) {stat_bxrfnd.Inc(); d->stat_bxrfnd.Inc();}
void DatabaseFile::IncStatBXSPLI(DatabaseServices* d) {stat_bxspli.Inc(); d->stat_bxspli.Inc();}
//file and user level are not always the same - groups
void DatabaseFile::AddToStatDIRRCD(DatabaseServices* d, int i) {stat_dirrcd.Add(i); d->stat_dirrcd.Add(i);}
void DatabaseFile::IncStatFINDS() {stat_finds.Inc();}
void DatabaseFile::IncStatRECDS(DatabaseServices* d) {stat_recds.Inc(); d->stat_recds.Inc();}
void DatabaseFile::IncStatSTRECDS(DatabaseServices* d) {stat_strecds.Inc(); d->stat_strecds.Inc();}

void DatabaseFile::IncStatILMRADD(DatabaseServices* d) {stat_ilmradd.Inc(); d->stat_ilmradd.Inc();}
void DatabaseFile::IncStatILMRDEL(DatabaseServices* d) {stat_ilmrdel.Inc(); d->stat_ilmrdel.Inc();}
void DatabaseFile::IncStatILMRMOVE(DatabaseServices* d) {stat_ilmrmove.Inc(); d->stat_ilmrmove.Inc();}
void DatabaseFile::IncStatILRADD(DatabaseServices* d) {stat_ilradd.Inc(); d->stat_ilradd.Inc();}
void DatabaseFile::IncStatILRDEL(DatabaseServices* d) {stat_ilrdel.Inc(); d->stat_ilrdel.Inc();}
void DatabaseFile::IncStatILSADD(DatabaseServices* d) {stat_ilsadd.Inc(); d->stat_ilsadd.Inc();}
void DatabaseFile::IncStatILSDEL(DatabaseServices* d) {stat_ilsdel.Inc(); d->stat_ilsdel.Inc();}
void DatabaseFile::IncStatILSMOVE(DatabaseServices* d) {stat_ilsmove.Inc(); d->stat_ilsmove.Inc();}

void DatabaseFile::AddToStatMRGVALS(DatabaseServices* d, _int64 i) {
									stat_mrgvals.Add(i); d->stat_mrgvals.Add(i);}

} //close namespace
