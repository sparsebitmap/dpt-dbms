//****************************************************************************************
//M204-style numbered messaging, with similar control options as M204.
//****************************************************************************************

#if !defined(BB_API_MSGROUTE)
#define BB_API_MSGROUTE

#include <string>

#include "msgopts.h"

namespace dpt {

class MsgRouter;

class APIMsgRouter {
public:
	MsgRouter* target;
	APIMsgRouter(MsgRouter* t) : target(t) {}
	APIMsgRouter(const APIMsgRouter& t) : target(t.target) {}
	//-----------------------------------------------------------------------------------

	//Manipulate user-specific MSGCTLs
	void ClearAllMsgCtl();
	void ClearMsgCtl(const int msgnum);
	void SetMsgCtl(const int msgnum, const MsgCtlOptions& newsettings);
	MsgCtlOptions GetMsgCtl(const int msgnum) const;
	
	//Disable/enable all info/error messages - shortcut for resetting the MSGCTL parameter
	int SetInfoOff();
	int SetInfoOn();
	int SetErrorsOff();
	int SetErrorsOn();
	int SetPrefixOff();
	int SetPrefixOn();

	//Mostly used by DPT, but can be useful for API programs e.g. in exception handlers,
	//or when MSGCTL settings are manipulated.  The rc says whether the msg was CLASS=E.
	bool Issue(const int msgnum, const std::string& msgtext);

	//General admin and info
	const std::string& GetErrmsg() const;
	const std::string& GetFsterr() const;
	int GetErrorCount() const;
	int GetErrorCount_Total() const;
	void ClearErrorCount();
	bool LastMessagePrinted();
	bool LastMessageAudited();
	int GetTotalMessagesPrinted();
	int GetTotalMessagesAudited();
	void ClearErrmsgAndFsterr();
	void InitializeHistory();

	//High water mark for the user
	int GetHiRetcode();
	void SetHiRetcode(int hrc, const std::string& text = std::string());
	const int GetHiMsgNum();
	const std::string& GetHiMsgText();
	void ClearHiRetCodeAndMsg();
	
	//High water mark for the whole run
	static int GetJobCode();
	static void SetJobCode(int);
};

} //close namespace

#endif
