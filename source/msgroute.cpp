
#include "stdafx.h"

#include "msgroute.h"

//Utils
#include "dataconv.h"
#include "lineio.h"
//API tiers
#include "msgini.h"
#include "audit.h"
#include "parmini.h"
#include "core.h"
//Diagnostics
#include "except.h"
#include "msg_core.h"

#ifdef _BBHOST
#include "msg_ul.h"
#endif

namespace dpt {

//define static members
MsgCtlRefTable*		MsgRouter::reftable = NULL;
MsgCtlIniSettings*	MsgRouter::inisettings = NULL;
Audit*				MsgRouter::audit = NULL;

//1000 means some kind of startup failure before message control is initialized
ThreadSafeLong		MsgRouter::static_parm_retcode = ThreadSafeLong(1000);

//******************************************************************************************
//Object constructor
//******************************************************************************************
MsgRouter::MsgRouter(CoreServices* c, util::LineOutput* dest) 
: core(c), output(dest), output_broken(false), issuing(false)
{
	InitializeHistory();

	//Initialize parameters
	parm_msgctl = GetIniValueInt("MSGCTL");
	parm_errsave = GetIniValueInt("ERRSAVE");
	
	//Register parms for VIEW and RESET 
	ViewerResetter* parmvr = core->GetViewerResetter();
	RegisterParm("MSGCTL", parmvr);
	RegisterParm("ERRSAVE", parmvr);
	RegisterParm("RETCODE", parmvr);

	//We can start issuing proper numbered messages now
	if (core->GetUserNo() == 0)
		static_parm_retcode.Set(0);
}

//***********************************
void MsgRouter::InitializeHistory()
{
	error_count_total = 0;
	messages_printed_total = 0;
	messages_audited_total = 0;

	ClearErrorCount();
	ClearErrmsgAndFsterr();
	ClearHiRetCodeAndMsg();
}

//******************************************************************************************
//In response to the MSGCTL command from a user.  The processing of that command will first
//read the existing settings, then add to them.
//******************************************************************************************
void MsgRouter::SetMsgCtl(const int msgnum, const MsgCtlOptions& newsettings)
{
	//The message number must be valid
	MsgCtlOptions mc = inisettings->GetMsgCtl(msgnum);
	settings[msgnum] = newsettings;
}

//******************************************************************************************
//This gives the message control settings in effect at the current time.  If uses a user
//specific setting, the ini file setting or the defaults.  The MSGCTL command will first do
//this before incorporating the options given on the command.
//******************************************************************************************
MsgCtlOptions MsgRouter::GetMsgCtl(const int msgnum) const
{
	std::map<int, MsgCtlOptions>::const_iterator sci = settings.find(msgnum);
	if (sci != settings.end()) {	
		return sci->second;
	}
	else {
		//No user override, so use system value
		return inisettings->GetMsgCtl(msgnum);
	}
}

//****************************
void MsgRouter::ClearAllMsgCtl()
{
	settings.clear();
}

//****************************
void MsgRouter::ClearMsgCtl(const int msgnum)
{
	//The message number must be valid
	MsgCtlOptions mc = inisettings->GetMsgCtl(msgnum);
	settings.erase(msgnum);
}

//******************************************************************************************
//This function routes the message optionally to the audit trail and optionally to 
//some user-specific destination.  If the message is curerntly set to class=E, the
//function returns true, and the message issuer might take appropriate action.  Generally
//though, action is taken retrospectively based in the error count.
//******************************************************************************************
bool MsgRouter::Issue(const int msgnum, const std::string& msgtext)
{
	//Active settings for the message
	MsgCtlOptions st;
	try {
		st = GetMsgCtl(msgnum);
	}
	//Doing this because I keep forgetting to insert new messages in the ref table,
	//and it's an annoying bug to find.
	catch (Exception&) {
		Issue(BUG_MISC, std::string("Bug: Error retrieving message control settings for code ")
			.append(util::IntToString(msgnum))
			.append(" - missing ref info?"));
		throw;
	}
	
	//Error message history admin - NB maintained even if the message is NOTERM+NOAUDIT
	if (st.error) {
		errmsg = msgtext;

		//add to list of errors for VIEW ERRORS
		saved_errors.push_back(errmsg);
		if (saved_errors.size() > (size_t)parm_errsave) 
			saved_errors.pop_front();

		if (error_count == 0) 
			fsterr = msgtext;
		error_count++;
		error_count_total++;
	}

	//Prepend message code if appropriate
	std::string msgstring = PrefixedMessageString(msgnum, msgtext, 0);

	//Audit if that's set in msgctl.  We do this before writing to the user's terminal so
	//that if an IO error there cased the message we still get an audit trail line.
	last_message_audited = false;
	if (st.audit) {
		const char* msgtype = "MSG";
		if (st.error)
			msgtype = "ERR"; 

		//Nice feature - show non-zero retcodes in the audit
		if (st.rcode == 0) 
			core->AuditLine(msgstring, msgtype);
		else {
			std::string audstring(msgstring);
			AppendRCToMessage(audstring, st.rcode);
			core->AuditLine(audstring, msgtype);
		}
		last_message_audited = true;
		messages_audited_total++;
	}

	//Output to user if msgctl set appropriately
	last_message_printed = false;
	if (st.term && !output_broken) {

		//Terminal output of all error messages or all info messages can also be 
		//suppressed using the MSGCTL parameter (which is actually how the APSY 
		//driver achieves it).
		if ((st.error && !(parm_msgctl & MSGCTL_SUPPRESS_ERROR)) 
		|| (!st.error && !(parm_msgctl & MSGCTL_SUPPRESS_INFO))) {

			//Terminal messages always begin with this.  The IODev7 client relies on it
			//for a couple of things, so that would need looking at if it became an option.
			std::string term_msg;
			term_msg.reserve(msgstring.length() + 16);
			term_msg.append("*** ");

			//Error messages show the error count
			if (st.error) {
				char buff[8];
				sprintf(buff, "%.1d ", error_count);
				term_msg.append(buff);
			}

			//Finally end the line with the message string itself
			term_msg.append(msgstring);

			//This message is a unique case in that it shows up differently on the audit
			//trail to the terminal.  That's because we will be printing the text of the
			//UL line where a runtime error occurred. For smart edit to work properly,
			//the line before the proc line details has to be a counting error, so issuing
			//another info message after this would not be ideal, and issuing another error
			//message messes up $ERRMSG as a useful tool for the UL coder.
			//This extra text does not feature in $ERRMSG.
#ifdef _BBHOST
			if (msgnum == UL_RUNTIME_ERROR_SHOWUL)
				term_msg.append(" - last line executed/attempted was:");
#endif
			TerminalWriteLine(term_msg);

			last_message_printed = true;
			messages_printed_total++;
		}
	}

	//Update the hi-score as regards message severity for this run of the system
	if (st.rcode > static_parm_retcode.Value()) 
		static_parm_retcode.Set(st.rcode); //threadsafe but iffy as not atomic

	//Useful on daemon threads where we might often want to know the worst thing that happened
	if (st.rcode > hi_retcode) {
		hi_retcode = st.rcode;
		hi_retcode_msgnum = msgnum;
		hi_retcode_msgtext = msgtext;
	}

	if (st.error) 
		return true; 
	else 
		return false;
}

//***********************
//Just did this to make the code a bit smaller, as this is one of the most heavily
//used functions in the system.  It's not that performance sensitive.
bool MsgRouter::Issue(const int msgnum, const char* msgtext)
{
	return Issue(msgnum, std::string(msgtext));
}

//***********************
//V2: Created so we can retrospectively reconstruct a message.
std::string MsgRouter::PrefixedMessageString
(int msgnum, const std::string& msgtext, int append_rc)
{
	std::string msgstring;
	msgstring.reserve(msgtext.length() + 16);
	if (!(parm_msgctl & MSGCTL_SUPPRESS_PREFIX)) {
		char buff[16];
		sprintf(buff, "DPT.%.4d ", msgnum);
		msgstring.append(buff);
	}
	msgstring.append(msgtext);

	if (append_rc)
		AppendRCToMessage(msgstring, append_rc);

	return msgstring;
}

//***********************
void MsgRouter::AppendRCToMessage(std::string& msgstring, int append_rc)
{
	char buff[10];
	sprintf(buff, " (RC=%d)", append_rc);
	msgstring.append(buff);
}

//***********************
void MsgRouter::TerminalWriteLine(const std::string& term_msg)
{
	try {
		issuing = true;
		output->WriteLine(term_msg.c_str(), term_msg.length());
		issuing = false;
	}
	catch (Exception& e) {
		issuing = false;
		NotifyOutputBroken(e.What());
	}
	catch (...) {
		issuing = false;
		NotifyOutputBroken("Unknown exception while issuing message");
	}
}

//***********************
void MsgRouter::SetHiRetcode(int newcode, const std::string& newtext)
{
	hi_retcode = newcode;
	hi_retcode_msgtext = newtext;

	//Since daemons will sometimes retrospectively "issue" the message at termination
	hi_retcode_msgnum = USER_SET_MSG_HWM;
}

//******************************************************************************************
//V1.3a. The output from the RESET command seems to be a kind of informational message 
//rather than command output in the normal sense, since it is suppressed in APSY and 
//MSGCTL = 0.  This func issues a message with no code/time/audit etc. options.
//******************************************************************************************
void MsgRouter::IssueTerminalInfo(const std::string& msg)
{
	if (parm_msgctl & MSGCTL_SUPPRESS_INFO)
		return;
	if (output_broken)
		return;

	TerminalWriteLine(msg);
}

//***********************
//See docs for details 
void MsgRouter::NotifyOutputBroken(const std::string& reason)
{
#ifdef _WINDOWS
	TRACE("Output broken - %s\n", reason.c_str());
#endif

	output_broken = true;

	//This code is used in a few places to ensure the thread closes properly
	throw Exception(USER_IO_ERROR, reason);
}











//******************************************************************************************
//Parameter viewing and resetting
//******************************************************************************************
std::string MsgRouter::ResetParm(const std::string& parmname, const std::string& newvalue)
{
//Non-resettable parameters
	//none

//Resettable parameters. 
	if	(parmname == "MSGCTL")	{
		//all bit settings between the min (0) and the max (7) are valid
		//V2.03.  That is no longer true.

		int tryi = util::StringToInt(newvalue);
		int newi = tryi & MSGCTL_ALL_BITS;
		parm_msgctl = newi;
		if (newi != tryi)
			return util::IntToString(newi);
	}
	else if	(parmname == "ERRSAVE")	{
		parm_errsave = util::StringToInt(newvalue);
		//drop older messages
		for (int x = saved_errors.size(); x > parm_errsave; x--) {
			saved_errors.pop_front();
		}
	}

	else if	(parmname == "RETCODE")	{
		static_parm_retcode.Set(util::StringToInt(newvalue));
		core->AuditLine(std::string("RETCODE manually reset to ")
			.append(newvalue), "SYS");
	}

//Anything else passed in
	else {
		return ResetWrongObject(parmname);
	}
		
	return newvalue;
}

//******************************************************************************************
std::string MsgRouter::ViewParm(const std::string& parmname, bool format) const
{
//Member parms.
	if (parmname == "ERRSAVE") return util::IntToString(parm_errsave);
	if(parmname == "MSGCTL")   return util::IntToString(parm_msgctl);

//Static parms
	if (parmname == "RETCODE") return util::IntToString(static_parm_retcode.Value());

//Anything else passed in
	return ViewWrongObject(parmname);
}

} // close namespace
