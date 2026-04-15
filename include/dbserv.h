//****************************************************************************************
// The top level interface class for database API callers, including the main DPT host.
//****************************************************************************************

#if !defined(BB_DBSERV)
#define BB_DBSERV

#include <map>
#include <string>
#include <vector>

#include "parmized.h"
#include "statized.h"
#include "lockable.h"
#include "apiconst.h"
#include "filehandle.h"

namespace dpt {

//****************************************************************************************
class DBAPILockInSentry;
class StatusCommand;
class SessionData;
class ContextSpecification;
class DefinedContext;
class DatabaseFile;
class BitMappedRecordSet;
class DatabaseFileContext;
class AtomicBackout; 
class AtomicUpdate;
class UpdateUnit; 
class GroupServices;
class GroupDatabaseFileContext;
class SequentialFileServices;
class CoreServices;
struct CAPICommArea;
namespace util {
	class LineOutput;
}

const unsigned short RCVOPT_CHKP			= 0x01;
const unsigned short RCVOPT_TBO				= 0x02;
const unsigned short RCVOPT_MANUAL_RCV		= 0x10;
const unsigned short RCVOPT_AUTORCV_BKP		= 0x20;
const unsigned short RCVOPT_FORCE_COMMIT	= 0x40;

//V3.0
const unsigned int LOADCTL_MEMQUERY_LONGWAY    = 0x00000001;
const unsigned int LOADCTL_LOAD_ORPHAN_ILRECS  = 0x00000002;
const unsigned int LOADCTL_UNLOAD_DEORPHAN     = 0x00000004;
const unsigned int LOADCTL_REORG_ERR_KEEPFILES = 0x00000008;


//****************************************************************************
class DatabaseServices : public Parameterized, public Statisticized {
	CoreServices*	core;
	GroupServices*	groups;
	SequentialFileServices*  seqfiles;

	static std::vector<DatabaseServices*> instances;
	static Sharable             instances_lock;
	static ThreadSafeLong		num_instances;
	bool destroyed; //V2.06 - see cpp file comments.

	static std::string delete_dir_at_closedown; //V2.27

	//Used by session level
	bool locked_in;
	Lockable lockin_lock;
	static DatabaseServices* GetHandleAndLockIn(int);
	friend class DBAPILockInSentry;
	friend class APIUserLockInSentry;
	void LockIn();
	void LetOut();

	//System wide parameters
	static int static_parm_maxbuf;
	static int static_parm_rcvopt;
	static int static_parm_btreepad;
	static ThreadSafeLong static_parm_cpto;
	static ThreadSafeLong static_parm_cptime;
	static ThreadSafeLong static_parm_bufage;
	static ThreadSafeLong static_parm_fmodldpt;
	static ThreadSafeLong static_parm_maxrecno;
	static ThreadSafeLong static_parm_loadctl;
	static ThreadSafeLong static_parm_loaddiag;
	static ThreadSafeLong static_parm_loadfvpc;
	static ThreadSafeLong static_parm_loadmemp;
	static ThreadSafeLong static_parm_loadmmfh;
	static ThreadSafeLong static_parm_loadthrd;
	//User specific parameters
	int parm_enqretry;
	int parm_mdkrd;
	int parm_mdkwr;
	int parm_mbscan;

	//Stats incremented directly
	static ThreadSafeI64 sysstat_backouts; //since each txn may include several files
	static ThreadSafeI64 sysstat_commits;
	static ThreadSafeI64 sysstat_updttime;
	MultiStat stat_backouts;
	MultiStat stat_commits;
	MultiStat stat_updttime;

	//Stats incremented at the same time as file stats
	friend class DatabaseFile;
	static ThreadSafeI64 sysstat_badd;
	static ThreadSafeI64 sysstat_bchg;
	static ThreadSafeI64 sysstat_bdel;
	static ThreadSafeI64 sysstat_bxdel;
	static ThreadSafeI64 sysstat_bxfind;
	static ThreadSafeI64 sysstat_bxfree;
	static ThreadSafeI64 sysstat_bxinse;
	static ThreadSafeI64 sysstat_bxnext;
	static ThreadSafeI64 sysstat_bxrfnd;
	static ThreadSafeI64 sysstat_bxspli;
	static ThreadSafeI64 sysstat_dirrcd;
	static ThreadSafeI64 sysstat_finds;
	static ThreadSafeI64 sysstat_recadd;
	static ThreadSafeI64 sysstat_recdel;
	static ThreadSafeI64 sysstat_recds;
	static ThreadSafeI64 sysstat_strecds;
	static ThreadSafeI64 sysstat_sorts;

