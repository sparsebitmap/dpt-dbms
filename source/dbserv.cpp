
#include "stdafx.h"

#include "dbserv.h"

#include "assert.h"

//Utils
#include "liostdio.h"	
#include "liocons.h" //V2.02 Feb 07.
#include "dataconv.h"	
#include "charconv.h"	
#include "winutil.h"	
#include "parsing.h"	
#include "rsvwords.h"	
#include "windows.h"	
#include "direct.h" //V2.27
//API tiers
#include "session.h"	
#include "grpserv.h"	
#include "ctxtdef.h"	
#include "dbfile.h"	
#include "update.h"	
#include "recovery.h"	
#include "checkpt.h"	
#include "buffmgmt.h"	
#include "dbstatus.h"	
#include "dbctxt.h"	
#include "page_t.h"	//#include "page_T.h" : V2.24 case is less interchangeable on *NIX - Roger M.
#include "core.h"
#include "parmvr.h"
#include "parmini.h"
#include "statview.h"
#include "msgroute.h"
#include "audit.h"
#include "seqserv.h"	
#include "sysfile.h"	
//Diagnostics
#include "except.h"
#include "msg_db.h"
#include "msg_file.h"
#include "msg_util.h"
#include "assert.h"

//The PSTs are spawned when resetting CPTIME/BUFAGE in API mode only
#ifdef _BBHOST
#include "daemon.h"
#endif

#ifdef DPT_BUILDING_CAPI_DLL
#include "capi_commarea_base.h"
#endif

