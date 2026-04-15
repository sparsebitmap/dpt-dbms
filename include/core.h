//****************************************************************************************
// The lowest tier of host functionality.  The main levels are:
// - Core    : Parameters, statistics, messaging, the audit trail, user initiation.
// - Database: All database work, structured locks and waits.  MVS style sequential IO.
// - Session : User IO, UL compiler/debugger, commands, APSY, web server.
//****************************************************************************************

#if !defined(BB_CORE)
#define BB_CORE

#include <string>
#include <vector>

#include "access.h"
#include "parmized.h"
#include "statized.h"
#include "lockable.h"
#include "garbage.h"
#include "filehandle.h"
#include "progress.h"

namespace dpt {

//****************************************
class ParmRefTable;
class ParmIniSettings;
class ViewerResetter;
class MsgCtlRefTable;
class MsgCtlIniSettings;
class Audit;
class StatRefTable;
class StatViewer;
class AccessController;
class MsgRouter;
namespace util {
	class LineOutput;
}

//****************************************************************************************
class CoreServices : public Parameterized, public Statisticized, public Destroyer {

	//System-wide information
	static ParmRefTable*				parmref;
	static ParmIniSettings*				parmini;
	static MsgCtlRefTable*				msgref;
	static MsgCtlIniSettings*			msgini;
	static Audit*						audit;
	static StatRefTable*				statref;
	static AccessController*			access;
	static CoreServices*				user0core;

	static void InitializeStaticObjects(const std::string&, const std::string&, const std::string&);
	static void DeleteStaticObjects();

	static int instances;
	static Sharable	instances_lock;
	static std::vector<CoreServices*> user_table;
	static ThreadSafeFlag quiesceing;
	ThreadSafeFlag scheduled_for_bump;
	bool bump_thrown; //V2 Jan 07.

	//Owned objects (per user)
	MsgRouter*				msgrouter;	//Numbered messaging
	ViewerResetter*			parmvr;		//Interface to VIEW/RESET
	StatViewer*				statview;	//Interface to $VIEW/MONITOR
	util::LineOutput*		output;		//"Terminal" output destination

	//System wide parameters
	static ThreadSafeLong   static_parm_cstflags;
	static int              static_parm_audctl;
	static int              static_parm_audkeep;
	static int              static_parm_nusers;
	static std::string      static_parm_sysname;
	static std::string      static_parm_versdpt;

	//User specific parameters
	std::string			parm_account;
	std::string			parm_userid;
	int					parm_userno;
	int					parm_mcnct;
	int					parm_mcpu;
	void InitializeUserParameters(const std::string& userid);

	//Pseudo parameter
	int				asynch_priority_refresh_value;
	int				cached_thread_priority;
	Lockable		asynch_priority_lock;

	//Pseudo stats
	time_t			user_logon_time;	//For CNCT user
	time_t			user_last_cnct;		//SL
	static time_t	system_startup_time;//SYS

	//Access control - V3.03
	mutable AccessController::CachedAccountInfo* access_info;
	mutable time_t access_info_cache_time;

	//For the CPU kluge
	_int64			user_last_cpu;
	_int64			user_asynch_cpu;
	mutable bool	asynch_cpu_refresh_required;
	Lockable		asynch_cpu_lock;

	static ThreadSafeI64 sysstat_audit;
	static ThreadSafeI64 sysstat_seqi;
	static ThreadSafeI64 sysstat_seqo;
	MultiStat stat_seqi;
	MultiStat stat_seqo;
	MultiStat stat_audit;

	time_t stat_mvalue_check_baseline_cnct;
	_int64 stat_mvalue_check_baseline_cpu;

	//Useful for other threads to enquire on, e.g. during MONITOR
	unsigned int thread_id;
	unsigned int wt;

	//Callback support
	bool (*yesnofunc) (const std::string&, bool, void*);
	void* yesnoobj;
	static bool DefaultInteractiveYesNo(const std::string&, bool b, void*) {return b;}
	void (*tickfunc) (const char*, void*);
	void* tickobj;

	ProgressFunction progress_function;

