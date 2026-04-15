
#include "stdafx.h"

#include "fieldval.h"

#include "lockable.h"
#include "dataconv.h"
#include "charconv.h"
#include "except.h"
#include "msg_db.h"

#include <map>

#define LONGMSGTRUNC 30

namespace dpt {

//***********************************************************************************
void FieldValue::TooLong(const char* s1, const char* s2) 
{
	std::string fullstring(s1);
	if (s2)
		fullstring.append(s2);

	if (fullstring.length() > LONGMSGTRUNC)
		fullstring.resize(LONGMSGTRUNC);

	ThrowTooLong(fullstring.c_str(), NULL);
}

//***********************************************************************************
void FieldValue::ThrowTooLong(const char* s, const char* reason) const
{
	std::string strunc(s);
	if (strunc.length() > LONGMSGTRUNC)
		strunc.resize(LONGMSGTRUNC);

	std::string msg("String values longer than 255 bytes may not be ");
	msg.append( (reason) ? reason : "used here");
	msg.append(": '").append(strunc).append("...'");

	throw Exception(DML_STRING_TOO_LONG, msg);
}


//*************************************************************************************
//Decided to do a separate func for this even though it's identical to CompareString
//except for 1 line. Also putting it here saves a compile dependency on charconv.h.
//The inline policy from before was getting rather absurd with such large funcs anyway!
//*************************************************************************************
int FieldValue::CompareNoCaseString(const FieldValue& rhs) const 
{
	const char t = Type(); const char rt = rhs.Type();
	assert(Type() != FV_TYPE_DBL);

	int len = StrLen(); int rlen = rhs.StrLen();
	int shortest = (len < rlen) ? len : rlen;

	int cmp = util::MemCmpNoCase(CPChars(t), rhs.CPChars(rt), shortest);

	if (cmp != 0)
		return cmp;
	if (len == rlen)
		return 0;
	if (len < rlen)
		return -1; //shortest sorts lower if they match

	return 1;
} 

//*************************************************************************************
//To work around a problem with Cstr() where if the value is originally len 15, calls
//to StrLen() after CStr() will return 16.  This is not so much a basic design problem 
//of this class (which remains utterly perfect in its internal structural elegance)
//but a mistake in the blurring which has crept in between internal representation 
//and exposed API.  The Cstr function is only intended for niche cases anyway but now 
//it's there it should be made to work accurately at least.  Unfortunately there 
//really is nowhere inside the object to put a temporary buffer without compromising
//its compact fixed 16 byte structure, so this is a workaround which maintains the 
//basic efficiency of the CStr() function in the majority of cases, and takes the hit 
//of a little bit of memory work and buffer lock in the "broken" cases, namely len 15 
//values.  There is also a tiny extra overhead when destroying objects in all cases.
//*************************************************************************************
static std::map<FieldValue*, char*> cstr_fudge_buffers;
static Lockable cstr_fudge_lock;
bool FieldValue::any_cstr_fudges = false;

//***************
const char* FieldValue::LocalMaxLenCstrFudge() {
	LockingSentry ls(&cstr_fudge_lock);

	char* buff = new char[FV_MAX_LOCAL_LEN+1];
	memcpy(buff, CPLocalChars(), FV_MAX_LOCAL_LEN); 
	buff[FV_MAX_LOCAL_LEN] = 0;

	cstr_fudge_buffers[this] = buff;

	any_cstr_fudges = true;
	return buff;
}

//***************
//Once Cstr() has been used and the fudge invoked, this will be called for *ALL*
//len 15 fieldvalues until the object on which the fudge was invoked is killed.
void FieldValue::CstrFudgeCleanup() {
	LockingSentry ls(&cstr_fudge_lock);

	std::map<FieldValue*, char*>::iterator iter = cstr_fudge_buffers.find(this);
	if (iter != cstr_fudge_buffers.end()) {
		delete[] iter->second;
		cstr_fudge_buffers.erase(iter);
	}

	//But revert to optimal case when all fudged objects are gone
	if (cstr_fudge_buffers.size() == 0)
		any_cstr_fudges = false;
}


//*************************************************************************************
//For the debugger
//*************************************************************************************
#ifdef _DEBUG

//Static array of debug strings for speed (the call stack display in particular seems
//to balk at too much processing in here), and to avoid debugger memory leaks.

//Actually I can't stop this hanging the call stack window in DPT host functions where
//there are a lot of FieldValue parameters on the stack.  Disabled it for now.  I don't
//think its a threadsafety problem.

#define DBUFFSZ 10000
char dbuff[DBUFFSZ][32];
//ThreadSafeLong dbuffix(-1);
int dbuffix = -1;

const char* FieldValue::DebuggerExtractString() const
{
//	return "";
	const char type = Type();

	//Choose next static buffer or loop back to first.
//	int ix = dbuffix.Inc();
//	if (ix >= DBUFFSZ) {
//		dbuffix.Set(-1);     //slight MT vulnerability here (cosmetic only though)
//		ix = dbuffix.Inc();
//	}
	dbuffix++;
	if (dbuffix >= DBUFFSZ)
		dbuffix = 0;
	int ix = dbuffix;
		
	char* buff = dbuff[ix];

	if (type == FV_TYPE_DBL)
		sprintf(buff, "%.20G", RDData()->Data());

	else {
		int nchars = StrLen();
		if (nchars < 32) {
			memcpy(buff, StrChars(), nchars);
			buff[nchars] = 0;
		}
		else {
			memcpy(buff, StrChars(), 28);
			buff[28] = '.';
			buff[29] = '.';
			buff[30] = '.';
			buff[31] = 0;
		}
	}

	return buff;
}
#endif

} //close namespace