	MultiStat stat_badd;
	MultiStat stat_bchg;
	MultiStat stat_bdel;
	MultiStat stat_bxdel;
	MultiStat stat_bxfind;
	MultiStat stat_bxfree;
	MultiStat stat_bxinse;
	MultiStat stat_bxnext;
	MultiStat stat_bxrfnd;
	MultiStat stat_bxspli;
	MultiStat stat_dirrcd;
	MultiStat stat_finds;
	MultiStat stat_recadd;
	MultiStat stat_recdel;
	MultiStat stat_recds;
	MultiStat stat_strecds;
	MultiStat stat_sorts;

	//Buffer stats
	static ThreadSafeI64 sysstat_dkpr;
	static ThreadSafeI64 sysstat_dkrd;
	static ThreadSafeI64 sysstat_dkwr;
	static ThreadSafeI64 sysstat_fbwt;
	static ThreadSafeI64 sysstat_dksfbs;
	static ThreadSafeI64 sysstat_dksfnu;
	static ThreadSafeI64 sysstat_dkskip;
	static ThreadSafeI64 sysstat_dkskipt;
	static ThreadSafeI64 sysstat_dkswait;
	static ThreadSafeI64 sysstat_dkuptime;
	MultiStat stat_dkpr;
	MultiStat stat_dkrd;
	MultiStat stat_dkwr;
	MultiStat stat_fbwt;
	MultiStat stat_dksfbs;
	MultiStat stat_dksfnu;
	MultiStat stat_dkskip;
	MultiStat stat_dkskipt;
	MultiStat stat_dkswait;
	MultiStat stat_dkuptime;
	_int64 stat_mvalue_check_baseline_dkrd;
	_int64 stat_mvalue_check_baseline_dkwr;

	//Record locking stats
	static ThreadSafeI64 sysstat_blkcfre;
	static ThreadSafeI64 sysstat_wtcfr;
	static ThreadSafeI64 sysstat_wtrlk;
	MultiStat stat_blkcfre;
	MultiStat stat_wtcfr;
	MultiStat stat_wtrlk;

	//Custom DPT stats
	static ThreadSafeI64 sysstat_ilmradd;
	static ThreadSafeI64 sysstat_ilmrdel;
	static ThreadSafeI64 sysstat_ilmrmove;
	static ThreadSafeI64 sysstat_ilradd;
	static ThreadSafeI64 sysstat_ilrdel;
	static ThreadSafeI64 sysstat_ilsadd;
	static ThreadSafeI64 sysstat_ilsdel;
	static ThreadSafeI64 sysstat_ilsmove;

	MultiStat stat_ilmradd;
	MultiStat stat_ilmrdel;
	MultiStat stat_ilmrmove;
	MultiStat stat_ilradd;
	MultiStat stat_ilrdel;
	MultiStat stat_ilsadd;
	MultiStat stat_ilsdel;
	MultiStat stat_ilsmove;

	static ThreadSafeI64 sysstat_merges;
	static ThreadSafeI64 sysstat_stvals;
	static ThreadSafeI64 sysstat_mrgvals;

	MultiStat stat_merges;
	MultiStat stat_stvals;
	MultiStat stat_mrgvals;

	//Control of the output destination
	util::LineOutput* output;
	FileHandle op_handle;
	bool op_created_here;

	//Open context information
	std::map<std::string, DatabaseFileContext*> context_directory;
	Lockable context_lock; //For ListOpenContexts

	//When opening groups we call these functions, as well as during direct open/close
	friend class GroupDatabaseFileContext;
	DatabaseFileContext*	CreateContext(DefinedContext* dc);
	bool					OpenContext_Single(DatabaseFileContext*, 
									const std::string&, const std::string&, int, DUFormat,
									const GroupDatabaseFileContext* = NULL);

	void					DeleteContext(DatabaseFileContext*);
	bool					CloseContext_Single(DatabaseFileContext*, 
									const GroupDatabaseFileContext* = NULL, bool = false);

