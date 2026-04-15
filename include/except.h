//****************************************************************************************
//Exceptions used throughout DPT.  
//All exceptions have a numeric code and a text string.  Usually a message is issued
//by the infrastructure before throwing, in which case the message code is the same
//as the code held in the exception object.
//Derived classes containing more info are used in one or two cases (e.g. record locking).
//****************************************************************************************

#if !defined(BB_EXCEPT)
#define BB_EXCEPT

#include <string>
#ifdef _BBHOST
#include "string2.h" //V2.26.  Only using string2 in User Language for the time being.
#endif

namespace dpt {

class Exception {
	int msgcode;
	std::string msgtext;

public:
	Exception(int x, const char* t) : msgcode(x), msgtext(t) {}
	Exception(int x, const std::string& t = std::string()) : msgcode(x), msgtext(t) {}
	Exception(const std::string& t = std::string()) : msgcode(0), msgtext(t) {}

#ifdef _BBHOST
	Exception(int x, const std::string2& t) : msgcode(x), msgtext(t.c_str()) {}
	Exception(const std::string2& t) : msgcode(0), msgtext(t.c_str()) {}
	std::string2 What2() const {return std::string2(msgtext.c_str(), msgtext.length());}
#endif

	virtual ~Exception() {}

	const std::string& What() const {return msgtext;}
	virtual int Code() const {return msgcode;}
};

}	//close namespace

#endif
