//****************************************************************************************
//Reference information for message control parameters
//****************************************************************************************

#if !defined(BB_MSGREF)
#define BB_MSGREF

#include <map>
#include <string>

#include "msgopts.h"

namespace dpt {

//****************************************************************************************
//There should only be one object of this class in the system.  It is not declared as
//static in CoreServices, because of the complex construction order, so we have a check
//via the <created> member variable.
//****************************************************************************************
class MsgCtlRefTable {
	static bool AUDIT;
	static bool NOAUDIT;
	static bool TERM;
	static bool NOTERM;
	static bool ERR;
	static bool NOERR;

	static bool created;

	std::map<int, MsgCtlOptions> data;

	void StoreEntry(int, bool t, bool a, bool i, int r, bool tr = false);

	//Full host build - initialize session level messages
#ifdef _BBHOST
	void FullHostExtraConstructor();
#endif

public:
	MsgCtlRefTable();
	~MsgCtlRefTable() {created = false;} //V2.16

	MsgCtlOptions GetMsgCtl(const int msgnum) const;
};

//Msgctl command parsing
MsgCtlOptions ParseMsgCtlCommand(const std::string& line, const MsgCtlOptions& current);

}	//close namespace

#endif