	//Transaction control
	UpdateUnit* update_unit;
	void Checkpoint_S(int, FileHandle* = NULL);
	static int recent_timed_out_checkpoints;
	static ThreadSafeLong current_checkpoint_time;
	static ThreadSafeLong last_checkpoint_time;
	static ThreadSafeLong next_checkpoint_time;
	bool take_final_checkpoint;
	static ThreadSafeFlag chkabort_flag;
	bool ChkAbortAcknowledge() {Commit(); return chkabort_flag.Reset();}
	static void UpdateCheckpointTimes();

	//Pseudo parms for $VIEW
	static time_t last_recovery_time;
	static time_t last_dkwr_time;
	static time_t last_update_time;

	//Base constructor code
	void DatabaseServices_S(util::LineOutput*, const std::string&, const std::string&, 
							const std::string&, const std::string&, util::LineOutput*,
							const std::string&);

	friend class StatusCommand;
	FileHandle FindDatabaseFile(const std::string&, bool);

	friend class SingleDatabaseFileContext;
	static void NotifyAllUsersContextsOfDirtyDelete(BitMappedRecordSet*);

	//Mar 07.
	friend class APIDatabaseServices;
	int refcount;

	//Feb 2010
#ifdef DPT_BUILDING_CAPI_DLL
	CAPICommArea* capi_commarea;
#endif

//****************************************
public:
	//Used by session level, or API code with special requirements.
	DatabaseServices(
		util::LineOutput* output_destination,
		const std::string& userid = "George",
		const std::string& parm_ini_filename = "parms.ini",
		const std::string& msgctl_ini_filename = "msgctl.ini",
		const std::string& audit_filename = "audit.txt",
		util::LineOutput* secondary_audit = NULL,
		const std::string& password = std::string());

	//Used by API code that is happy with all-file output or the console.
	DatabaseServices(
		const std::string& output_filename = "sysprint.txt", //or "CONSOLE"
		const std::string& userid = "George",
		const std::string& parm_ini_filename = "parms.ini",
		const std::string& msgctl_ini_filename = "msgctl.ini",
		const std::string& audit_filename = "audit.txt",
		const std::string& password = std::string());

	~DatabaseServices() {Destroy();}
	void Destroy();

	//Allows several invocations from the same executable location
	static void CreateAndChangeToUniqueWorkingDirectory(bool delete_at_closedown);	

	void SwitchOutputDevice(util::LineOutput*);
	bool IsLockedIn();

	CoreServices* Core() {return core;}
	GroupServices* Groups() {return groups;}
	SequentialFileServices* SeqFiles() {return seqfiles;}

	//Parms and stats
	std::string ResetParm(const std::string&, const std::string&);
	std::string ViewParm(const std::string&, bool) const;
	_int64 ViewStat(const std::string&, StatLevel) const;

	//Most of these should be private eventually
	void ClearSLStats();
	void AddToStatUPDTTIME(_int64 i) {stat_updttime.Add(i);}
	void IncStatBLKCFRE() {stat_blkcfre.Inc();}
	void IncStatWTCFR() {stat_wtcfr.Inc();}
	void IncStatWTRLK() {stat_wtrlk.Inc();}
	void IncStatFINDS() {stat_finds.Inc();}
	void IncStatSORTS() {stat_sorts.Inc();}
	void IncStatMERGES() {stat_merges.Inc();}
	void AddToStatSTVALS(_int64 i) {stat_stvals.Add(i);}
	void AddToStatMRGVALS(_int64 i) {stat_mrgvals.Add(i);}
	void BaselineStatsForMValueCheck();
	const char* StatMValueExceeded();

	//File allocate and free.  Disp can be NEW, OLD or COND.
	void Allocate(const std::string&, const std::string&, 
		FileDisp = FILEDISP_OLD, const std::string& = std::string());
	void Free(const std::string& dd);

	//The defaults mean take values from parm defaults or parms.ini.  See doc for notes
	//on the reason for the restricted maximum dsize and bsize (i.e. not unsigned ints).
	void Create(const std::string&, 
		int	bsize		= -1, 
		int	brecppg		= -1, 
		int	breserve	= -1, 
		int	breuse		= -1, 
		int	dsize		= -1, 
		int	dreserve	= -1,
		int	dpgsres		= -1,
		int	fileorg		= -1);