namespace dpt {

//Define static objects
#ifdef _DEBUG_LOCKS
Sharable DatabaseServices::instances_lock = Sharable("DBServs instances");;
#else
Sharable DatabaseServices::instances_lock;
#endif
ThreadSafeLong DatabaseServices::num_instances;
std::vector<DatabaseServices*> DatabaseServices::instances;

std::string DatabaseServices::delete_dir_at_closedown;

int DatabaseServices::recent_timed_out_checkpoints = 0;
ThreadSafeLong DatabaseServices::last_checkpoint_time;
ThreadSafeLong DatabaseServices::current_checkpoint_time;
ThreadSafeLong DatabaseServices::next_checkpoint_time;
ThreadSafeFlag DatabaseServices::chkabort_flag;
time_t DatabaseServices::last_recovery_time = 0;
time_t DatabaseServices::last_dkwr_time = 0;
time_t DatabaseServices::last_update_time = 0;

//parms
int DatabaseServices::static_parm_maxbuf = 0;
int DatabaseServices::static_parm_rcvopt = 0;
int DatabaseServices::static_parm_btreepad = 0;
ThreadSafeLong DatabaseServices::static_parm_cpto = 0;
ThreadSafeLong DatabaseServices::static_parm_cptime = 0;
ThreadSafeLong DatabaseServices::static_parm_bufage = 0;
ThreadSafeLong DatabaseServices::static_parm_fmodldpt = 0;
ThreadSafeLong DatabaseServices::static_parm_maxrecno = 0;
ThreadSafeLong DatabaseServices::static_parm_loadctl = 0;
ThreadSafeLong DatabaseServices::static_parm_loaddiag = 0;
ThreadSafeLong DatabaseServices::static_parm_loadfvpc = 0;
ThreadSafeLong DatabaseServices::static_parm_loadmemp = 0;
ThreadSafeLong DatabaseServices::static_parm_loadmmfh = 0;
ThreadSafeLong DatabaseServices::static_parm_loadthrd = 0;

//stats
ThreadSafeI64 DatabaseServices::sysstat_backouts;
ThreadSafeI64 DatabaseServices::sysstat_commits;
ThreadSafeI64 DatabaseServices::sysstat_updttime;

ThreadSafeI64 DatabaseServices::sysstat_badd;
ThreadSafeI64 DatabaseServices::sysstat_bchg;
ThreadSafeI64 DatabaseServices::sysstat_bdel;
ThreadSafeI64 DatabaseServices::sysstat_bxdel;
ThreadSafeI64 DatabaseServices::sysstat_bxfind;
ThreadSafeI64 DatabaseServices::sysstat_bxfree;
ThreadSafeI64 DatabaseServices::sysstat_bxinse;
ThreadSafeI64 DatabaseServices::sysstat_bxnext;
ThreadSafeI64 DatabaseServices::sysstat_bxrfnd;
ThreadSafeI64 DatabaseServices::sysstat_bxspli;
ThreadSafeI64 DatabaseServices::sysstat_dirrcd;
ThreadSafeI64 DatabaseServices::sysstat_finds;
ThreadSafeI64 DatabaseServices::sysstat_recadd;
ThreadSafeI64 DatabaseServices::sysstat_recdel;
ThreadSafeI64 DatabaseServices::sysstat_recds;
ThreadSafeI64 DatabaseServices::sysstat_strecds;
ThreadSafeI64 DatabaseServices::sysstat_sorts;

ThreadSafeI64 DatabaseServices::sysstat_dkpr;
ThreadSafeI64 DatabaseServices::sysstat_dkrd;
ThreadSafeI64 DatabaseServices::sysstat_dkwr;
ThreadSafeI64 DatabaseServices::sysstat_fbwt;
ThreadSafeI64 DatabaseServices::sysstat_dksfbs;
ThreadSafeI64 DatabaseServices::sysstat_dksfnu;
ThreadSafeI64 DatabaseServices::sysstat_dkskip;
ThreadSafeI64 DatabaseServices::sysstat_dkskipt;
ThreadSafeI64 DatabaseServices::sysstat_dkswait;
ThreadSafeI64 DatabaseServices::sysstat_dkuptime;

ThreadSafeI64 DatabaseServices::sysstat_blkcfre;
ThreadSafeI64 DatabaseServices::sysstat_wtcfr;
ThreadSafeI64 DatabaseServices::sysstat_wtrlk;

ThreadSafeI64 DatabaseServices::sysstat_ilmradd;
ThreadSafeI64 DatabaseServices::sysstat_ilmrdel;
ThreadSafeI64 DatabaseServices::sysstat_ilmrmove;
ThreadSafeI64 DatabaseServices::sysstat_ilradd;
ThreadSafeI64 DatabaseServices::sysstat_ilrdel;
ThreadSafeI64 DatabaseServices::sysstat_ilsadd;
ThreadSafeI64 DatabaseServices::sysstat_ilsdel;
ThreadSafeI64 DatabaseServices::sysstat_ilsmove;

ThreadSafeI64 DatabaseServices::sysstat_merges;
ThreadSafeI64 DatabaseServices::sysstat_stvals;
ThreadSafeI64 DatabaseServices::sysstat_mrgvals;

static std::string NULLSTRING;

//**********************************************************************************************
//Constructors
//First one is where the output object is passed in
//**********************************************************************************************
DatabaseServices::DatabaseServices( 
  util::LineOutput* op,
  const std::string& userid,
  const std::string& parm_ini_filename, 
  const std::string& msgctl_ini_filename,
  const std::string& audit_filename,
  util::LineOutput* secondary_audit,
  const std::string& password)
: core(NULL), groups(NULL), seqfiles(NULL), output(op),
  op_created_here(false), update_unit(NULL)
#ifdef _DEBUG_LOCKS
  , lockin_lock(std::string("DBS Lockin: user ").append(userid))
  , context_lock(std::string("DBS Contexts: user ").append(userid))
#endif
{
	//Nothing extra to do here so just use the shared code
	DatabaseServices_S(output, userid, parm_ini_filename, 
		msgctl_ini_filename, audit_filename, secondary_audit, password);
}

//**********************************************************************************************
//Second one is where the output object is created locally.  Nicer for API programs.
//**********************************************************************************************
DatabaseServices::DatabaseServices( 
  const std::string& opf,
  const std::string& userid,
  const std::string& parm_ini_filename, 
  const std::string& msgctl_ini_filename,
  const std::string& audit_filename,
  const std::string& password)
: core(NULL), groups(NULL), seqfiles(NULL), output(NULL),
  op_created_here(true), update_unit(NULL)
{


//Do we need prtkeep here?
	//First create the all-important output file name.  Register it as a system file to 
	//prevent it being used for the audit trail etc.  Use the "dsn" as the "dd" too,
	//even though it will be much longer than usual.
//Probably the ideal would be for the user to specify their own DD name for the
	//user's output file.


	//V2.02 Feb 07.
	if (opf == "CONSOLE") {
		try {
			output = new util::StdioConsoleLineIO;
			DatabaseServices_S(output, userid, parm_ini_filename, 
				msgctl_ini_filename, audit_filename, NULL, password);
		}
		catch (...) {
			if (output) 
				delete output;
			throw;
		}
	}

	//This is as per pre Feb 07.
	else {
		try {
			std::string output_filename = opf;
			op_handle = SystemFile::Construct("+SYSPRNT", output_filename, BOOL_EXCL);
			output = new util::StdIOLineOutput(output_filename.c_str(), util::STDIO_CCLR);

			DatabaseServices_S(output, userid, parm_ini_filename, 
				msgctl_ini_filename, audit_filename, NULL, password);
		}
		catch (...) {
			if (output) 
				delete output;
			SystemFile::Destroy(op_handle);
			throw;
		}
	}
}

//**********************************************************************************************
//Common code used by both constructors
//**********************************************************************************************
void DatabaseServices::DatabaseServices_S(
  util::LineOutput* output,
  const std::string& userid,
  const std::string& parm_ini_filename, 
  const std::string& msgctl_ini_filename,
  const std::string& audit_filename,
  util::LineOutput* secondary_audit,
  const std::string& password
  )
{
	locked_in = false;
	refcount = 0;
	destroyed = false;

	stat_backouts.SetSys(&sysstat_backouts);
	stat_commits.SetSys(&sysstat_commits);
	stat_updttime.SetSys(&sysstat_updttime);

	stat_badd.SetSys(&sysstat_badd);
	stat_bchg.SetSys(&sysstat_bchg);
	stat_bdel.SetSys(&sysstat_bdel);
	stat_bxdel.SetSys(&sysstat_bxdel);
	stat_bxfind.SetSys(&sysstat_bxfind);
	stat_bxfree.SetSys(&sysstat_bxfree);
	stat_bxinse.SetSys(&sysstat_bxinse);
	stat_bxnext.SetSys(&sysstat_bxnext);
	stat_bxrfnd.SetSys(&sysstat_bxrfnd);
	stat_bxspli.SetSys(&sysstat_bxspli);
	stat_dirrcd.SetSys(&sysstat_dirrcd);
	stat_finds.SetSys(&sysstat_finds);
	stat_recadd.SetSys(&sysstat_recadd);
	stat_recdel.SetSys(&sysstat_recdel);
	stat_recds.SetSys(&sysstat_recds);
	stat_strecds.SetSys(&sysstat_strecds);
	stat_sorts.SetSys(&sysstat_sorts);

	stat_dkpr.SetSys(&sysstat_dkpr);
	stat_dkrd.SetSys(&sysstat_dkrd);
	stat_dkwr.SetSys(&sysstat_dkwr);
	stat_fbwt.SetSys(&sysstat_fbwt);
	stat_dksfbs.SetSys(&sysstat_dksfbs);
	stat_dksfnu.SetSys(&sysstat_dksfnu);
	stat_dkskip.SetSys(&sysstat_dkskip);
	stat_dkskipt.SetSys(&sysstat_dkskipt);
	stat_dkswait.SetSys(&sysstat_dkswait);
	stat_dkuptime.SetSys(&sysstat_dkuptime);

	stat_blkcfre.SetSys(&sysstat_blkcfre);
	stat_wtcfr.SetSys(&sysstat_wtcfr);
	stat_wtrlk.SetSys(&sysstat_wtrlk);

	stat_ilmradd.SetSys(&sysstat_ilmradd);
	stat_ilmrdel.SetSys(&sysstat_ilmrdel);
	stat_ilmrmove.SetSys(&sysstat_ilmrmove);
	stat_ilradd.SetSys(&sysstat_ilradd);
	stat_ilrdel.SetSys(&sysstat_ilrdel);
	stat_ilsadd.SetSys(&sysstat_ilsadd);
	stat_ilsdel.SetSys(&sysstat_ilsdel);
	stat_ilsmove.SetSys(&sysstat_ilsmove);

	stat_merges.SetSys(&sysstat_merges);
	stat_stvals.SetSys(&sysstat_stvals);
	stat_mrgvals.SetSys(&sysstat_mrgvals);

	try {
		core = new CoreServices(output, userid, parm_ini_filename, 
			msgctl_ini_filename, audit_filename, secondary_audit, password);

		//Enable commands etc. to access all users' objects by user number
		LockingSentry s(&instances_lock);
		int userno = core->GetUserNo();
		if (userno == 0) 
			instances.resize(CoreServices::MaxThreads(), NULL);
		instances[userno] = this;
		num_instances.Inc();

		//This is required for the viewer to access file parms (since CFR locks may be 
		//required, which in turn means setting a WT value).
		core->GetViewerResetter()->dbapi = this;

		groups = new GroupServices(core);
		seqfiles = new SequentialFileServices(this);

		//Register parms
		Core()->GetViewerResetter()->Register("BTREEPAD", this);
		Core()->GetViewerResetter()->Register("BUFAGE", this);
		Core()->GetViewerResetter()->Register("CPTIME", this);
		Core()->GetViewerResetter()->Register("DUFILES", this);
		Core()->GetViewerResetter()->Register("CODESA2E", this);
		Core()->GetViewerResetter()->Register("CODESE2A", this);
		Core()->GetViewerResetter()->Register("CPTO", this);
		Core()->GetViewerResetter()->Register("DTSLCHKP", this);
		Core()->GetViewerResetter()->Register("DTSLRCVY", this);
		Core()->GetViewerResetter()->Register("DTSLDKWR", this);
		Core()->GetViewerResetter()->Register("DTSLUPDT", this);
		Core()->GetViewerResetter()->Register("ENQRETRY", this);
		Core()->GetViewerResetter()->Register("FMODLDPT", this);
		Core()->GetViewerResetter()->Register("LOADCTL", this);
		Core()->GetViewerResetter()->Register("LOADDIAG", this);
		Core()->GetViewerResetter()->Register("LOADFVPC", this);
		Core()->GetViewerResetter()->Register("LOADMEMP", this);
		Core()->GetViewerResetter()->Register("LOADMMFH", this);
		Core()->GetViewerResetter()->Register("LOADTHRD", this);
		Core()->GetViewerResetter()->Register("MAXBUF", this);
		Core()->GetViewerResetter()->Register("MAXRECNO", this);
		Core()->GetViewerResetter()->Register("MBSCAN", this);
		Core()->GetViewerResetter()->Register("MDKRD", this);
		Core()->GetViewerResetter()->Register("MDKWR", this);
		Core()->GetViewerResetter()->Register("PAGESZ", this);
		Core()->GetViewerResetter()->Register("RCVOPT", this);
		Core()->GetViewerResetter()->Register("UPDTID", this);

		//User parms
		parm_enqretry = GetIniValueInt("ENQRETRY");
		parm_mdkrd = GetIniValueInt("MDKRD");
		parm_mdkwr = GetIniValueInt("MDKWR");
		parm_mbscan = GetIniValueInt("MBSCAN");

		//Only ever one of these objects per user - simpler code for Backout() this way
		update_unit = new UpdateUnit(this);
	}
	catch (...) {
		if (core) delete core;
		if (groups) delete groups;
		if (seqfiles) delete seqfiles;
		if (update_unit) delete update_unit;
		throw;
	}

	if (num_instances.Value() == 1) {
		try {
			_tzset();

			take_final_checkpoint = false;

			//Round MAXBUF down to a multiple of 32 for simpler buffer handling.
			//Not a major requirement but results from use of the tuned bitmap class.
			static_parm_maxbuf = GetIniValueInt("MAXBUF");
			static_parm_maxbuf = static_parm_maxbuf - (static_parm_maxbuf % 32);

			static_parm_rcvopt = GetIniValueInt("RCVOPT");
			static_parm_btreepad = GetIniValueInt("BTREEPAD");
			static_parm_cpto.Set(GetIniValueInt("CPTO"));
			static_parm_cptime.Set(GetIniValueInt("CPTIME"));
			static_parm_bufage.Set(GetIniValueInt("BUFAGE"));
			static_parm_fmodldpt.Set(GetIniValueInt("FMODLDPT"));
			static_parm_maxrecno.Set(GetIniValueInt("MAXRECNO"));
			static_parm_loadctl.Set(GetIniValueInt("LOADCTL"));
			static_parm_loaddiag.Set(GetIniValueInt("LOADDIAG"));
			static_parm_loadfvpc.Set(GetIniValueInt("LOADFVPC"));
			static_parm_loadmemp.Set(GetIniValueInt("LOADMEMP"));
			static_parm_loadmmfh.Set(GetIniValueInt("LOADMMFH"));

			//int ncpus = win::GetProcessorCount();
			//static_parm_loadthrd.Set(GetIniValueInt("LOADTHRD", &ncpus));
			static_parm_loadthrd.Set(GetIniValueInt("LOADTHRD")); //1 is usually better

			DatabaseFile::InitializeBuffers(static_parm_maxbuf, ChkpIsEnabled());
			ReservedWords::Initialize();

			//See comments in BTreePage about why this is here.  Also see tech docs.
			BTreePage::SetNodeFiller(static_parm_btreepad);

			core->AuditLine(
				std::string
				("System DatabaseServices initialization complete (MAXBUF = ")
				.append(util::IntToString(static_parm_maxbuf)).append(1, ')'), "SYS");
		}
		catch (...) {
			DatabaseFile::ClosedownBuffers(NULL);

			delete core;
			delete groups;
			delete seqfiles;
			delete update_unit;
			throw;
		}
	}

	//Initialize stats
	StatViewer* sv = core->GetStatViewer();
	RegisterHolder(sv);

	RegisterStat("BACKOUTS", sv);
	RegisterStat("COMMITS", sv);
	RegisterStat("UPDTTIME", sv);

	RegisterStat("BADD", sv);
	RegisterStat("BCHG", sv);
	RegisterStat("BDEL", sv);
	RegisterStat("BXDEL", sv);
	RegisterStat("BXFIND", sv);
	RegisterStat("BXFREE", sv);
	RegisterStat("BXINSE", sv);
	RegisterStat("BXNEXT", sv);
	RegisterStat("BXRFND", sv);
	RegisterStat("BXSPLI", sv);
	RegisterStat("DIRRCD", sv);
	RegisterStat("FINDS", sv);
	RegisterStat("RECADD", sv);
	RegisterStat("RECDEL", sv);
	RegisterStat("RECDS", sv);
	RegisterStat("STRECDS", sv);
	RegisterStat("SORTS", sv);

	RegisterStat("DKPR", sv);
	RegisterStat("DKRD", sv);
	RegisterStat("DKWR", sv);
	RegisterStat("FBWT", sv);
	RegisterStat("DKSFBS", sv);
	RegisterStat("DKSFNU", sv);
	RegisterStat("DKSKIP", sv);
	RegisterStat("DKSKIPT", sv);
	RegisterStat("DKSWAIT", sv);
	RegisterStat("DKUPTIME", sv);

	RegisterStat("BLKCFRE", sv);
	RegisterStat("WTCFR", sv);
	RegisterStat("WTRLK", sv);

	RegisterStat("ILMRADD", sv);
	RegisterStat("ILMRDEL", sv);
	RegisterStat("ILMRMOVE", sv);
	RegisterStat("ILRADD", sv);
	RegisterStat("ILRDEL", sv);
	RegisterStat("ILSADD", sv);
	RegisterStat("ILSDEL", sv);
	RegisterStat("ILSMOVE", sv);

	RegisterStat("MERGES", sv);
	RegisterStat("STVALS", sv);
	RegisterStat("MRGVALS", sv);

#ifdef DPT_BUILDING_CAPI_DLL
	capi_commarea = new struct CAPICommArea(this);
#endif

	//-------------------------------------------------------
	core->GetRouter()->Issue(USER_STARTED_LEVEL2, std::string
		("User=")
		.append(core->GetUserID())
		.append(" DatabaseServices started"));
}

//**********************************************************************************************
//Destructor
//**********************************************************************************************
void DatabaseServices::Destroy()
{
	//Wait till any other users' code has finished with us
	while (IsLockedIn()) 
		Sleep(100);

	//V2.06 Jun 07.  To support unpredictable destruction timing in garbage-collected 
	//environments, you can now force destruction of the contents of this object.
	if (destroyed)
		return;
	else
		destroyed = true;

	LockingSentry s(&instances_lock);
	instances[core->GetUserNo()] = NULL;
	num_instances.Dec();

	//If the user thread terminates without committing, we assume they want to keep their
	//work but mark the file logically inconsistent.
	update_unit->AbruptEnd();

	//In an error situation some of the open contexts might still have associated objects
	//open, so we use the "force" option when closing them, to ensure they're deleted too.
	CloseAllContexts(true);

	core->GetRouter()->Issue(USER_FINISHED_LEVEL2, std::string
		("User=")
		.append(core->GetUserID())
		.append(" DatabaseServices terminating"));

	//Free off any remaining allocated files when the last user terminates.  Need not
	//be the first user to have logged on - i.e. user zero, although it will be in the
	//main host system.
	if (num_instances.Value() == 0) {
		core->AuditLine("System DatabaseServices closing down", "SYS");
		core->AuditLine("(Please wait for any updated pages to flush)", "SYS");

		//A final checkpoint
		if (take_final_checkpoint) {
			try {
				Checkpoint(-1); //immediate
			}
			catch (...) {
				core->GetRouter()->Issue(CHKP_ABORTED, 
					"Serious closedown error - the final checkpoint could not be performed. "
					"Recovery will probably be required next time.");
			}
		}

		std::vector<FileHandle> info;
		AllocatedFile::ListAllocatedFiles(info, BOOL_EXCL, FILETYPE_DB);

		for (size_t x = 0; x < info.size(); x++) {

//			DatabaseFile* f = static_cast<DatabaseFile*>(info[x].GetFile());
			const std::string& dd = info[x].GetDD();
			
			//Try/catch to ensure each file gets a turn.
			//Since this is the last user, the frees should in theory not fail.
			try {
				std::string msg("Database file freed at system closedown: ");
				msg.append(dd);

				info[x].StageForFree();
				DatabaseFile::Destroy(this, info[x]);
				core->GetRouter()->Issue(SYSFILE_FREED_FINAL, msg);
			}
			catch (...) {
				std::string msg("Bug! FREE failed at system closedown: ");
				msg.append(dd);
				core->GetRouter()->Issue(BUG_MISC, msg);
			}
		}

		//User 0 frees all the buffer stuff
		DatabaseFile::ClosedownBuffers(this);
	}

	delete update_unit;
	update_unit = NULL;

	delete seqfiles;
	delete groups;
	delete core;

	//Review this now that we have the iodev installed in coreservices as the output
	//device.  Looks OK but might not be.  Might not even need op in this class - it's
	//not currently used in any of the funcs in this file - all messaging is via core
	//after all.
	if (op_created_here) {
		//V2.02 Feb 07.  CONSOLE API will not have a system file allocated.
		if (op_handle.IsEnabled())
			SystemFile::Destroy(op_handle);

		if (output) 
			delete output;
	}

	if (num_instances.Value() == 0) {

		//V3.0 Individual reorgs clean up after themselves, but this may remain
		try {
			win::RemoveDirectoryTree("#REORGS");
		}
		catch (...) {}


		//V2.27.  Clear up all the files we just created - audit trail etc.
		if (delete_dir_at_closedown != "") {
			try {
				_chdir("..");
				win::RemoveDirectoryTree(delete_dir_at_closedown.c_str());

				//Deleting this whole directory is good because it means any lingering
				//stuff from previous crashes eventually gets cleaned up too.  If other
				//instances are running (i.e. enqueing files) it'll just get left.
				_chdir("..");
				win::RemoveDirectoryTree("#DPTTEMP");
			}
			catch (...) {}
		}
	}

#ifdef DPT_BUILDING_CAPI_DLL
	if (capi_commarea) 
		delete capi_commarea;
#endif
}

//**********************************************************************************************
//Used in the IODev7 scenario specifically but would I guess be valid any time from API
//**********************************************************************************************
void DatabaseServices::SwitchOutputDevice(util::LineOutput* op)
{
	output = op;
	core->SwitchOutputDevice(op);
}

//**********************************************************************************************
//See comments at the session level - these are just the same but for API programs
//**********************************************************************************************
DatabaseServices* DatabaseServices::GetHandleAndLockIn(int u)
{
	SharingSentry s(&instances_lock);

	if (u >= CoreServices::MaxThreads()) return NULL;
	
	DatabaseServices* d = instances[u];
	if (d != NULL) 
		d->LockIn();

	return d;
}
void DatabaseServices::LockIn() {LockingSentry s(&lockin_lock); locked_in = true;}
void DatabaseServices::LetOut() {LockingSentry s(&lockin_lock); locked_in = false;}
bool DatabaseServices::IsLockedIn() {LockingSentry s(&lockin_lock); return locked_in;}


//**********************************************************************************************
//V2.27.  Added this for API programs where you wouldn't necessarily want to go through the
//full "mainframe" rigmarole to "install" DPT in fresh directories all over the place, and the
//API program can execute several instances and not get clashes with audit, checkpoint file etc.
//**********************************************************************************************
void DatabaseServices::CreateAndChangeToUniqueWorkingDirectory(bool delete_at_closedown) 
{
	if (num_instances.Value() != 0)
		throw Exception(MISC_DEBUG_INFO, 
			"CreateAndChangeToUniqueWorkingDirectory() must be called before "
			"logging any users on");

	std::string dirname = util::StartupWorkingDirectory();

	//Put them all in here.  It may exist if another instance is already running.
	dirname.append("\\#DPTTEMP");
	util::StdioEnsureDirectoryExists(dirname.c_str());

	//Generate a unique subdirectory name from the time
	dirname.append("\\RUN_").append(util::IntToString(time(NULL)));

	//Create and change to it
	if (_mkdir(dirname.c_str()) || _chdir(dirname.c_str()))
		throw Exception(UTIL_STDIO_ERROR, win::GetLastErrorMessage(true));

	//V3.0. Future calls to StartupWorkingDirectory() should return this now
	util::NoteChangedStartupWorkingDirectory(dirname);

	//Remember it for delete at closedown
	if (delete_at_closedown)
		delete_dir_at_closedown = dirname;
}



//*****************************************************************************************
//File allocation and freeing
//*****************************************************************************************
FileHandle DatabaseServices::FindDatabaseFile(const std::string& dd, bool locktype)
{
	//Locate the file object
	FileHandle fh = AllocatedFile::FindAllocatedFile(dd, locktype, FILETYPE_DB);

	//M204 doesn't report this.  Note that the FREE command will catch this exception and
	//then try a sequential file instead, only reporting an error if both fail.
	if (fh.GetFile() == NULL)
		throw Exception(SYSFILE_NOT_ALLOCATED, std::string
			("Database file is not allocated: ").append(dd));

	if (fh.GetType() != FILETYPE_DB)
		throw Exception(SYSFILE_BAD_TYPE, std::string
			("File is allocated, but not a database file: ").append(dd));

	return fh;
}

//*****************************************************************************************
void DatabaseServices::Allocate
(const std::string& dd, const std::string& dsn, FileDisp disp, const std::string& alias)
{
	std::string realdsn = dsn;

	//The constructor will throw all the exceptions we're interested in.  We don't give
	//the handle/pointer back to the application - it should use OPEN to get a context.
	FileHandle fh = DatabaseFile::Construct(dd, realdsn, disp, alias);

	//See docs for info on why we checkpoint here.
	try {
		Checkpoint_S(-1, &fh);
		fh.CommitAllocation();
	}
	catch (...) {
		DatabaseFile::Destroy(this, fh);
		throw;
	}

	std::string msg("Database file allocated: ");
	msg.append(dd).append(1, '=').append(realdsn);
	if (alias.length() != 0)
		msg.append(" (AltName=").append(alias).append(1, ')');
	core->GetRouter()->Issue(SYSFILE_ALLOCATED, msg);
}

//*****************************************************************************************
void DatabaseServices::Free(const std::string& dd)
{
	FileHandle fh = FindDatabaseFile(dd, BOOL_EXCL);
	fh.StageForFree();

	//See docs for info on why we checkpoint here.  Note that checkpoint success is
	//an absolute imperative for freeing a file, since if it's been updated the whole 
	//transaction control infrastructure is riddled with pointers to the DatabaseFile 
	//object we're about to destroy.
	try {
		Checkpoint_S(-1);
	}
	catch (...) {
		fh.CommitAllocation();
		throw;
	}

	std::string alias = fh.GetAlias();
	std::string realdd = fh.GetDD();

	//OK - safe to delete now.  Note that passing the pointer into Checkpoint above
	//told the algorithm not to write the file to the fresh file list in the CP file.
	DatabaseFile::Destroy(this, fh);

	std::string msg = std::string("Database file freed: ").append(realdd);
	if (alias.length() != 0)
		msg.append(" (AltName=").append(alias).append(1, ')');

	core->GetRouter()->Issue(SYSFILE_FREED, msg);
}

//*****************************************************************************************
//This is provided in database services to save the user ever having to deal with
//DatabaseFile objects.
//*****************************************************************************************
void DatabaseServices::Create(const std::string& dd, 		
	int bsize, int brecppg, int breserve, int breuse, 
	int dsize, int dreserve, int dpgsres, int fileorg)
{
	FileHandle fh = FindDatabaseFile(dd, BOOL_SHR);
	DatabaseFile* f = static_cast<DatabaseFile*>(fh.GetFile());

	//Do this here before the implied checkpoint - just nicer I think
	FileOpenLockSentry fols(f, BOOL_EXCL, false);

	//Because of the OS file resize, undoing after that is outside our control, as
	//we can't prevent other OS apps taking any space we release.  Since it will not
	//therefore be possible to roll back across a create, we take a checkpoint here.
	Checkpoint_S(-1);

	//We have an exclusive handle so no need for FILE CFR during this
	f->Create(this, bsize, brecppg, breserve, breuse, dsize, dreserve, dpgsres, fileorg);
}














//*****************************************************************************************
//Functions used during context opening.
//*****************************************************************************************
DatabaseFileContext* DatabaseServices::CreateContext(DefinedContext* dc)
{
	DatabaseFileContext* result;

	if (dc->GetGroup()) 
		result = new GroupDatabaseFileContext(dc, this);
	else 
		result = new SingleDatabaseFileContext(dc, this);

	context_directory[result->GetFullName()] = result;

	core->GetRouter()->Issue(MISC_DEBUG_INFO, std::string
		("Database context created: ").append(result->GetFullName()));

	return result;
}

//*****************************************************************************************
//This function is shared code between this class and GroupOpenableContext::Open().
//This is where the context is marked as actually open.
//With single files, the second function parameter denotes whether the file is being opened 
//as part of a group or not.
//*****************************************************************************************
bool DatabaseServices::OpenContext_Single(DatabaseFileContext* context, 
const std::string& du_parm1, const std::string& du_parm2, int du_parm3, DUFormat du_parm4, 
const GroupDatabaseFileContext* parent_group)
{
	bool really_opened = context->Open(parent_group, du_parm1, du_parm2, du_parm3, du_parm4);

	core->GetRouter()->Issue(DB_OPENED, std::string
		("Database context opened: ").append(context->GetFullName()));

	return really_opened;
}

//*****************************************************************************************
//This corresponds to the OPEN command issued by a user, as opposed to the case where a
//file is opened as a group member.
//*****************************************************************************************
DatabaseFileContext* DatabaseServices::OpenContext_Allparms
(const ContextSpecification& spec, bool reopen, bool* notify_opened, 
 const std::string& du_parm1, const std::string& du_parm2, int du_parm3, int du_parm4)
{
	LockingSentry ls(&context_lock);
		
	//Determine what is meant by the specification - e.g. a new group may have been defined
	//since a recent, identical open command.
	DefinedContext* dc = DefinedContext::Create(groups, spec);

	//First decide if it's open already.
	std::string fullname = dc->GetFullName();
	std::map<std::string, DatabaseFileContext*>::const_iterator 
		i = context_directory.find(fullname);

	//See comments in procserv.cpp about this next bit
	DatabaseFileContext* result = NULL;
	if (i != context_directory.end()) {
		delete dc;
		result = i->second;

		//See comments above
		if (result->CastToGroup() || !reopen) {
			if (notify_opened)
				*notify_opened = false;
		}
		else {
			bool really_opened = OpenContext_Single
				(result, du_parm1, du_parm2, du_parm3, du_parm4);
			if (notify_opened)
				*notify_opened = really_opened;
		}

		return result;
	}

	//No it's not, so give it the full processing.
	//NB. We don't issue failure messages here, but leave that to the caller.  The reason 
	//is that the caller is usually the OPEN command, in which case it only issues error 
	//messages if neither a procedure nor a database context could be opened.
	try {
		result = CreateContext(dc);
		OpenContext_Single(result, du_parm1, du_parm2, du_parm3, du_parm4);

		if (notify_opened)
			*notify_opened = true;

		return result;
	}
	catch (...) {
		if (result)
			DeleteContext(result);
		throw;
	}
}

//*****************************************************************************************
//V2.14 Jan 09.  Added these just to kluge the old and new Opens into the above.
DatabaseFileContext* DatabaseServices::OpenContext(const ContextSpecification& spec) 
{
	return OpenContext_Allparms(spec, false, NULL, NULLSTRING, NULLSTRING, 0, 0);
}

//********************************************************
DatabaseFileContext* DatabaseServices::OpenContext_DUMulti
(const ContextSpecification& spec, const std::string& duname_n, const std::string& duname_a, 
 int du_numlen, DUFormat du_format) 
{
	return OpenContext_Allparms(spec, false, NULL, duname_n, duname_a, du_numlen, du_format);
}

//********************************************************
DatabaseFileContext* DatabaseServices::OpenContext_DUSingle(const ContextSpecification& spec)
{
	return OpenContext_Allparms(spec, false, NULL, "1STEPLOAD", NULLSTRING, -1, -1);
}

//********************************************************
//Used in APSY and a couple other places
DatabaseFileContext* DatabaseServices::OpenContext_B(const ContextSpecification& spec, bool b) 
{
	return OpenContext_Allparms(spec, b, NULL, NULLSTRING, NULLSTRING, 0, 0);
} 

//*****************************************************************************************
//Functions used during Close
//*****************************************************************************************
void DatabaseServices::DeleteContext(DatabaseFileContext* context)
{
	std::string fullname = context->GetFullName();
	context_directory.erase(fullname);
	delete context;

	core->GetRouter()->Issue(MISC_DEBUG_INFO, std::string
		("Database context deleted: ").append(fullname));
}

//*****************************************************************************************
//This function is shared code between this class and GroupOpenableContext::Close().
//This is where we ensure that it's ok to delete the object, and any other pre-close 
//processing.  With groups, the member files are closed by a recursive
//sequence of function calls, initiated by this Close() call, and which will come through 
//here a number of times.  
//With single files, the second function parameter denotes whether the file is being closed 
//as part of a group or not.
//The return code says whether any other contexts are using this one (i.e. close a single
//file and it's also owned by a group, or close a group member and its also open as a
//single file in its own right, or another group).  Thus the caller knows whether the 
//context can be deleted.
//Exceptions thrown mean that the close has failed because there are still associated
//objects in use.  In such cases, the third parameter can be used to force them
//closed, so no exceptions should then be thrown.
//*****************************************************************************************
bool DatabaseServices::CloseContext_Single
(DatabaseFileContext* context, const GroupDatabaseFileContext* parent_group, bool force)
{
	bool fully_closed = context->Close(parent_group, force);

	std::string msg;
	if (fully_closed) 
		msg = "Database context closed: ";
	else {
		if (parent_group)
			msg = "Database file remains open in other groups or on its own: ";
		else 
			msg = "Database file remains open in at least one group: ";
	}
	msg.append(context->GetFullName());
	core->GetRouter()->Issue(DB_CLOSED, msg);

	//The caller uses this to decide re. deletion
	return fully_closed;
}

//*****************************************************************************************
//This corresponds to the CLOSE command issued by a user, as opposed to the case where a
//file is closed as a group member.
//The bool result is provided for calling code to easily see if the pointer passed in is
//still valid.
//*****************************************************************************************
bool DatabaseServices::CloseContext(DatabaseFileContext* context)
{
	LockingSentry ls(&context_lock);

	//I'm ambivalent about this check, as the user in theory has no need to call with an
	//invalid pointer.  Closing a file is not usually performance sensitive though.
	std::map<std::string, DatabaseFileContext*>::const_iterator i = context_directory.begin();
	for (;;) {
		if (i == context_directory.end())
			throw Exception(CONTEXT_IS_NOT_OPEN, 
				"Invalid/obsolete context passed to DatabaseServices::Close()");
		if (i->second == context)
			//OK, found it
			break;
		i++;
	}

	//The context is left open if child objects still exist.  In other words an API
	//user can't force close the context, although we do this internally if there are
	//serious errors - see force parameter elsewhere.  It was thought that it would be
	//more useful for users to see when they are leaving sets as that might indicate
	//a failure to commit updates etc.
	if (CloseContext_Single(context)) {
		DeleteContext(context);
		return true;
	}
	else {
		return false;
	}
}

//*****************************************************************************************
//As used at e.g. logoff or for the CLOSE all command.
//If the force option is specified it means close all procedures and cursors too.  This
//would obviously be used with care - usually just at logoff time.  And even then on an
//IODev7 the LOGOFF command should trigger a managed closedown of everythting.
//*****************************************************************************************
void DatabaseServices::CloseAllContexts(bool force_option)
{
	LockingSentry ls(&context_lock);

	//Here we want to make sure we have just one go at closing each context, and do the
	//groups first, so that any remaining single files will successfully close at the end.
	//therefore we start at the end (i.e. TEMP GROUP, then PERM GROUP, then FILE).
	std::map<std::string, DatabaseFileContext*>::iterator pci = context_directory.end();

	while (pci != context_directory.begin()) {
		pci--;

//		std::string cname = pci->first;

		//This should therefore never happen
		bool completely_closed = CloseContext_Single(pci->second, NULL, force_option);
		assert(completely_closed);

		DeleteContext(pci->second);
		pci = context_directory.end();
	}
}

//*****************************************************************************************
DatabaseFileContext* DatabaseServices::FindOpenContext
(const ContextSpecification& spec) const
{
	DefinedContext* dc = DefinedContext::Create(groups, spec);
	std::map<std::string, DatabaseFileContext*>::const_iterator i;
	i = context_directory.find(dc->GetFullName());
	delete dc;

	if (i == context_directory.end()) 
		return NULL;
	else 
		return i->second;
}

//*****************************************************************************************
//Used in D FILE, LOGWHO etc.
//*****************************************************************************************
std::vector<DatabaseFileContext*> DatabaseServices::ListOpenContexts
(bool include_singles, bool include_groups) const
{
	std::vector<DatabaseFileContext*> result;

	//Ensure the user's set of open files remains stable while we look at it
	LockingSentry ls(&context_lock);

	std::map<std::string, DatabaseFileContext*>::const_iterator i;
	for (i = context_directory.begin(); i != context_directory.end(); i++) {
		if (i->second->CastToGroup()) {
			if (include_groups)
				result.push_back(i->second);
		}
		else {
			if (include_singles)
				result.push_back(i->second);
		}
	}

	return result;
}

//*****************************************************************************************
//To handle dirty delete reasonably efficiently.  This just makes use of the existing 
//data structures that are held for other reasons, and places some overhead on the thread
//which is doing the dirty delete.  Most of the time this function is done in single
//user mode anyway, so this function will be trivial to execute.
//Note that we notify all record sets, not just unlocked ones, because locked sets on
//our own thread will need to know too.  Let's keep it simple.
//*****************************************************************************************
void DatabaseServices::NotifyAllUsersContextsOfDirtyDelete(BitMappedRecordSet* goners)
{
	std::vector<int> usernos = CoreServices::GetUsernos();

	for (size_t x = 0; x < usernos.size(); x++) {
		DBAPILockInSentry s(usernos[x]);

		DatabaseServices* api = s.GetPtr();
		if (!api)
			continue;

		//Stop them creating/deleting context objects
		LockingSentry s2(&api->context_lock);

		//Tell all the files that the dirty delete has been done
		std::map<std::string, DatabaseFileContext*>::const_iterator i2;
		for (i2 = api->context_directory.begin(); i2 != api->context_directory.end(); i2++)
			i2->second->NotifyRecordSetsOfDirtyDelete(goners);
	}
}















//******************************************************************************************
//Transaction control
//******************************************************************************************
bool DatabaseServices::FileIsBeingUpdated(DatabaseFile* f)
{
	assert(update_unit);
	return update_unit->FileIsParticipating(f);
}

//******************************************************************************************
void DatabaseServices::Commit(bool if_backoutable, bool if_nonbackoutable)
{
	assert(update_unit);
	if (update_unit->Commit(if_backoutable, if_nonbackoutable)) {
		stat_commits.Inc();

		//This is for $VIEW - I'm not sure if the required value is sys or file level - if
		//it's supposed to be file level we already have this in the FCT.
		//The FCT gets written when files are opened, so this value will often lag 
		//behind the last DKWR time, until a commit happens.
		time(&last_update_time);
	}
}

//******************************************************************************************
void DatabaseServices::Backout(bool discreet)
{
	assert(update_unit);
	bool any_backouts = update_unit->Backout(discreet);

	//The discreet parameter allows this function to be called blindly by assorted
	//error handling code without incrementing the stat or producing unwanted messages.
	if (any_backouts || !discreet)
		stat_backouts.Inc();
}

//******************************************************************************************
void DatabaseServices::AbortTransaction()
{
	assert(update_unit);
	update_unit->AbruptEnd();
}

//******************************************************************************************
bool DatabaseServices::UpdateIsInProgress()
{
	assert(update_unit);
	return (update_unit->ID() != -1);
}

#ifdef _BBHOST
//******************************************************************************************
void DatabaseServices::SpawnCheckpointPST(SessionData* sess)
{
	if (!ChkpIsEnabled()) //not doing checkpointing in this run
		return;
	if (GetParmCPTIME() == 0) //there is checkpointing but no PST is required
		return;
	if (CoreServices::MaxThreads() == 1) //single user mode
		return;

	//V2.20 Aug 09.  Moved test into Spawn().
//	if (DaemonInfo::IsLoggedOn("PST_C")) //the PST is already active
//		return;

	try {
		DaemonInfo* di = DaemonInfo::FindOrDefine("PST_C");
		di->Spawn(sess, "=PSTCHECK", "");
	}
	catch (Exception& e) {
		core->GetRouter()->Issue(e.Code(), e.What());
		throw "bug if you see this";
	}
}

//******************************************************************************************
void DatabaseServices::SpawnBuffTidyPST(SessionData* sess)
{
	if (GetParmBUFAGE() == 0) //No PST required
		return;
	if (CoreServices::MaxThreads() == 1) //single user mode
		return;

	//V2.20 Aug 09.  Moved test into Spawn().
//	if (DaemonInfo::IsLoggedOn("PST_T")) //already logged on
//		return;

	try {
		DaemonInfo* di = DaemonInfo::FindOrDefine("PST_T");
		di->Spawn(sess, "=PSTTIDY", "");
	}
	catch (Exception& e) {
		core->GetRouter()->Issue(e.Code(), e.What());
		throw "bug if you see this";
	}
}
#endif













//******************************************************************************************
//Buffers, checkpointing, system management
//******************************************************************************************
void DatabaseServices::Checkpoint_S(int cpto, FileHandle* allocee)
{
	//Special value passed from an unadorned CHECKPOINT command or API func call
	if (cpto == -2)
		cpto = static_parm_cpto.Value();

	//If we get to the initial checkpoint, we can take one at the end too
	take_final_checkpoint = true;

	Commit();

	if (!ChkpIsEnabled())
		return;

	//This time is passed through and used everywhere during the checkpoint process.
	//This is important when comparing timestamps in various places in recovery etc.
	time_t cptime;
	time(&cptime);
	current_checkpoint_time.Set(cptime);

	std::string msg("Checkpoint starting: timestamp is ");	
	msg.append(win::GetCTime(cptime));

	//When doing an implied checkpoint we want it to happen quietly.
	int msgcode = (cpto == -1) ? CHKP_STARTING_NOTERM : CHKP_STARTING;
	core->GetRouter()->Issue(msgcode, msg);

	try {
		DatabaseFile::CheckpointProcessing(this, cpto, allocee);

		//Decided not to append time to this message.  The checkpoint should 
		//be thought of as all occurring in the same instant (see above comment).
		std::string msg = "Checkpoint complete";

		//Same comment as above
		int msgcode = (cpto == -1) ? CHKP_COMPLETE_NOTERM : CHKP_COMPLETE;
		core->GetRouter()->Issue(msgcode, msg);
	}
	catch (Exception& e) {
		if (cpto != -1)
			throw;
		
		throw Exception(CHKP_ABORTED, 
			std::string("Implied checkpoint failed (").append(e.What()).append(1, ')'));
	}
	catch (...) {
		if (cpto != -1)
			throw;
		throw Exception(CHKP_ABORTED, "Implied checkpoint failed (unknown reason)");
	}
}

//******************************************************************************************
void DatabaseServices::UpdateCheckpointTimes()
{
	recent_timed_out_checkpoints = 0;

	last_checkpoint_time.Set(current_checkpoint_time.Value());
	
	//Next checkpoint time is CPTIME from the end of the last (see PSTCHECK command)
	int cptime = static_parm_cptime.Value();
	if (cptime != 0)
		next_checkpoint_time.Set(time(NULL) + cptime * 60);
	else
		next_checkpoint_time.Set(0);
}

//******************************************************************************************
int DatabaseServices::Tidy(int bufage)
{
	//Special value passed from an unadorned TIDY command or API func call
	if (bufage == -2)
		bufage = static_parm_bufage.Value();

	Commit();

	//Retain any pages that have been used since the cutoff point.  BUFAGE of zero 
	//therefore means discard all pages not in use.
	time_t cutoff;
	time(&cutoff);
	cutoff -= bufage;

	WTSentry ws(core, 97);
	int numpages = DatabaseFile::BufferTidyProcessing(cutoff);

	if (numpages > 0) {
		core->GetRouter()->Issue(BUFF_TIDY_INFO, std::string("Buffer pool tidy-up: ")
			.append(util::IntToString(numpages))
			.append(" old page(s) discarded, ")
			.append(util::IntToString(numpages * DBPAGE_SIZE / 1024))
			.append("K of memory returned to OS"));
	}
	return numpages;
}

//******************************************************************************************
int DatabaseServices::Rollback1()
{
	try {
		if (!ChkpIsEnabled()) {
			Recovery::control_code = 1;
			return 1;
		}

		MsgRouter* router = core->GetRouter();

		CheckpointFile* chkp = CheckpointFile::Object();
		if (chkp->FLength() == 0) {
			router->Issue(ROLLBACK_NOTREQ, "Recovery is not required");
			Recovery::control_code = 1;
			return 1;
		}

		core->GetRouter()->Issue(ROLLBACK_START, 
			"Invoking recovery due to non-empty checkpoint file");

		Recovery::StartRollback(this);

		//----------------------------------
		//Phase 1: Pre-scan the CP file
		Recovery::RollbackPreScan();
		if (Recovery::Bypassed()) {
			router->Issue(ROLLBACK_BYPASSED, 
				"Recovery was bypassed entirely - one or more files may"
				" be physically inconsistent");
			Recovery::control_code = 1;
			return 1;
		}

		int num_required_files = Recovery::rb_required_files.size();
		if (num_required_files == 0) {
			router->Issue(ROLLBACK_COMPLETE, "No files required rollback");
			Recovery::control_code = 0;
			return 0;
		}

		router->Issue(ROLLBACK_FILEINFO, 
			std::string("Attempting to open ").append(util::IntToString(num_required_files))
			.append(" file(s)"));

		//----------------------------------
		//Phase 2: Allocate and open the files that need rolling back
		Recovery::RollbackOpenFiles();
		int num_failed_files = Recovery::rb_failed_files.size();

		if (Recovery::Bypassed()) {
			router->Issue(ROLLBACK_BYPASSED, 
				"Recovery was bypassed entirely - one or more files may"
				" be physically inconsistent");
			Recovery::control_code = 1;
			return 1;
		}

		//All opens failed - go no further and abort
		if (num_failed_files == num_required_files)
			throw Exception(ROLLBACK_BADFILES, 
				"None of the files which required rollback could "
				"be allocated and opened, so it was not worth continuing");

		//In auto mode no opens can fail at all
		if (num_failed_files > 0 && AutoRecoveryIsOn()) {
			throw Exception(ROLLBACK_ABORTED, 
				"One or more of the files which required rollback "
				"could not be allocated and opened");
		}

		//----------------------------------
		//Phase 3: Show the list of remaining files and prompt for whether to do a backup
		if ( ! (AutoRecoveryIsOn() && AutoRecoverySkipBackups()) )
			Recovery::RollbackBackup();

		if (Recovery::RBBackupsTaken())
			router->Issue(ROLLBACK_BACKUPS, "Backup(s) made successfully");
		else
			router->Issue(ROLLBACK_BACKUPS, "Backup(s) skipped");

		//----------------------------------
		//Phase 4: Roll back
		try {
			Recovery::Rollback();
			Recovery::RollbackDeleteBackups();
		}
		catch (Exception& e) {
			//If they cancel we quit now rather than restore
			if (e.Code() == ROLLBACK_CANCELLED)
				throw;

			Recovery::failed_code = e.Code();
			Recovery::failed_reason = e.What();
		}
		catch (...) {
			Recovery::failed_code = MISC_CAUGHT_UNKNOWN;
			Recovery::failed_reason = "unknown reason";
		}

		time(&last_recovery_time);

		//Optional phase 5: Restore if appropriate
		if (Recovery::failed_code) {

			router->Issue(Recovery::failed_code, std::string("Error during rollback (")
				.append(Recovery::failed_reason).append(1, ')'));

			if (Recovery::RBBackupsTaken()) {
				Recovery::RollbackRestore();
				Recovery::RollbackDeleteBackups();

				//If they said "bypass" it's the same as bypassing rollback a phase earlier
				if (Recovery::Bypassed())
					router->Issue(ROLLBACK_BACKUPS, "Backups were not reinstated");
				else {
					router->Issue(ROLLBACK_BACKUPS, 
						"The backup copies were successfully reinstated");

					//Now rethrow to terminate the run
					throw Exception(ROLLBACK_ABORTED, "see messages above");
				}
			}
		}

		//---------------------------------------------------------------------------------		
		if (Recovery::Bypassed()) {
			router->Issue(ROLLBACK_BYPASSED, 
				"Rollback was bypassed - one or more files may be physically inconsistent");

			//Set a pseudo "failure" reason for all files
			Recovery::RollbackSetFilesStatusBypassed();

			Recovery::control_code = 1; //no roll forward
			return 1;
		}
		else {
			router->Issue(ROLLBACK_COMPLETE, std::string("Rollback is complete: ")
			.append(util::IntToString(Recovery::RBPreImages()))
			.append(" pre-image(s) reapplied"));

			Recovery::control_code = 0; //can roll forward
			return 0;
		}
	}
	catch (dpt::Exception& e) {
		//Delay message issue till main dialog message loop freed up - ugly!
		Recovery::failed_code = e.Code();
		Recovery::failed_reason = e.What();
		Recovery::control_code = 2;
		return 2;
	}
	catch (...) {
		Recovery::failed_code = MISC_CAUGHT_UNKNOWN;
		Recovery::failed_reason = "unknown error";
		Recovery::control_code = 2;
		return 2;
	}
}

//******************************************************************************************
void DatabaseServices::Rollback2()
{
	//Delayed till now because the recovery worker threads detect "cancel" asynchronously
	//- we leave files open until the worker thread has detected it.
	if (Recovery::FailedCode() == ROLLBACK_CANCELLED) {
		Sleep(100);
		Sleep(100);
	}
	Recovery::RollbackCloseDatabaseFiles();

	if (Recovery::GetControlCode() != 2)
		Checkpoint(-1);
}

//******************************************************************************************
/*void DatabaseServices::Rollforward()
{
	if (Recovery::GetControlCode() != 0)
		return;
}*/


















//******************************************************************************************
//Parameter viewing and resetting
//******************************************************************************************
std::string DatabaseServices::ResetParm(
	const std::string& parmname, const std::string& newvalue)
{
	if (parmname == "BUFAGE") {
		static_parm_bufage.Set(util::StringToInt(newvalue));
		//Spawn the PST if appropriate (not API mode).  Zero will end the PST later.
#ifdef _BBHOST
		try {SpawnBuffTidyPST(Session::user0_session->Data());}
		catch (...) {}
#endif
		return newvalue;
	}
	if (parmname == "CPTO") {
		static_parm_cpto.Set(util::StringToInt(newvalue)); 
		return newvalue;
	}
	if (parmname == "CPTIME") {
		int cptime = util::StringToInt(newvalue);
		static_parm_cptime.Set(cptime);
		//Spawn the PST if appropriate (not API mode).  Zero will end the PST later.
#ifdef _BBHOST
		try {SpawnCheckpointPST(Session::user0_session->Data());}
		catch (...) {}
#endif
		if (cptime > 0) {
			time_t currtime = time(NULL);
			next_checkpoint_time.Set(currtime + cptime * 60);
		}
		else
			next_checkpoint_time.Set(0);

		return newvalue;
	}
	if (parmname == "MAXRECNO") {static_parm_maxrecno.Set(util::StringToInt(newvalue)); return newvalue;}
	if (parmname == "LOADCTL") {static_parm_loadctl.Set(util::StringToInt(newvalue)); return newvalue;}
	if (parmname == "LOADDIAG") {static_parm_loaddiag.Set(util::StringToInt(newvalue)); return newvalue;}
	if (parmname == "LOADFVPC") {static_parm_loadfvpc.Set(util::StringToInt(newvalue)); return newvalue;}
	if (parmname == "LOADMEMP") {static_parm_loadmemp.Set(util::StringToInt(newvalue)); return newvalue;}
	if (parmname == "LOADMMFH") {
		//Decided to allow any value here rather than forcing a power of two.  Merging
		//with an unbalanced tree on 100 files is still more efficient than rounding 
		//down to 64 here and having to do one premerge to get down to 37.
		static_parm_loadmmfh.Set(util::StringToInt(newvalue));
		return newvalue;
	}
	if (parmname == "LOADTHRD") {
		int inew = util::StringToInt(newvalue);
		int ncpus = win::GetProcessorCount();
		if (inew > ncpus) {
			static_parm_loadthrd.Set(ncpus); 
			return util::IntToString(ncpus);
		}
		else {
			static_parm_loadthrd.Set(inew); 
			return newvalue;
		}
	}


//Member parms
	if (parmname == "ENQRETRY") {parm_enqretry = util::StringToInt(newvalue); return newvalue;}
	if (parmname == "FMODLDPT") {static_parm_fmodldpt.Set(util::StringToInt(newvalue)); return newvalue;}
	if (parmname == "MDKRD") {parm_mdkrd = util::StringToInt(newvalue); return newvalue;}
	if (parmname == "MDKWR") {parm_mdkwr = util::StringToInt(newvalue); return newvalue;}
	if (parmname == "MBSCAN") {parm_mbscan = util::StringToInt(newvalue); return newvalue;}

	return ResetWrongObject(parmname);
}

//******************************************************************************************
std::string DatabaseServices::ViewParm(const std::string& parmname, bool format) const
{
//Read-only static parms
	if (parmname == "MAXBUF") return util::IntToString(static_parm_maxbuf);
	if (parmname == "RCVOPT") {
		//V3.0
		if (format)
			return util::UlongToHexString(static_parm_rcvopt, 2, true);
		else
			return util::UlongToString(static_parm_rcvopt);
	}
	if (parmname == "BTREEPAD") return util::IntToString(static_parm_btreepad);
	if (parmname == "PAGESZ") return util::IntToString(DBPAGE_SIZE);
	
	if (parmname == "UPDTID") {
		int id = -1;
		if (update_unit) id = update_unit->ID();
		if (id == -1) id = 0; //we store -1 but standard view value when no txn is zero
		return util::IntToString(id);
	}

//Resettable static parms - threadsafe variables used
	if (parmname == "BUFAGE") return util::IntToString(static_parm_bufage.Value());
	if (parmname == "CPTO") return util::IntToString(static_parm_cpto.Value());
	if (parmname == "CPTIME") return util::IntToString(static_parm_cptime.Value());
	if (parmname == "DUFILES") return util::IntToString(DatabaseFile::NumDUFiles());
	if (parmname == "FMODLDPT") return util::IntToString(static_parm_fmodldpt.Value());
	if (parmname == "MAXRECNO") return util::IntToString(static_parm_maxrecno.Value());
	if (parmname == "LOADCTL") return util::IntToString(static_parm_loadctl.Value());
	if (parmname == "LOADDIAG") return util::IntToString(static_parm_loaddiag.Value());
	if (parmname == "LOADFVPC") return util::IntToString(static_parm_loadfvpc.Value());
	if (parmname == "LOADMEMP") return util::IntToString(static_parm_loadmemp.Value());
	if (parmname == "LOADMMFH") return util::IntToString(static_parm_loadmmfh.Value());
	if (parmname == "LOADTHRD") return util::IntToString(static_parm_loadthrd.Value());

//Member parms
	if	(parmname == "CODESA2E")	{
		std::string s((char*)util::A_TO_E_CODE_PAGE, 256);
		if (format) return util::AsciiStringToHexString(s, true);
		else return s;
	}
	if	(parmname == "CODESE2A")	{
		std::string s((char*)util::E_TO_A_CODE_PAGE, 256);
		if (format) return util::AsciiStringToHexString(s, true);
		else return s;
	}
	if (parmname == "ENQRETRY") return util::IntToString(parm_enqretry);
	if (parmname == "MDKRD") return util::IntToString(parm_mdkrd);
	if (parmname == "MDKWR") return util::IntToString(parm_mdkwr);
	if (parmname == "MBSCAN") return util::IntToString(parm_mbscan);

//These are pseudo parms, put here to support $VIEW
	if (util::OneOf(parmname, "DTSLCHKP/DTSLRCVY/DTSLDKWR/DTSLUPDT")) {
		int lasttime;

		if (parmname == "DTSLCHKP")
			lasttime = last_checkpoint_time.Value();
		else if (parmname == "DTSLRCVY")
			lasttime = last_recovery_time;
		else if (parmname == "DTSLDKWR")
			lasttime = last_dkwr_time;
		else
			lasttime = last_update_time;

		if (lasttime == 0)
			return "0000/00/00 00:00:00";

		tm t = win::GetDateAndTime_tm(lasttime);
		char buff[32];
		strftime(buff, 31, "%Y/%m/%d %H:%M:%S", &t);
		return buff;
	}
	
//Anything else passed in
	return ViewWrongObject(parmname);
}















//******************************************************************************************
//Stat viewing
//******************************************************************************************
_int64 DatabaseServices::ViewStat(const std::string& statname, StatLevel lev) const
{
	if (statname == "BACKOUTS")	return stat_backouts.Value(lev);
	if (statname == "COMMITS")	return stat_commits.Value(lev);
	if (statname == "UPDTTIME")	return (stat_updttime.Value(lev) / 10000);

	if (statname == "BADD")		return stat_badd.Value(lev);
	if (statname == "BCHG")		return stat_bchg.Value(lev);
	if (statname == "BDEL")		return stat_bdel.Value(lev);
	if (statname == "BXDEL")	return stat_bxdel.Value(lev);
	if (statname == "BXFIND")	return stat_bxfind.Value(lev);
	if (statname == "BXFREE")	return stat_bxfree.Value(lev);
	if (statname == "BXINSE")	return stat_bxinse.Value(lev);
	if (statname == "BXNEXT")	return stat_bxnext.Value(lev);
	if (statname == "BXRFND")	return stat_bxrfnd.Value(lev);
	if (statname == "BXSPLI")	return stat_bxspli.Value(lev);
	if (statname == "DIRRCD")	return stat_dirrcd.Value(lev);
	if (statname == "FINDS")	return stat_finds.Value(lev);
	if (statname == "RECADD")	return stat_recadd.Value(lev);
	if (statname == "RECDEL")	return stat_recdel.Value(lev);
	if (statname == "RECDS")	return stat_recds.Value(lev);
	if (statname == "STRECDS")	return stat_strecds.Value(lev);
	if (statname == "SORTS")	return stat_sorts.Value(lev);

	if (statname == "DKPR")		return stat_dkpr.Value(lev);
	if (statname == "DKRD")		return stat_dkrd.Value(lev);
	if (statname == "DKWR")		return stat_dkwr.Value(lev);
	if (statname == "FBWT")		return stat_fbwt.Value(lev);
	if (statname == "DKSFBS")	return stat_dksfbs.Value(lev);
	if (statname == "DKSFNU")	return stat_dksfnu.Value(lev);
	if (statname == "DKSKIP")	return stat_dkskip.Value(lev);
	if (statname == "DKSKIPT")	return stat_dkskipt.Value(lev);
	if (statname == "DKSWAIT")	return stat_dkswait.Value(lev);
	if (statname == "DKUPTIME")	return (stat_dkuptime.Value(lev) / 10000);

	if (statname == "BLKCFRE")	return stat_blkcfre.Value(lev);
	if (statname == "WTCFR")	return stat_wtcfr.Value(lev);
	if (statname == "WTRLK")	return stat_wtrlk.Value(lev);

	if (statname == "ILMRADD")	return stat_ilmradd.Value(lev);
	if (statname == "ILMRDEL")	return stat_ilmrdel.Value(lev);
	if (statname == "ILMRMOVE")	return stat_ilmrmove.Value(lev);
	if (statname == "ILRADD")	return stat_ilradd.Value(lev);
	if (statname == "ILRDEL")	return stat_ilrdel.Value(lev);
	if (statname == "ILSADD")	return stat_ilsadd.Value(lev);
	if (statname == "ILSDEL")	return stat_ilsdel.Value(lev);
	if (statname == "ILSMOVE")	return stat_ilsmove.Value(lev);

	if (statname == "MERGES")	return stat_merges.Value(lev);
	if (statname == "STVALS")	return stat_stvals.Value(lev);
	if (statname == "MRGVALS")	return stat_mrgvals.Value(lev);

	StatWrongObject(statname);
	return 0;
}

//******************************************************************************************
void DatabaseServices::ClearSLStats()
{
	stat_backouts.ClearSL();
	stat_commits.ClearSL();
	stat_updttime.ClearSL();

	stat_badd.ClearSL();
	stat_bchg.ClearSL();
	stat_bdel.ClearSL();
	stat_bxdel.ClearSL();
	stat_bxfind.ClearSL();
	stat_bxfree.ClearSL();
	stat_bxinse.ClearSL();
	stat_bxnext.ClearSL();
	stat_bxrfnd.ClearSL();
	stat_bxspli.ClearSL();
	stat_dirrcd.ClearSL();
	stat_finds.ClearSL();
	stat_recadd.ClearSL();
	stat_recdel.ClearSL();
	stat_recds.ClearSL();
	stat_strecds.ClearSL();
	stat_sorts.ClearSL();

	stat_dkpr.ClearSL();
	stat_dkrd.ClearSL();
	stat_dkwr.ClearSL();
	stat_fbwt.ClearSL();
	stat_dksfbs.ClearSL();
	stat_dksfnu.ClearSL();
	stat_dkskip.ClearSL();
	stat_dkskipt.ClearSL();
	stat_dkswait.ClearSL();
	stat_dkuptime.ClearSL();

	stat_blkcfre.ClearSL();
	stat_wtcfr.ClearSL();
	stat_wtrlk.ClearSL();

	stat_ilmradd.ClearSL();
	stat_ilmrdel.ClearSL();
	stat_ilmrmove.ClearSL();
	stat_ilradd.ClearSL();
	stat_ilrdel.ClearSL();
	stat_ilsadd.ClearSL();
	stat_ilsdel.ClearSL();
	stat_ilsmove.ClearSL();

	stat_merges.ClearSL();
	stat_stvals.ClearSL();
	stat_mrgvals.ClearSL();
}

//******************************************************************************************
void DatabaseServices::BaselineStatsForMValueCheck()
{
	stat_mvalue_check_baseline_dkrd = stat_dkrd.Value(STATLEVEL_USER_LOGOUT);
	stat_mvalue_check_baseline_dkwr = stat_dkwr.Value(STATLEVEL_USER_LOGOUT);

	//The check includes a check at core level so baseline that too
	core->BaselineStatsForMValueCheck();
}

//******************************************************************************************
const char* DatabaseServices::StatMValueExceeded()
{
//	static char* lit_mcnct = "MDKRD"; //V2.24.  gcc 4.2.  Deprecated.  Quite right too.
//	static char* lit_mcpu = "MDKWR";
	static const char* lit_mdkrd = "MDKRD";
	static const char* lit_mdkwr = "MDKWR";

	if (parm_mdkrd != -1) {
		_int64 delta = stat_dkrd.Value(STATLEVEL_USER_LOGOUT) - stat_mvalue_check_baseline_dkrd;
		if (delta >= parm_mdkrd)
			return lit_mdkrd;
	}

	if (parm_mdkwr != -1) {
		_int64 delta = stat_dkwr.Value(STATLEVEL_USER_LOGOUT) - stat_mvalue_check_baseline_dkwr;
		if (delta >= parm_mdkwr)
			return lit_mdkwr;
	}

	return core->StatMValueExceeded();
}

} //close namespace


