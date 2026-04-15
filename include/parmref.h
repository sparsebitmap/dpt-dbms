//****************************************************************************************
//Reference information for system parameters.
//We use the same structure for both numeric and alpha parameters.  This is because the 
//only difference is in the validation, and it makes things more convenient not to have 
//to fiddle about with a virtual base class.  Might change this at some point.
//****************************************************************************************

#if !defined(BB_PARMREF)
#define BB_PARMREF

#include <string> 
#include <map>
#include <vector> 

namespace dpt {

enum ParmType		{num, alpha, not_emulated};
enum ParmCategory	{system, user, fparms, tables, nocat, all};

enum ParmResettability	{resettable, never_resettable, inifile_only, not_inifile};

struct ParmRefInfo {
	std::string		default_value;
	std::string		validation_pattern;		//only for strings
	_int32			maximum_value;			//only for numbers
	_int32			minimum_value;			//"
	std::string		description;
	ParmType		type;
	ParmCategory	category;
	ParmResettability	resettability;

	//Default constructor required by <map> but not otherwise used
	ParmRefInfo() {}
	ParmRefInfo(const char*, _int32, _int32, const char*, ParmCategory, ParmResettability);
	ParmRefInfo(const char*, const char*, const char*, ParmCategory, ParmResettability);
	ParmRefInfo(const char*, ParmType, ParmCategory);
};

//****************************************************************************************
//There should only be one object of this class in the system.  It is not declared as
//static in CoreServices, because of the complex construction order, so we have a check
//via the <created> member variable.
//****************************************************************************************
class ParmRefTable {
	static bool created;

	std::map<std::string, ParmRefInfo> data;

	//For smaller code building the table
	void StoreEntry(const char*, const char*, _int32, _int32, 
		const char*, ParmCategory, ParmResettability = resettable);
	void StoreEntry(const char*, const char*, const char*, 
		const char*, ParmCategory, ParmResettability = resettable);
	void StoreEntry(const char*, const char*, 
			ParmType = not_emulated, ParmCategory = nocat);

	//Extra tier parms
#ifdef _BBDBAPI
	void DBAPIExtraConstructor();
#endif
#ifdef _BBHOST
	void FullHostExtraConstructor();
#endif

public:
	ParmRefTable();
	~ParmRefTable() {created = false;} //V2.16
	const ParmRefInfo& GetRefInfo(const std::string& parmname) const;

	//Appends to the result vector
	int GetParmsInCategory(std::vector<std::string>&, const std::string&) const;

	//Used when setting initial system overrides and also during RESET
	std::string ValidateResetDetails(const std::string& parmname, const std::string& val, bool = false) const;
};

}	//close namespace

#endif
