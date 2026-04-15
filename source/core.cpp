
#include "stdafx.h"

#include "core.h"

#include "windows.h"		//for GetCurrentThreadId()

//Utils
#include "pattern.h"
#include "dataconv.h"
#include "lineio.h"
#include "flgs.h" //#include "FLGS.h" : V2.24 case is less interchangeable on *NIX - Roger M.
#include "winutil.h"
//API tiers
#include "custom.h"
#include "msgref.h"
#include "msgini.h"
#include "msgroute.h"
#include "parmref.h"
#include "parmini.h"
#include "parmvr.h"
#include "statref.h"
#include "statview.h"
#include "access.h"
#include "audit.h"
#include "bbfloat.h"
//Diagnostics
#include "except.h"
#include "msg_core.h"
#include "assert.h"

namespace dpt {

//define static members
CoreServices*			CoreServices::user0core = NULL;
ParmRefTable*			CoreServices::parmref = NULL;
ParmIniSettings*		CoreServices::parmini = NULL;
MsgCtlRefTable*			CoreServices::msgref = NULL;
MsgCtlIniSettings*		CoreServices::msgini = NULL;
Audit*					CoreServices::audit = NULL;
StatRefTable*			CoreServices::statref = NULL;
AccessController*		CoreServices::access = NULL;

int						CoreServices::instances = 0;
#ifdef _DEBUG_LOCKS
Sharable				CoreServices::instances_lock = Sharable("Core instances");
#else
Sharable				CoreServices::instances_lock = Sharable();
#endif

ThreadSafeLong			CoreServices::static_parm_cstflags;
int						CoreServices::static_parm_audctl = 0;
int						CoreServices::static_parm_audkeep = 0;
int						CoreServices::static_parm_nusers = 0;
std::string				CoreServices::static_parm_sysname = "*uninitialized*";
std::string				CoreServices::static_parm_versdpt = "*uninitialized*";

ThreadSafeFlag			CoreServices::quiesceing = ThreadSafeFlag();
std::vector<CoreServices*> CoreServices::user_table = std::vector<CoreServices*>();
time_t					CoreServices::system_startup_time = 0;

ThreadSafeI64			CoreServices::sysstat_audit;
ThreadSafeI64			CoreServices::sysstat_seqi;
ThreadSafeI64			CoreServices::sysstat_seqo;

//**********************************************************************************************
//Constructor
//There is a certain amount of bootstrapping to be done with the various system facilities, 
//and this constructor ensures everything happens in the correct order.
//NB. There is only so far we can go in reporting errors before there is any output context,
//so any problems before the audit file is open are thrown out as dpt::Exceptions.
//**********************************************************************************************
CoreServices::CoreServices( 
	util::LineOutput* op,
	const std::string& userid,
	const std::string& parm_ini_filename, 
	const std::string& msgctl_ini_filename,
	const std::string& audit_filename,
	util::LineOutput* secondary_audit,
	const std::string& password
	)
:
	scheduled_for_bump(false),
	parmvr(NULL),
	statview(NULL),
	output(op),
	asynch_priority_refresh_value(0),
	stat_seqi(&sysstat_seqi),
	stat_seqo(&sysstat_seqo),
	stat_audit(&sysstat_audit),
	yesnofunc(DefaultInteractiveYesNo),
	yesnoobj(NULL),
	tickfunc(DefaultTickHandler),
	progress_function(NULL),
	access_info_cache_time(0)
{
	tickobj = this;

	//*************************************************************************************
	//When the first object is created, there is some system set-up work to be done
	//*************************************************************************************
	LockingSentry s(&instances_lock);

	if (instances == 0) {
		parm_userno = 0; //assume this, for startup audit messages

		//this try simplifies cleanup of created objects (of which there are several)
		try {	
			//Initialize static reference data

			std::string audvers("Unknown");

			//*****************************************
			//Parameters 
			//*****************************************
			try {
				parmref = new ParmRefTable();

				static_parm_audctl = util::StringToInt(
					parmref->GetRefInfo("AUDCTL").default_value);
				static_parm_audkeep = util::StringToInt(
					parmref->GetRefInfo("AUDKEEP").default_value);

				audvers = parmref->GetRefInfo("VERSDPT").default_value;
			}
			//can only really be memory
			catch (...) {
				//Open the audit trail with very basic settings
				audit = new Audit(audit_filename, secondary_audit, audvers);
				std::string s("Error initializing system static data (parms)");
				AuditLine(s, "SYS");
				throw Exception(MISC_CAUGHT_STL, s);
			}

			//Now all parameterized objects know where to go for parm validation during RESET
			ViewerResetter::SetRefTable(parmref);

			//We take a similar approach to the above when it comes to reading the ini file
			try {
				parmini = new ParmIniSettings(parm_ini_filename, parmref);
			}
			catch (Exception& e) {
				audit = new Audit(audit_filename, secondary_audit, audvers, 
					static_parm_audctl, static_parm_audkeep);
				std::string s(e.What());
				AuditLine(s, "SYS");
				throw Exception(SYS_BAD_INIFILE, s);
			}

			//Now all parameterized objects know where to go for parameter values when they're 
			//created (i.e. they don't use the defaults but the values specified for this run 
			//in the ini file).  Note: can't have user/thread-specific ini settings (like M204) 
			//doing it like this.
			Parameterized::SetIni(parmini);

			static_parm_audctl = util::StringToInt(parmini->GetParmValue("AUDCTL"));
			static_parm_audkeep = util::StringToInt(parmini->GetParmValue("AUDKEEP"));

			//We can finally create the audit trail with proper parms
			audit = new Audit(audit_filename, secondary_audit, audvers, 
				static_parm_audctl, static_parm_audkeep);
			AuditLine("Parameters initialized", "SYS");

			//Summarise parameter overrides for this run
			if (parmini->NumOverrides() > 0) {
				AuditLine("Overrides were supplied for the following parameters", "SYS");
				if (parmini->SUMOM()) 
					AuditLine("Max or min was used "
								"instead of some supplied values - see (*)", "SYS");

				std::vector<std::string> vs;
				parmini->SummarizeOverrides(&vs);

				for (size_t x = 0; x < vs.size(); x++)
					AuditLine(vs[x], "SYS");
			}

			//*****************************************
			//Message control
			//*****************************************

			//Now move on to setting up the MSGCTL system so user code can issue messages.
			MsgRouter::SetAudit(audit);

			//Static reference data (defaults etc.)
			try {
				msgref = new MsgCtlRefTable();
			}
			catch (...) {
				std::string s("Error initializing system static data (msgctl)");
				AuditLine(s, "SYS");
				throw Exception(MISC_CAUGHT_STL, s);
			}
			
			MsgRouter::SetRefTable(msgref);

			//Next ini file settings.  (Much like parms).
			try {
				msgini = new MsgCtlIniSettings(msgctl_ini_filename, msgref);
			}
			catch (Exception& e) {
				AuditLine(e.What(), "SYS");
				throw Exception(SYS_BAD_INIFILE, e.What());
			}

			AuditLine("Message control initialized", "SYS");
			MsgRouter::SetIni(msgini);

			//Summarise MSGCTL overrides for this run
			if (msgini->NumOverrides() > 0) {
				AuditLine("Overrides were supplied for the following messages", "SYS");
				std::vector<std::string> vs;
				msgini->SummarizeOverrides(&vs);

				for (size_t x = 0; x < vs.size(); x++)
					AuditLine(vs[x], "SYS");
			}

			//*****************************************
			//Statistics
			//*****************************************

			//Another type of static reference data is that for system stats.  This is less
			//tricksy than parms and messaging, since there is no interdependence.
			try {
				statref = new StatRefTable();
			}
			catch (...) {
				std::string s("Error initializing system static data (stats)");
				AuditLine(s, "SYS");
				throw Exception(MISC_CAUGHT_STL, s);
			}

			//Now all stat-maintaining objects know where to go for validation
			StatViewer::SetRefTable(statref);
			AuditLine("Statistics initialized", "SYS");

			//*****************************************
			//V2.16 Access control.
			//*****************************************
			try {
				//User name isn't set up yet, so give a new file a system ID stamp
				parm_userid = parmini->GetParmValue("SYSNAME");
				access = new AccessController(this);
				parm_userid.resize(0);
			}
			catch (Exception& e) {
				parm_userid.resize(0);
				AuditLine(e.What(), "SYS");
				throw Exception(e.Code(), e.What());
			}
			catch (...) {
				parm_userid.resize(0);
				std::string s("Error initializing system static data (access control)");
				AuditLine(s, "SYS");
				throw Exception(MISC_CAUGHT_STL, s);
			}

			AuditLine("Access control initialized", "SYS");

			//*****************************************
			//General stuff
			//*****************************************

			//Initialize system-wide CoreServices parameters
			ResetParm("CSTFLAGS", parmini->GetParmValue("CSTFLAGS"));

			static_parm_nusers = util::StringToInt(parmini->GetParmValue("NUSERS"));
			static_parm_sysname = parmini->GetParmValue("SYSNAME");
			static_parm_versdpt = parmini->GetParmValue("VERSDPT");
			user_table.resize(static_parm_nusers, NULL);

			//Pseudo-stat baseline
			time(&system_startup_time);

			AuditLine("System CoreServices initialization complete", "SYS");

		//Any problems with any of that, make sure all objects created get cleared
		}
		catch (Exception&) {
			DeleteStaticObjects();
			throw;
		}
		catch (...) {
			DeleteStaticObjects();
			throw Exception(MISC_CAUGHT_UNKNOWN, "* * * Unknown exception * * * \r\n");
		}

		user0core = this;
	}

	//*************************************************************************************
	//This next bit happens for all users, not just user 0
	//*************************************************************************************

	//It is important that only one user is started per thread, as this is the key we use
	//during all resource acquisitions and other low level locking.
	thread_id = GetCurrentThreadId();

	//User gets the next available number, so long as NUSERS is not exceeded
	int uno = -1;
	for (int x = 0; x < static_parm_nusers; x++) {
		if (user_table[x] == NULL) {
			//Use first available slot
			if (uno == -1) { //see comment below
				uno = x;
			}
		}
		else {
			//Unpleasant total table scan but tidy and saves a std::set<tid> just for this
			if (user_table[x]->GetThreadID() == thread_id) {
				throw Exception(USER_DUPETHREAD, "User CoreServices initialization failed "
					"- second API on the same thread is not allowed");
			}
		}
	}
	if (uno == -1) {
		//This can't happen for user 0 as the minimum is 1, so no need to delete statics
		throw Exception(USER_TOO_MANY_NUSERS, "User initialization failed "
			"- the maximum number of users is now logged on");
	}

	parm_userno = uno;
	user_table[parm_userno] = this;

	//Now we have enough info to set up the message router
	try {
		parmvr = new ViewerResetter();
		msgrouter = new MsgRouter(this, output);
	}
	catch (...) {
		user_table[parm_userno] = NULL;
		if (parmvr) delete parmvr;
		if (instances == 0) DeleteStaticObjects();
		throw;
	}

	//Has EOD been issued?  If so no new users are allowed.  NB we leave this till now
	//to check so that a message can be issued via the router.
	if (quiesceing.Value()) {
		std::string c = std::string("User=")
			.append(userid)
			.append(" start-up failed.  System is quiescing, no new users are allowed");
		msgrouter->Issue(USER_HEEDINGQUIESCE, c);
		delete msgrouter;
		delete parmvr;
		user_table[parm_userno] = NULL;
		if (instances == 0) DeleteStaticObjects();
		throw Exception(USER_HEEDINGQUIESCE, c);
	}

	//User name, and password if required
	if (parm_userno == 0) {

		//Give user 0 the system name and account
		parm_userid = static_parm_sysname;
		parm_account = AccessController::fullprivs_account;
	}
	else {
		try {
			parmref->ValidateResetDetails("USERID", userid);
			parm_userid = userid;
		}
		catch (...) {

			//V3.03. Invalid user ID is only a problem if we require logon
			if (RequireLogon()) {
				user_table[parm_userno] = NULL;
				throw Exception(USER_BADID, "User name has an invalid format");
			}

			//This is the original DPT behaviour - proceed with an info message.
			parm_userid = "George";
			std::string msg("Invalid user name - I will call you ");
			msg.append(parm_userid);
			msgrouter->Issue(USER_BADID, msg);
		}

		//V3.03. Validate the userid/password
		if (RequireLogon()) {

			//Kluge to get daemons through with their spawner's account.
			//Does the supplied password start with a special prefix?
			const std::string& prefix = AccessController::system_account_prefix_string;

			if (password.length() >= prefix.length() && password.compare(0, prefix.length(), prefix) == 0)

				//Yes so trim prefix and use that as the account
				parm_account = password.substr(prefix.length());

			else {

				//Otherwise validate properly
				try {
					if (!access->CheckAccountPassword(parm_userid, password))
						throw Exception(LOGIN_FAILED, "Invalid password (check case?)");
				}
				catch (...) {
					user_table[parm_userno] = NULL;
					throw;
				}
				parm_account = parm_userid;
			}
		}
	}

	try {
		//Tell the parms VIEW/RESET interface where to go...
		//User specific CoreServices parms
		RegisterParm("ACCOUNT", parmvr);
		RegisterParm("MCNCT", parmvr);
		RegisterParm("MCPU", parmvr);
		RegisterParm("PRIORITY", parmvr);
		RegisterParm("USERID", parmvr);
		RegisterParm("USERNO", parmvr);
		RegisterParm("UPRIV", parmvr);

		parm_mcnct = GetIniValueInt("MCNCT");
		parm_mcpu = GetIniValueInt("MCPU");

		//System wide CoreServices parms (as above)
		RegisterParm("CSTFLAGS", parmvr);
		RegisterParm("NUSERS", parmvr);
		RegisterParm("NCPUS", parmvr); //V3.0
		RegisterParm("OPSYS", parmvr);
		RegisterParm("SYSNAME", parmvr);
		RegisterParm("VERSDPT", parmvr);

		//The audit trail could not register itself when it was created
		RegisterParm("AUDCTL", parmvr);
		RegisterParm("AUDKEEP", parmvr);

		//Similarly for stats
		statview = new StatViewer(this);
		RegisterHolder(statview);
		RegisterStat("AUDIT", statview);
		RegisterStat("CNCT", statview);
		RegisterStat("CPU", statview);
		RegisterStat("SEQI", statview);
		RegisterStat("SEQO", statview);

		//Pseudo-stat start points
		time(&user_logon_time);
		user_last_cnct = user_logon_time;
		user_last_cpu = 0;
		user_asynch_cpu = 0;

		//Log-on is complete - issue a message
		std::string msg = std::string("User=")
			.append(parm_userid)
			.append(" CoreServices started, printing to file/device: ")
			.append(output->GetName());
		msgrouter->Issue(USER_STARTED_LEVEL1, msg);

		//Debugging V2.21
		std::string desc = output->GetDesc();          
		if (desc.length() > 0) {
			msg.append(" (").append(desc).append(")");
			msgrouter->Issue(MISC_DEBUG_USRAUDIT_INFO, msg);
		}

		instances++;
	}
	catch (Exception& e) {
		msgrouter->Issue(e.Code(), e.What());
		delete msgrouter;
		delete parmvr;
		if (statview) delete statview;
		user_table[parm_userno] = NULL;
		if (instances == 0) DeleteStaticObjects();
		throw;
	}
	catch (...) {
		msgrouter->Issue(MISC_CAUGHT_UNKNOWN, "* * * Unknown exception * * * \r\n");
		delete msgrouter;
		delete parmvr;
		if (statview) delete statview;
		user_table[parm_userno] = NULL;
		if (instances == 0) DeleteStaticObjects();
		throw;
	}
}

//*****************************************************************************************
CoreServices::~CoreServices()
{
	//All access to the static data structures stops whilst we log off
	LockingSentry s(&instances_lock);

	//Issue a log-off message
	msgrouter->Issue(USER_FINISHED_LEVEL1, std::string
		("User=")
		.append(parm_userid)
		.append(" CoreServices terminating"));

	instances--;

	//Note if we are the last user apart from user 0 after a system quiesce
	if (instances == 1 && quiesceing.Value()) {
		msgrouter->Issue(SYS_QUIESCECOMPLETE, 
			"Last non-system user terminated - system quiesce complete");
	}

	//Audit user logout statistics
	std::string statline = statview->UnformattedLine(STATLEVEL_USER_LOGOUT);
	std::string auditline("$$$ USERID='");
	auditline.append(parm_userid).append(1, '\'');
	AuditLine(auditline, "STT");
	AuditLine(statline, "STT");

	if (instances == 0) {
		AuditLine(std::string("System CoreServices closing down, return code is ")
			.append(util::IntToString(msgrouter->GetJobCode())), "SYS");

		//Audit system final statistics
		std::string statline = statview->UnformattedLine(STATLEVEL_SYSTEM_FINAL);
		std::string auditline("$$$ SYSTEM='");
		auditline.append(static_parm_versdpt).append(1, '\'');
		AuditLine(auditline, "STT");
		AuditLine(statline, "STT");

		DeleteStaticObjects();
	}

	user_table[parm_userno] = NULL;
	delete msgrouter; 
	delete parmvr;
	delete statview;
}

//**********************************************************************************************
//Useful because of the complicated initializer - we can call this for probs at any stage
//**********************************************************************************************
void CoreServices::DeleteStaticObjects()
{
	if (audit)		{delete audit; audit = NULL;}
	if (parmref)	{delete parmref; parmref = NULL;}
	if (statref)	{delete statref; statref = NULL;}
	if (parmini)	{delete parmini; parmini = NULL;}
	if (msgref)		{delete msgref; msgref = NULL;}
	if (msgini)		{delete msgini; msgini = NULL;}
	if (access)		{delete access; access = NULL;}
	
	user0core = NULL;
}

//**********************************************************************************************
void CoreServices::SwitchOutputDevice(util::LineOutput* op)
{
	output = op;
	msgrouter->output = op;
}

//**********************************************************************************************
void CoreServices::AuditLine(const std::string& line, const char* linetype)
{
	stat_audit.Inc();
	audit->WriteOutputLine(line, parm_userno, linetype);
}

//V2.29
void CoreServices::ExtractAuditLines
(std::vector<std::string>* result, const std::string& from, const std::string& to, 
 const std::string& patt, bool pattcase, int maxlines)
{
	audit->ExtractLines(result, from, to, patt, pattcase, maxlines);
}

//**********************************************************************************************
//Access control.
//This function differs from the one in the AccessController class, which just returns the 
//info in the password file. Here we take account of what kind of access control is enabled with 
//CSTFLAGS. If no access control at all, we return FFFFFFFF - i.e. all access.
//**********************************************************************************************
unsigned int CoreServices::GetEffectiveUserPrivs(int fid) const
{
	//"No access control" really means that all access control checks will succeed:
	if ( (CstFlags() & ENABLE_ACCESS_CONTROL) == 0)
		return UINT_MAX;

	//System users get full privileges to start with.
	if (parm_account == AccessController::fullprivs_account)
		return UINT_MAX;

	//If LOGCTL commands have been issued since we last cached our access data, recache it.
	//We cache for speed so that access control checks (which might be extremely frequent)
	//do not have to do a user name map lookup every time.
	if (access_info_cache_time < access->last_write_time_t) {
		access_info = access->LocateCachedInfo(parm_account, false);
		access_info_cache_time = access->last_write_time_t;
	}

	if (access_info == NULL)
		//Account doesn't exist in PW file
		return 0;
	
	else 
		//Return privs for file, or basic privs if none for file		
		return access_info->FilePrivs(fid);
}

//***********************************************************************************
//This function effectively lets the user become somebody else. It is the meat of
//what happens in a LOGON command. 
//***********************************************************************************
_int64 CoreServices::SwitchToNewUseridAndAccount(const std::string& new_userid, 
		const std::string& new_account, const std::string& new_account_pw)
{
	//Make room for negative RC when the "real" result is in the range 0..UINT_MAX
	_int64 result;

	try {
		//You must give a valid new account and password
		if (!access->CheckAccountPassword(new_account, new_account_pw))
			result = -1; //incorrect pw

		else {
			//OK here we go. First note the currently-active privs for the RC.
			result = GetEffectiveUserPrivs();

			//Change account. This value is used in all future access checks.
			parm_account = new_account;

			//Invalidate access info so the next access check will recache it.
			access_info_cache_time = 0;

			//Change userid. No real implications of this, it's largely cosmetic.
			//However, we should still ensure a sensible format is given.
			const ParmRefInfo& ref = parmvr->GetRefTable()->GetRefInfo("USERID");
			util::Pattern patt(ref.validation_pattern);
			if (!patt.IsLike(new_userid))
				throw "will return -2";

			parm_userid = new_userid;
		}
	}
	catch (...) {
		result = -2; //bad userid or bad format account
	}

	return result;
}

//***********************************************************************************
//A cut-down version of the above for where the supplied account is pre-validated.
//***********************************************************************************
void CoreServices::DaemonSwitchToSpawnerAccount(const std::string& spawner_account)
{
	parm_account = spawner_account;

	access_info_cache_time = 0;
}

//****************************************
bool CoreServices::RequireLogon() {
	return (CstFlags() & REQUIRE_LOGON_AT_CONNECTION_TIME) != 0;
}

//**********************************************************************************************
//General enquiry functions etc.
//**********************************************************************************************
//WT might change so if we get corrupt data, assume zero.  No need to lock.
int CoreServices::GetWT() {
	int result = wt; if (result < 0 || result >= WT_MAX_VALUE) return 0; return result;}
const char* CoreServices::GetFLGS() {return WT_FLGS[GetWT()];}


//**********************************************************************************************
//This function is always called by another thread - a thread can't bump itself.
//**********************************************************************************************
void CoreServices::ScheduleForBump(int bumpee, bool from_console)
{
	//Ensure their doomed CoreServices object remains valid while we kill it
	SharingSentry s(&instances_lock);

	if (bumpee >= static_parm_nusers)
		throw Exception(USER_BADNUMBER, "Invalid user number (>= NUSERS)");

	//User 0 can only bump itself via the "operator console"
	if (bumpee == 0 && !from_console)
		throw Exception(USER_BADNUMBER, 
			"The system thread can only be bumped via the host console");

	CoreServices* victim = user_table[bumpee];
	if (victim == NULL)
		throw Exception(USER_BADNUMBER, "User not logged on");

	victim->scheduled_for_bump.Set();
	victim->bump_thrown = false;
}

//**********************************************************************************************
bool CoreServices::IsScheduledForBump()
{
	return scheduled_for_bump.Value();
}

//**********************************************************************************************
int CoreServices::NumThreads() 
{
	SharingSentry s(&instances_lock);

	int result = 0;
	for (int x = 0; x < static_parm_nusers; x++)
		if (user_table[x] != NULL) 
			result++;

	return result;
}

int CoreServices::MaxThreads() {return static_parm_nusers;}
int CoreServices::CstFlags() {return static_parm_cstflags.Value();}

//*****************************************************************************************
void CoreServices::Quiesce(CoreServices* c)
{
	if (!quiesceing.Value()) {
		c->AuditLine("System quiesce started - 'EOD'", "SYS");
		quiesceing.Set();
	}
}

//*****************************************************************************************
void CoreServices::Unquiesce(CoreServices* c)
{
	if (quiesceing.Value()) {
		c->AuditLine("System quiesce ended - 'EOD OFF'", "SYS");
		quiesceing.Reset();
	}
}

//*****************************************************************************************
bool CoreServices::IsQuiesceing()
{
	return quiesceing.Value();
}

//******************************************************************************************
//Both these functions return a user number, but there is no guarantee that the user
//won't log off before the result is accessed.  See SessionLockInSentry.
//******************************************************************************************
std::vector<int> CoreServices::GetUsernos(const std::string& who) 
{
	SharingSentry s(&instances_lock); 

	std::vector<int> result;

	//Look at all user objects to see which one has this user ID - used in various
	//commands to allow users to refer to themselves/each other by name - e.g. BUMP.
	//Note that several users may be using the same name, so return all of them.
	for (int x = 0; x < static_parm_nusers; x++) {
		CoreServices* c = user_table[x];
		if (c != NULL) {

			unsigned int ptr = (unsigned int) c;
			std::string id = c->GetUserID();
			std::string acct = c->GetAccount();
			int no = c->GetUserNo();

			//"ALL" is an invalid user id so it's safe to use as a dummy value
			if (who == c->GetUserID() || who == "ALL") {
				result.push_back(x);
			}
		}
	}

	return result;
}

//******************************************************************************************
int CoreServices::GetUsernoOfThread(const ThreadID tid) 
{
	SharingSentry s(&instances_lock);

	for (int x = 0; x < static_parm_nusers; x++) {
		CoreServices* c = user_table[x];
		if (!c)
			continue;

		if (c->GetThreadID() == tid)
			return c->GetUserNo();
	}

	return -1;
}

//******************************************************************************************
//******************************************************************************************
//******************************************************************************************
//Parameter viewing and resetting
//******************************************************************************************
//******************************************************************************************
//******************************************************************************************
std::string CoreServices::ResetParm(const std::string& parmname, const std::string& newvalue)
{
	if (parmname == "CSTFLAGS") {

		//V3.03.
		AccessController::EnsureSystemAdministrator(GetEffectiveUserPrivs(), 
											"Resetting the CSTFLAGS parameter");

		unsigned int uinew = util::StringToUlong(newvalue);

		static_parm_cstflags.Set(uinew);

		//Also V3.03
		bool allow_hex = ((static_parm_cstflags.Value() & DISALLOW_HEX_NUM_LITERALS) == 0);
		RangeCheckedDouble::SetAllowHexOption(allow_hex);

		return newvalue;}
	if (parmname == "MCNCT") {parm_mcnct = util::StringToInt(newvalue); return newvalue;}
	if (parmname == "MCPU") {parm_mcpu = util::StringToInt(newvalue); return newvalue;}

	return ResetWrongObject(parmname);
}

//******************************************************************************************
std::string CoreServices::ViewParm(const std::string& parmname, bool format) const
{
//Static parms
	if (parmname == "CSTFLAGS") {
		if (format) return util::UlongToHexString(static_parm_cstflags.Value(), 4, true);
		else return util::IntToString(static_parm_cstflags.Value());
	}
	if (parmname == "AUDCTL")	{
		if (format) return util::UlongToHexString(static_parm_audctl, 2, true);
		else return util::IntToString(static_parm_audctl);
	}
	if (parmname == "AUDKEEP") return util::IntToString(static_parm_audkeep);
	if (parmname == "VERSDPT") return static_parm_versdpt;
	if (parmname == "NUSERS") return util::IntToString(static_parm_nusers);
	if (parmname == "SYSNAME") return static_parm_sysname;

	//V3.0
	if (parmname == "NCPUS") return util::IntToString(win::GetProcessorCount());

	//dynamically determine the operating system
	if (parmname == "OPSYS") {
		OSVERSIONINFOEX osvi;
		ZeroMemory(&osvi, sizeof(OSVERSIONINFOEX));
		
		//Try for NT initially
		bool nt = true;
		osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
		if (!GetVersionEx((OSVERSIONINFO*) &osvi)) {
			//must not be NT
			nt = false;
			osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
			if (! GetVersionEx ((OSVERSIONINFO *) &osvi)) {
				return "Unknown";
			}
		}

		std::string os;

		switch (osvi.dwPlatformId) {
		case VER_PLATFORM_WIN32_NT:
			if (osvi.dwMajorVersion <= 4) os = "Microsoft Windows NT, ";
			if (osvi.dwMajorVersion == 5) os = "Microsoft Windows 2000, ";

/*				//These things available on NT, but my winbase.h doesn't have them
			if (nt) {
				if (osvi.wProductType == VER_NT_WORKSTATION) os.append("Professional " );
				if (osvi.wProductType == VER_NT_SERVER) os.append("Server ");
			}
*/
			char details[256];
			sprintf (details, "version %d.%d, %s, Build %d.",
				(UINT) osvi.dwMajorVersion,
				(UINT) osvi.dwMinorVersion,
				osvi.szCSDVersion,
				(UINT) (osvi.dwBuildNumber & 0xFFFF)); //V2.24 3 UINT casts to satisfy gcc
			os.append(details);
			break;

		case VER_PLATFORM_WIN32_WINDOWS:
			if ((osvi.dwMajorVersion > 4) || ((osvi.dwMajorVersion == 4) && (osvi.dwMinorVersion > 0)))
				os = "Microsoft Windows 98 ";
			else 
				os = "Microsoft Windows 95 ";
			break;
		case VER_PLATFORM_WIN32s:
			os = "Microsoft Win32s ";
			break;
	   }

	   return os;
	}

//Member parms. 
	if	(parmname == "ACCOUNT")	return parm_account;
	if	(parmname == "MCNCT")	return util::IntToString(parm_mcnct);
	if	(parmname == "MCPU")	return util::IntToString(parm_mcpu);
	if	(parmname == "USERNO")	return util::IntToString(parm_userno);
	if	(parmname == "USERID")	return parm_userid;

	if	(parmname == "PRIORITY") {
		if (cached_thread_priority == 1) return "Low";
		if (cached_thread_priority == 2) return "Standard";
		if (cached_thread_priority == 3) return "High";
		return "Unknown/no value";
	}

	//V3.03
	if (parmname == "UPRIV")
		return util::UlongToHexString(GetEffectiveUserPrivs(), 8, true);
	
//Anything else passed in
	return ViewWrongObject(parmname);
}




//******************************************************************************************
//******************************************************************************************
//******************************************************************************************
//Stat viewing
//******************************************************************************************
//******************************************************************************************
//******************************************************************************************
_int64 CoreServices::ViewStat(const std::string& statname, StatLevel lev) const
{
	if (statname == "AUDIT") 
		return stat_audit.Value(lev);

	//I don't know why M204 disallows view of CNCT and CPU, since they have to be 
	//accessible by the MONITOR command.  I'm going to allow it.
	if (statname == "CNCT") {
		time_t now;
		time(&now);

		time_t basetime;
		if      (lev == STATLEVEL_SYSTEM_FINAL)	basetime = system_startup_time;
		else if (lev == STATLEVEL_USER_LOGOUT)  basetime = user_logon_time;
		else if (lev == STATLEVEL_USER_SL)      basetime = user_last_cnct;

		return now - basetime;
	}
	else if (statname == "CPU") {
		//The results are in 100 ns units so divide to get msec
		if (lev == STATLEVEL_SYSTEM_FINAL)	{
			return win::GetProcessCPU() / 10000;
		}
		else if (lev == STATLEVEL_USER_LOGOUT)	{
			//Kluge to get round a restriction in earlier Windows on inter-thread 
			//querying, as required by MONITOR.  Most of the time when CPU is viewed
			//it's from the same thread when writing stat lines to the audit trail, and
			//the call will immediately follow a call to ClearSLStats() below, which
			//will mean the figure is not out of date.  A MONITORing user however will
			//get figures that are not quite up-to-date.
			LockingSentry ls(&asynch_cpu_lock);
			asynch_cpu_refresh_required = true; 
//			TRACE ("**CPU** LO: Refresh required, Tid=%d \n", thread_id);
			return user_asynch_cpu / 10000;
		}
		else {
			LockingSentry ls(&asynch_cpu_lock);
			asynch_cpu_refresh_required = true; 
//			TRACE ("**CPU** SL: Refresh required, Tid=%d \n", thread_id);
			return (user_asynch_cpu - user_last_cpu) / 10000;
		}
	}
	if (statname == "SEQI")		return stat_seqi.Value(lev);
	if (statname == "SEQO")		return stat_seqo.Value(lev);

	StatWrongObject(statname);
	return 0;
}

//******************************************************************************************
void CoreServices::ClearSLStats()
{
	stat_audit.ClearSL();
	stat_seqi.ClearSL();
	stat_seqo.ClearSL();

	//See comments above re these
	time(&user_last_cnct);
	user_last_cpu = win::GetThreadCPU();

	RefreshCPUStat(true);
}

//******************************************************************************************
void CoreServices::BaselineStatsForMValueCheck()
{
	time(&stat_mvalue_check_baseline_cnct);
	stat_mvalue_check_baseline_cpu = RefreshCPUStat(true) / 10000;
}

//******************************************************************************************
const char* CoreServices::StatMValueExceeded()
{
//	static char* lit_mcnct = "MCNCT"; //V2.24.  gcc 4.2.  Deprecated.  Quite right too.
//	static char* lit_mcpu = "MCPU";
	static const char* lit_mcnct = "MCNCT";
	static const char* lit_mcpu = "MCPU";

	if (parm_mcnct != -1) {
		time_t now;
		time(&now);
		int delta = now - stat_mvalue_check_baseline_cnct;
		if (delta >= parm_mcnct)
			return lit_mcnct;
	}

	if (parm_mcpu != -1) {
		_int64 cpu_100nsec = RefreshCPUStat(true);
		_int64 cpu_msec = cpu_100nsec / 10000;
		_int64 delta = cpu_msec - stat_mvalue_check_baseline_cpu;
		if (delta >= parm_mcpu)
			return lit_mcpu;
	}

	return NULL;
}


//******************************************************************************************
//******************************************************************************************
//******************************************************************************************
//Callback support
//******************************************************************************************
//******************************************************************************************
//******************************************************************************************

//******************************************************************************************
_int64 CoreServices::RefreshCPUStat(bool force)
{
	LockingSentry ls(&asynch_cpu_lock);

	if (asynch_cpu_refresh_required || force) {
		user_asynch_cpu = win::GetThreadCPU();
		asynch_cpu_refresh_required = false;
//		TRACE ("**CPU** Tid=%d  UAC=%d \n", thread_id, user_asynch_cpu);
	}

	return user_asynch_cpu;
}

//******************************************************************************************
void CoreServices::RequestPriorityChange(int newp)
{
	LockingSentry ls(&asynch_priority_lock);
	if (newp >= 1 && newp <= 3)
		asynch_priority_refresh_value = newp;
}

//******************************************************************************************
int CoreServices::GetThreadPriorityAsynch()
{
	//In other words return the current OS priority, not the most recently-requested.
	//If e.g. the user is blocked somewhere, there will be a delay syncrhonizing them.
	return cached_thread_priority;
}

//******************************************************************************************
void CoreServices::RefreshThreadPriority()
{
	LockingSentry ls(&asynch_priority_lock);
	switch (asynch_priority_refresh_value) {
	case 0:
		return;
	case 1:
		SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
		break;
	case 2:
		SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);
		break;
	case 3:
		SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
		break;
	};