	//V3.03.
	friend class IODev;
	void DaemonSwitchToSpawnerAccount(const std::string& spawner_account);

public:
	CoreServices(
		util::LineOutput* output_destination,
		const std::string& userid = "George",
		const std::string& parm_ini_filename = "parms.ini", 
		const std::string& msgctl_ini_filename = "msgctl.ini",
		const std::string& audit_filename = "audit.txt",
		util::LineOutput* secondary_audit = NULL,
		const std::string& password = std::string());
	~CoreServices();

	void SwitchOutputDevice(util::LineOutput*);

	static CoreServices* User0Core() {return user0core;};
	MsgRouter* GetRouter() {return msgrouter;};
	ViewerResetter* GetViewerResetter() {return parmvr;};
	StatViewer* GetStatViewer() {return statview;};
	void AuditLine(const std::string&, const char*);
	void ExtractAuditLines(std::vector<std::string>*, const std::string&, const std::string&, 
						const std::string&, bool = false, int = 0);

	//Parms and stats
	std::string ResetParm(const std::string&, const std::string&);
	std::string ViewParm(const std::string&, bool) const;
	_int64 ViewStat(const std::string&, StatLevel) const;
	void ClearSLStats();
	_int64 RefreshCPUStat(bool = false);
	void RefreshThreadPriority();
	void RequestPriorityChange(int); //1=below normal, 2=normal, 3=above normal
	int GetThreadPriorityAsynch();
	void BaselineStatsForMValueCheck();
	const char* StatMValueExceeded();
	void IncStatSEQO() {stat_seqo.Inc();}
	void IncStatSEQI() {stat_seqi.Inc();}

	//See further comments in .cpp and the tech docs
	static void ScheduleForBump(int, bool = false);
	bool IsScheduledForBump();
	void UnBump() {scheduled_for_bump.Reset();} //Used during APSY error proc

	//Typically use sentry - see later
	int SetWT(int i) {int j = wt; wt = i; return j;}

	//Functions relating to all users
	static std::string VersDPT() {return static_parm_versdpt;}
	static int NumThreads();
	static int MaxThreads();
	static int CstFlags();
	static std::vector<int> GetUsernos(const std::string& = "ALL");
	static int GetUsernoOfThread(const ThreadID);
	static void Quiesce(CoreServices*);
	static void Unquiesce(CoreServices*);
	static bool IsQuiesceing();
	static Destroyer* GlobalDestroyer() {return &(GlobalDestroyer::global_destroyer);}

	//Functions relating to a specific user
	const std::string& GetAccount() {return parm_account;}
	const std::string& GetUserID() {return parm_userid;}
	util::LineOutput* Output() {return output;}
	int GetUserNo() {return parm_userno;}
	unsigned int GetThreadID() {return thread_id;}
	int GetWT();
	const char* GetFLGS();

	//Access control
	AccessController* GetAccessController() {return access;} //PW file management
	unsigned int GetEffectiveUserPrivs(int fid = -1) const; //actual runtime checks use this
	_int64 SwitchToNewUseridAndAccount(const std::string& new_userid, 
		const std::string& new_account, const std::string& new_account_pw);
	static bool RequireLogon();

	//Callbacks for various purposes
	bool InteractiveYesNo(const std::string& s, bool b) {return yesnofunc(s, b, yesnoobj);}
	void RegisterInteractiveYesNoFunc(bool (*f) (const std::string&, bool, void*)) {yesnofunc = f;}
	void RegisterInteractiveYesNoObj(void* v) {yesnoobj = v;}

	void Tick(const char* c) {tickfunc(c, tickobj);}
	void RegisterTickFunc(void (*f) (const char*, void*)) {tickfunc = f;}
	void RegisterTickObj(void* v) {tickobj = v;}
	static void DefaultTickHandler(const char*, void*);

	int CallProgressFunction(const int, ProgressReportableActivity*);
	void RegisterProgressFunction(ProgressFunction f);

	//Does nothing but is bumpable.  Handy for testing sometimes.
	void NullBumpableWait(int msec, int wt = 90);
};



//****************************************
// For convenience setting WT state
class WTSentry {
	CoreServices* core;
	int prev_wt_value;

public:
	WTSentry(CoreServices* c, int wt) : core(c) {prev_wt_value = core->SetWT(wt);}
	~WTSentry() {core->SetWT(prev_wt_value);}
};

} //close namespace

#endif
