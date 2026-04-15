//****************************************************************************************
//Reference inforfmation for statistics.  This is mostly used to help when processing
//the $VIEW/$STAT functions and the monitor command, so that we know what's a valid
//stat and what's not.  It works much like parameter viewing.
//****************************************************************************************

#if !defined(BB_STATREF)
#define BB_STATREF

#include <vector> 
#include <string> 
#include <map>

namespace dpt {

struct StatRefInfo {
	std::string		description;
	bool			filestat;
	//Default constructor required by <map>
	StatRefInfo() {}
	StatRefInfo(const char* s, bool b) : description(s), filestat(b) {}
};

//****************************************************************************************
class StatRefTable {
	static bool created;

	std::map<std::string, StatRefInfo> data;

	//For smaller code building the table
	void StoreEntry(const char*, const char*, bool = false);

	//Extra tier stats
#ifdef _BBDBAPI
	void DBAPIExtraConstructor();
#endif
#ifdef _BBHOST
	void FullHostExtraConstructor();
#endif

public:
	StatRefTable();
	~StatRefTable() {created = false;} //V2.16

	StatRefInfo GetRefInfo(const std::string& statname) const;

	void GetAllStatNames(std::vector<std::string>&) const;
};

}	//close namespace

#endif
