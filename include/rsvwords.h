//******************************************************************************************
//Words that have special meaning to the UL compiler and therefore get special treatment
//at parse time and when defining fields.
//******************************************************************************************

#if !defined BB_RSVWORDS
#define BB_RSVWORDS

#include <string>
#include <map>

namespace dpt {

class ReservedWords {

	//V1.1 18/6/06 - Most use-cases now have their own list of terminator words.
	//This class is now only used where the lists are long and we might feel morally 
	//obliged to do a table look up.  Might one day change to always use table lookup
	//which would be neater but would grate for cases with e.g. only one reserved word.
	//Changed from a set to a map.
	static char usage_fd;
	static char usage_expr;
	static char usage_prtexpr;

	static std::map<std::string, char> data;
	static void AddEntry(const std::string word, const char usages = 0) {data[word] = usages;}

	static bool FindUsagedWord(const std::string& w, char usage) {
		std::map<std::string, char>::const_iterator iter = data.find(w);
		if (iter == data.end()) return false;
		if (iter->second & usage) return true; return false;}

public:
	static void Initialize();

	//See above comment
	static bool FindAnyWord(const std::string& w) {return (data.find(w) != data.end());}
	static bool FindFDWord(const std::string& w) {return FindUsagedWord(w, usage_fd);}
	static bool FindExprWord(const std::string& w) {return FindUsagedWord(w, usage_expr);}
	static bool FindPrtExprWord(const std::string& w) {return FindUsagedWord(w, usage_prtexpr);}
};

}	//close namespace

#endif

