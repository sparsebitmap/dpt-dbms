//****************************************************************************************
//An interface for viewing and resetting parameters.
//****************************************************************************************

#if !defined(BB_PARMVR)
#define BB_PARMVR

#include <map> 
#include <vector> 
#include <string> 

namespace dpt {

class Parameterized;
class ParmRefTable;
class DatabaseFileContext;
class DatabaseServices;
class CoreServices;

//****************************************************************************************
class ViewerResetter {
	//Used to validate parm names and new values during RESET (read only)
	static ParmRefTable* reftable;
	std::map<std::string, Parameterized*> data;

	friend class DatabaseServices;
	DatabaseServices* dbapi;

	friend class Parameterized;
	void Register(const std::string&, Parameterized*);

	friend class CoreServices;
	ViewerResetter() : data(std::map<std::string, Parameterized*>()), dbapi(NULL) {};
	static void SetRefTable(ParmRefTable* r) {reftable = r;}

public:
	ParmRefTable* GetRefTable() {return reftable;}

//--------------------------
//Database API build or higher
#ifdef _BBDBAPI

	//This returns the value actually set. (Note that the max or min might be substituted,
	//in which case it is up to the calling code to report this difference.)
	std::string Reset(const std::string& parmname, const std::string& newvalue, 
		DatabaseFileContext* file = NULL);

	//Retrieve parm value from the appropriate object
	std::string View(const std::string& parmname, bool fancyformat = true, 
		DatabaseFileContext* file = NULL) const;

	//Rudimentary alternative 
	int ViewAsInt(const std::string& p, DatabaseFileContext* f = NULL) const;

//--------------------------
//CoreServices-only build
#else
	std::string Reset(const std::string& parmname, const std::string& newvalue);

	std::string View(const std::string& parmname, bool fancyformat = true) const;

	int ViewAsInt(const std::string& p) const;

#endif
};

}	//close namespace

#endif
