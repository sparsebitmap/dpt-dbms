//****************************************************************************************
//An interface for viewing statistics, much like the parameter viewer.
//****************************************************************************************

#if !defined(BB_STATVIEW)
#define BB_STATVIEW

#include <map> 
#include <vector> 
#include <string> 
#include "const_stat.h" 

namespace dpt {

class DatabaseFileContext;
class DatabaseFile;
class Statisticized;
class StatRefTable;
class CoreServices;
class ULSourceProgram;

//****************************************************************************************
class StatViewer {

	CoreServices* core;

	static StatRefTable* reftable;
	std::vector<Statisticized*> holders;
	std::map<std::string, Statisticized*> holder_xref;
	bool registration_complete;

	std::string current_activity;
	const ULSourceProgram* current_source_code;
	std::string t_request_cache;

	friend class Statisticized;
	void RegisterHolder(Statisticized* h) {holders.push_back(h);}
	void RegisterStat(const std::string&, Statisticized*);

	friend class CoreServices;
	StatViewer(CoreServices* c) 
		: core(c), registration_complete(false), current_source_code(NULL) {};
	static void SetRefTable(StatRefTable* r) {reftable = r;}

#ifdef _BBDBAPI

	DatabaseFile* ConvertContext(DatabaseFileContext*) const;

public:
	//A single stat
	_int64 View(const std::string&, const StatLevel, DatabaseFile* = NULL) const;
	_int64 View(const std::string& s, const StatLevel l, DatabaseFileContext* c) const
		{return View(s, l, ConvertContext(c));}

	//All nonzero stats
	std::string UnformattedLine(const StatLevel, DatabaseFile* = NULL) const;
	std::string UnformattedLine(const StatLevel l, DatabaseFileContext* c) const
		{return UnformattedLine(l, ConvertContext(c));}

#else

public:
	_int64 View(const std::string&, const StatLevel) const;
	std::string UnformattedLine(const StatLevel) const;

#endif

	void StartActivity(const std::string&, const ULSourceProgram* = NULL); //API: use null
	void EndActivity();

	const std::string& CurrentActivity() {return current_activity;}

#ifdef _BBHOST
	std::string CurrentProcName();
	std::string CurrentProcFile();
	std::string CurrentProcInfo();
#endif

	const std::string& TRequestCache() {return t_request_cache;} //a nice formatted line

	StatRefTable* GetRefTable() {return reftable;}
	void CompleteRegistration() {registration_complete = true;}
};

}	//close namespace

#endif
