//****************************************************************************************
// The top level interface class for database API callers, including the main DPT host.
//****************************************************************************************

#if !defined(BB_API_DBSERV)
#define BB_API_DBSERV

#include <string>
#include <vector>
#include "apiconst.h"

#include "core.h"
#include "grpserv.h"
#include "seqserv.h"
#include "dbctxt.h"

namespace dpt {

class DatabaseServices;
class APIContextSpecification;
namespace util {
	class LineOutput;
}

class APIDatabaseServices {
public:
	DatabaseServices* target;

	//For special output destination requirements
	APIDatabaseServices(
		util::LineOutput* output_destination,
		const std::string& userid = "George",
		const std::string& parm_ini_filename = "parms.ini",
		const std::string& msgctl_ini_filename = "msgctl.ini",
		const std::string& audit_filename = "audit.txt",
		util::LineOutput* secondary_audit = NULL);

	//All-file or console output
	APIDatabaseServices(
		const std::string& output_filename = "sysprint.txt", //or "CONSOLE"
		const std::string& userid = "George",
		const std::string& parm_ini_filename = "parms.ini",
		const std::string& msgctl_ini_filename = "msgctl.ini",
		const std::string& audit_filename = "audit.txt");

	APIDatabaseServices(DatabaseServices*);
	APIDatabaseServices(const APIDatabaseServices&);
	~APIDatabaseServices();

	//Allows several invocations from the same executable location
	static void CreateAndChangeToUniqueWorkingDirectory(bool delete_at_closedown);	

	//-----------------------------------------------------------------------------------

	//Service objects
	APICoreServices Core();
	APIGroupServices GrpServs();
	APISequentialFileServices SeqServs();

	//File management
	void Allocate(const std::string& dd, const std::string& dsn, 
		FileDisp = FILEDISP_OLD, const std::string& alias = std::string());
	void Free(const std::string& dd);

	void Create(const std::string& dd, 
		int bsize = -1, int brecppg = -1, int breserve = -1, int breuse = -1, 
		int dsize = -1, int dreserve = -1, int dpgsres = -1, int fileorg = -1);

	//Opening and closing generic file or group contexts.
	//V2.14  Jan 09.  3 simpler funcs now - all the optional parameters were becoming silly.
	APIDatabaseFileContext OpenContext(const APIContextSpecification&);
	//Deferred updates, two flavours:
	APIDatabaseFileContext OpenContext_DUMulti(const APIContextSpecification&, 
		const std::string& duname_n, const std::string& duname_a, int dulen = -1, DUFormat = DU_FORMAT_DEFAULT);
	APIDatabaseFileContext OpenContext_DUSingle(const APIContextSpecification&);
	
	APIDatabaseFileContext FindOpenContext(const APIContextSpecification&) const;
	std::vector<APIDatabaseFileContext> ListOpenContexts
		(bool include_singles = true, bool include_groups = true) const;

	bool CloseContext(const APIDatabaseFileContext&);
	void CloseAllContexts(bool force = false);

	//Transaction control
	void Commit(bool if_backoutable = true, bool if_nonbackoutable = true);
	void Backout(bool discreet = false);
	void AbortTransaction();
	bool UpdateIsInProgress();
	static bool TBOIsOn();
	static bool ForceBatchCommit();

	//Checkpointing
	void Checkpoint(int = DEFAULT_CPTO);
	static bool ChkpIsEnabled();
	bool ChkAbortRequest();
	int GetNumTimedOutChkps();
	static time_t GetLastChkpTime();
	static time_t GetCurrentChkpTime();
	static time_t GetNextChkpTime();

	//Rollback
	int Rollback1(); //0=OK, 1=Bypassed, 2=Failed.
	void Rollback2();
	int RecoveryFailedCode();
	const std::string& RecoveryFailedReason();

	//Other system management
	int Tidy(int = DEFAULT_BUFAGE);

	//This can be used to encourage destruction in a garbage-collected environment
	void Destroy();
};

//*********************************
//Use when monitoring other threads
class APIUserLockInSentry {
public:
	APIDatabaseServices* other;

	APIUserLockInSentry(int usernum);
	~APIUserLockInSentry();
};

} //close namespace

#endif
