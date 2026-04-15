
#include "stdafx.h"

#include "dbapi\msgroute.h"
#include "msgroute.h"

namespace dpt {

//**********************************************
void APIMsgRouter::InitializeHistory()
{
	target->InitializeHistory();
}

//**********************************************
void APIMsgRouter::ClearAllMsgCtl()
{
	target->ClearAllMsgCtl();
}

void APIMsgRouter::ClearMsgCtl(const int msgnum)
{
	target->ClearMsgCtl(msgnum);
}

void APIMsgRouter::SetMsgCtl(const int msgnum, const MsgCtlOptions& newsettings)
{
	target->SetMsgCtl(msgnum, newsettings);
}

MsgCtlOptions APIMsgRouter::GetMsgCtl(const int msgnum) const
{
	return target->GetMsgCtl(msgnum);
}


//**********************************************
int APIMsgRouter::SetInfoOff()
{
	return target->SetInfoOff();
}

int APIMsgRouter::SetInfoOn()
{
	return target->SetInfoOn();
}

int APIMsgRouter::SetErrorsOff()
{
	return target->SetErrorsOff();
}

int APIMsgRouter::SetErrorsOn()
{
	return target->SetErrorsOn();
}

int APIMsgRouter::SetPrefixOff()
{
	return target->SetPrefixOff();
}

int APIMsgRouter::SetPrefixOn()
{
	return target->SetPrefixOn();
}


//**********************************************
bool APIMsgRouter::Issue(const int msgnum, const std::string& msgtext)
{
	return target->Issue(msgnum, msgtext);
}


//**********************************************
const std::string& APIMsgRouter::GetErrmsg() const
{
	return target->GetErrmsg();
}

const std::string& APIMsgRouter::GetFsterr() const
{
	return target->GetFsterr();
}

int APIMsgRouter::GetErrorCount() const
{
	return target->GetErrorCount();
}

int APIMsgRouter::GetErrorCount_Total() const
{
	return target->GetErrorCount_Total();
}

void APIMsgRouter::ClearErrorCount()
{
	target->ClearErrorCount(0);
}

bool APIMsgRouter::LastMessagePrinted()
{
	return target->LastMessagePrinted();
}

bool APIMsgRouter::LastMessageAudited()
{
	return target->LastMessageAudited();
}

int APIMsgRouter::GetTotalMessagesPrinted()
{
	return target->GetTotalMessagesPrinted();
}

int APIMsgRouter::GetTotalMessagesAudited()
{
	return target->GetTotalMessagesAudited();
}

void APIMsgRouter::ClearErrmsgAndFsterr()
{
	target->ClearErrmsgAndFsterr();
}


//**********************************************
int APIMsgRouter::GetHiRetcode()
{
	return target->GetHiRetcode();
}

void APIMsgRouter::SetHiRetcode(int hrc, const std::string& text)
{
	target->SetHiRetcode(hrc, text);
}

const int APIMsgRouter::GetHiMsgNum()
{
	return target->GetHiMsgNum();
}

const std::string& APIMsgRouter::GetHiMsgText()
{
	return target->GetHiMsgText();
}

void APIMsgRouter::ClearHiRetCodeAndMsg()
{
	target->ClearHiRetCodeAndMsg();
}


//**********************************************
int APIMsgRouter::GetJobCode()
{
	return MsgRouter::GetJobCode();
}

void APIMsgRouter::SetJobCode(int jc)
{
	MsgRouter::SetJobCode(jc);
}


} //close namespace


