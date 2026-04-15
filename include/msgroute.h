//****************************************************************************************
//M204-style numbered messaging.
//****************************************************************************************

#if !defined(BB_MSGROUTE)
#define BB_MSGROUTE

#include <map>
#include <list>
#include "msgref.h"
#include "parmized.h"
#include "const_route.h"

namespace dpt {

class CoreServices;
class MsgCtlIniSettings;
class ViewerResetter;
class Audit;
namespace util {
	class LineOutput;
}

//****************************************************************************************
class MsgRouter : public Parameterized {
	friend class CoreServices;

	//Contents only updated during start-up, so no need for locks
	static MsgCtlRefTable* reftable;
	static MsgCtlIniSettings* inisettings;
	static Audit* audit;
	CoreServices* core;

	static void SetRefTable(MsgCtlRefTable* r) {reftable = r;}
	static void SetIni(MsgCtlIniSettings* i) {inisettings = i;}
	static void SetAudit(Audit* a) {audit = a;}

	std::map<int, MsgCtlOptions> settings; //User specific MSGCTL overrides
	util::LineOutput* output;

	//Parameterization
	int parm_errsave;
	int parm_msgctl;
	int hi_retcode;
	int hi_retcode_msgnum;
	std::string hi_retcode_msgtext;
	static ThreadSafeLong static_parm_retcode;
	
	//Control variables
	int error_count;						//Counting errors since last reset
	int error_count_total;					//Counting errors since logon
	int messages_printed_total;				//All TERM messages
	int messages_audited_total;				//All AUDIT messages
	bool last_message_printed;
	bool last_message_audited;
	std::string errmsg;						//Last class E message
	std::list<std::string> saved_errors;	//last N class E messages
	std::string fsterr;						//First class E message since last reset

	//Used to ensure recursive exceptions don't occur when closing a user down, which
	//would usually involve issuing a number of messages.  If there are IO errors to the 
	//user's output device, this is set, and then the thread is closed.
	bool output_broken;

	//V2 - Jan 07.  So session level can see if a line is coming from here.
	bool issuing;

	void TerminalWriteLine(const std::string&);
	static void AppendRCToMessage(std::string&, int);

public:
	MsgRouter(CoreServices*, util::LineOutput* dest);
	void InitializeHistory();

	//Manipulate user specific MSGCTLs
	void ClearAllMsgCtl();
	void ClearMsgCtl(const int msgnum);
	void SetMsgCtl(const int msgnum, const MsgCtlOptions& newsettings);
	MsgCtlOptions GetMsgCtl(const int msgnum) const;
	
	//These are used internally sometimes to disable/enable all info/error messages
	int GetParmMsgCtl() const {return parm_msgctl;}
	int SetParmMsgCtl(int m) {int n = parm_msgctl; parm_msgctl = m; return n;}
	int SetInfoOff() {int n = parm_msgctl; parm_msgctl |= MSGCTL_SUPPRESS_INFO; return n;}
	int SetInfoOn() {int n = parm_msgctl; parm_msgctl &= (MSGCTL_ALL_BITS ^ MSGCTL_SUPPRESS_INFO); return n;}
	int SetErrorsOff() {int n = parm_msgctl; parm_msgctl |= MSGCTL_SUPPRESS_ERROR; return n;}
	int SetErrorsOn() {int n = parm_msgctl; parm_msgctl &= (MSGCTL_ALL_BITS ^ MSGCTL_SUPPRESS_ERROR); return n;}
	int SetPrefixOff() {int n = parm_msgctl; parm_msgctl |= MSGCTL_SUPPRESS_PREFIX; return n;}
	int SetPrefixOn() {int n = parm_msgctl; parm_msgctl &= (MSGCTL_ALL_BITS ^ MSGCTL_SUPPRESS_PREFIX); return n;}
	void NotifyOutputBroken(const std::string&);

	//Issues a message, returning true if it's currently set as an error message, in which
	//case the caller can take appropriate action if it wants to.
	bool Issue(const int msgnum, const std::string& msgtext);
	bool Issue(const int msgnum, const char* msgtext);
	//V1.3a. Nov 06. For the RESET command.
	void IssueTerminalInfo(const std::string&);

	//Manipulate general admin info
	const std::string& GetErrmsg() const {return errmsg;}
	const std::string& GetFsterr() const {return fsterr;}
	std::list<std::string> GetSavedErrors() {return saved_errors;}
	int GetErrorCount() const {return error_count;}
	int GetErrorCount_Total() const {return error_count_total;}
	void ClearErrorCount(int n = 0) {error_count = n;}
	bool LastMessagePrinted() {return last_message_printed;}
	bool LastMessageAudited() {return last_message_audited;}
	int GetTotalMessagesPrinted() {return messages_printed_total;}
	int GetTotalMessagesAudited() {return messages_audited_total;}
	void ClearErrmsgAndFsterr() {errmsg = std::string(); fsterr = std::string();}

	//Parameterization
	std::string ResetParm(const std::string&, const std::string&);
	std::string ViewParm(const std::string&, bool) const;

	//Normally set by messaging, but also accessed by $JOBCODE.
	static int GetJobCode() {return static_parm_retcode.Value();}
	static void SetJobCode(int n) {static_parm_retcode.Set(n);}

	//Used by daemons
	int GetHiRetcode() {return hi_retcode;}
	void SetHiRetcode(int, const std::string& = std::string());
	const int GetHiMsgNum() {return hi_retcode_msgnum;}
	const std::string& GetHiMsgText() {return hi_retcode_msgtext;}
	void ClearHiRetCodeAndMsg() {hi_retcode = 0; hi_retcode_msgnum = 0; hi_retcode_msgtext = std::string();}

	//V2
	std::string PrefixedMessageString(int, const std::string&, int);
	bool Issuing() {return issuing;}
};

}	//close namespace

#endif
