//****************************************************************************************
//Controls access to the MSGCTL ini file, and maintains the set of parameter values in 
//effect for this run.  No direct messaging from here, only exceptions.
//****************************************************************************************

#if !defined(BB_MSGINI)
#define BB_MSGINI

#include <string>
#include <map>
#include <vector>
#include "msgref.h"

namespace dpt {

//****************************************************************************************
//There should only be one object of this class in the system.  It is not declared as
//static in CoreServices, because of the complex construction order, so we have a check
//via the <created> member variable.
//****************************************************************************************
class MsgCtlIniSettings {
	static bool created;

	MsgCtlRefTable* reftable;
	std::map<int, MsgCtlOptions> data;
	int num_overrides;
public:
	MsgCtlIniSettings(const std::string& inifilename, MsgCtlRefTable*);
	~MsgCtlIniSettings() {created = false;} //V2.16

	MsgCtlOptions GetMsgCtl(const int msgnum) const;
	void SummarizeOverrides(std::vector<std::string>*) const;
	int NumOverrides() {return num_overrides;}
};

}	//close namespace

#endif
