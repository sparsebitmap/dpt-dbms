//****************************************************************************************
//Controls access to the parm ini file, and maintains the set of parameter values in effect
//for this run.  No direct messaging from here, only exceptions.
//****************************************************************************************

#if !defined(BB_PARMINI)
#define BB_PARMINI

#include <string>
#include <map>
#include <vector>

namespace dpt {

class ParmRefTable;

//****************************************************************************************
//There should only be one object of this class in the system.  It is not declared as
//static in CoreServices, because of the complex construction order, so we have a check
//via the <created> member variable.
//****************************************************************************************
class ParmIniSettings {
	static bool created;

	ParmRefTable* reftable;
	bool some_used_max_or_min;
	struct ParmIniVal {
		std::string value;
		bool used_max_or_min;
		bool hexchar_supplied;
		bool hexnum_supplied;
		bool charquotes_supplied;

		ParmIniVal() {}	//required by <map>
		ParmIniVal(std::string& v, bool mmu, bool hcs, bool hns, bool cqs) 
			: value(v), used_max_or_min(mmu), 
				hexchar_supplied(hcs), hexnum_supplied(hns), charquotes_supplied(cqs) {}
	};
	std::map<std::string, ParmIniVal> data;
	int num_overrides;

public:
	ParmIniSettings(const std::string& inifilename, ParmRefTable*);
	~ParmIniSettings() {created = false;} //V2.16

	bool ParmIsNumeric(const std::string& parmname) const; //for debugging
	const std::string& GetParmValue(const std::string& parmname, const std::string* = NULL) const;
	void SummarizeOverrides(std::vector<std::string>*) const;
	int NumOverrides() {return num_overrides;}
	bool SUMOM() const {return some_used_max_or_min;}
};

}	//close namespace

#endif