	cached_thread_priority = asynch_priority_refresh_value;
	asynch_priority_refresh_value = 0;
}

//******************************************************************************************
//This function is here to deal with asynchronous requests of various kinds passed
//from other threads.  The most important is to check for bumps.  Also handled are
//requests to refresh the local CPU stat, and to change this thread's priority.  See tech 
//docs for more on bumps in general.
//******************************************************************************************
void CoreServices::DefaultTickHandler(const char* msg, void* obj)
{
	CoreServices* core = reinterpret_cast<CoreServices*>(obj);

	core->RefreshCPUStat();
	core->RefreshThreadPriority();

	if (core->IsScheduledForBump()) {

		//V2 Jan 07.  Decided to go for this after many months of prevaricating.  I didn't
		//want to break anything so fundamental as this, but it seems to work OK, and this
		//is conceptually cleaner.  The issue was that in one or two situations a thread 
		//would be bumped for one reason, then during closedown they would be trying to 
		//issue messages etc. and would get this thrown again, so that the final message
		//the user got when the exception made it all the way out was not the original one.
		//Also I have recently become suspicious of whether throwing an exception while the
		//stack is still unwinding from another one is entirely safe on VC++.
		if (!core->bump_thrown) {
			core->bump_thrown = true;
			throw Exception(USER_HEEDINGBUMP, msg);
		}
	}
}

//******************************************************************************************
int CoreServices::CallProgressFunction(const int r, ProgressReportableActivity* a)
{
	if (progress_function == NULL)
		return PROGRESS_PROCEED;

	int rc = progress_function(r, a, this);
	a->ValidateReturnOption(rc);
	return rc;
}

//******************************************************************************************
void CoreServices::RegisterProgressFunction(ProgressFunction f)
{
	if (progress_function)
		CallProgressFunction(PROGRESS_DEREGISTER, NULL);

	progress_function = f;
	if (progress_function)
		CallProgressFunction(PROGRESS_REGISTER, NULL);
}

//******************************************************************************************
void CoreServices::NullBumpableWait(int msec, int wt)
{
	WTSentry s(this, wt);

	double now = win::GetFTimeWithMillisecs() * 1000;
	double then = now + msec;

	while (now < then) {
		Tick("In null bumpable wait loop");

		static const int snooze = 100;
		Sleep(snooze);
		now += snooze;
	}
}

} //close namespace