	//File/group open and close
	//V2.14  Jan 09.  3 simpler funcs now - all the optional parameters were becoming silly.
	DatabaseFileContext* OpenContext(const ContextSpecification&);
	//Deferred updates, two flavours:
	DatabaseFileContext* OpenContext_DUMulti(const ContextSpecification&, 
		const std::string&, const std::string&, int = -1, DUFormat = DU_FORMAT_DEFAULT);
	DatabaseFileContext* OpenContext_DUSingle(const ContextSpecification&);
	
	//These two mainly for system use now - all optional parameters exposed here.
	DatabaseFileContext* OpenContext_Allparms(const ContextSpecification&, bool, bool*, 
									const std::string&, const std::string&, int, int);
	DatabaseFileContext* OpenContext_B(const ContextSpecification&, bool);

	DatabaseFileContext* FindOpenContext(const ContextSpecification&) const;
	std::vector<DatabaseFileContext*> ListOpenContexts
		(bool include_singles = true, bool include_groups = true) const;
	bool CloseContext(DatabaseFileContext*);
	void CloseAllContexts(bool force = false);

	//Transaction control
	UpdateUnit* GetUU() {return update_unit;}
	bool FileIsBeingUpdated(DatabaseFile*);
	void Commit(bool if_backoutable = true, bool if_nonbackoutable = true);
	void Backout(bool discreet = false);
	void AbortTransaction();
	bool UpdateIsInProgress();
	static bool TBOIsOn() {return (static_parm_rcvopt & RCVOPT_TBO) ? true : false;}
	static bool ForceBatchCommit() {return (static_parm_rcvopt & RCVOPT_FORCE_COMMIT) ? true : false;}

	//System management - make most of these private eventually I think
	int Rollback1();
	void Rollback2();
	//void RollForward();
	void Checkpoint(int cpto = -2) {Checkpoint_S(cpto, NULL);}
	static bool ChkpIsEnabled() {return (static_parm_rcvopt & RCVOPT_CHKP) ? true : false;}
	static bool AutoRecoveryIsOn() {return (static_parm_rcvopt & RCVOPT_MANUAL_RCV) ? false : true;}
	static bool AutoRecoverySkipBackups() {return (static_parm_rcvopt & RCVOPT_AUTORCV_BKP) ? false : true;}
	int Tidy(int = -2);
	bool ChkAbortRequest() {Commit(); return chkabort_flag.Set();}
	int GetNumTimedOutChkps() {return recent_timed_out_checkpoints;}
	static time_t GetLastChkpTime() {return last_checkpoint_time.Value();}
	static time_t GetCurrentChkpTime() {return current_checkpoint_time.Value();}
	static time_t GetNextChkpTime() {return next_checkpoint_time.Value();}
	int GetParmCPTIME() {return static_parm_cptime.Value();}
	int GetParmBUFAGE() {return static_parm_bufage.Value();}
	int GetParmFMODLDPT() {return static_parm_fmodldpt.Value();}
	int GetParmENQRETRY() {return parm_enqretry;}
	int GetParmMAXRECNO() {return static_parm_maxrecno.Value();}
	int GetParmMBSCAN() {return parm_mbscan;}
	int GetParmMAXBUF() {return static_parm_maxbuf;}

#ifdef _BBHOST
	void SpawnCheckpointPST(SessionData* sess);
	void SpawnBuffTidyPST(SessionData* sess);
#endif

	static int GetParmLOADCTL() {return static_parm_loadctl.Value();}
	static int GetParmLOADDIAG() {return static_parm_loaddiag.Value();}
	static int GetParmLOADFVPC() {return static_parm_loadfvpc.Value();}
	static int GetParmLOADMEMP() {return static_parm_loadmemp.Value();}
	static int GetParmLOADMMFH() {return static_parm_loadmmfh.Value();}
	static int GetParmLOADTHRD() {return static_parm_loadthrd.Value();}

#ifdef DPT_BUILDING_CAPI_DLL
	CAPICommArea* CAPICommArea() {return capi_commarea;}
#endif
};

//******************************************************************************************
//See doc for more on this - create one to lock in another thread.
class DBAPILockInSentry {
	DatabaseServices* ptr;
public:
	DBAPILockInSentry(int usernum) {ptr = DatabaseServices::GetHandleAndLockIn(usernum);}
	~DBAPILockInSentry() {if (ptr) ptr->LetOut();}
	DatabaseServices* GetPtr() {return ptr;}
};


} //close namespace

#endif
