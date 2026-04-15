//*******************************************************************************************
//Classes representing and manipulating M204-stylee patterns, and parts thereof.  This 
//functionality is used in a surprisingly wide range of situations within the system.
//Consult BB documentation for full details and examples of how it works.
//NB the main interface class "Pattern" is declared at the bottom.  All run time diagnostics 
//from parsing are thrown as dpt::Exceptions to be handled by the caller.
//*******************************************************************************************

#if !defined(BB_PATTERN)
#define BB_PATTERN

#include <vector>
#include <string>
#include "bitmap3.h"

//The breakdown function is only to help with debugging this code
#ifdef _DEBUG
#include <list>
#define DEBUG_VIRTUAL_BREAKDOWN_FUNCTION virtual std::list<std::string> Breakdown() = 0;
#define DEBUG_BREAKDOWN_FUNCTION std::list<std::string> Breakdown();
#else
#define DEBUG_VIRTUAL_BREAKDOWN_FUNCTION 
#define DEBUG_BREAKDOWN_FUNCTION 
#endif

namespace dpt { namespace util {

const std::string pattdummy("¬");	//unlikely character may help speed slightly
const int TOPLEVEL = -1;
//UL defines the maximum repeat in e.g. /10(A,B) as 255.  I'm happy to go along with this
//as the way i've coded it repeats are big memory gobblers.
const int PATRPTMAX = 255;

std::string PattErrorString(std::string pattern, int pos, std::string details);

//*******************************************************************************************
//A "component" is a single character, some kind of character placeholder, a wildcard, or
//a list of sub-patterns (with optional repeat count).
//The return value of Match() here is a set of flags denoting whether 
//the component matched the first N characters of the string, (and therefore denotes
//the set of positions at which testing for the next pattern component should be attempted.
//Note that matching a string of length zero is possible (wildcard, empty set etc.), and 
//this is denoted by the zero bit of the bitmap.  All off means no match of any kind.
//*******************************************************************************************
struct PatternComponent {
	DEBUG_VIRTUAL_BREAKDOWN_FUNCTION
	virtual BitMap Match(const std::string& matchstring, int stroffset) = 0;
	virtual ~PatternComponent() {}
};

//*******************************************************************************************
//This represents an atomic pattern, in the sense of a member of a bracketed list.
//It may well have sub-lists within it though.  The "meat" of all patterns lives 
//in these (in the vector).  It is never a member of its own components vector directly
//though (so it is not derived from PatternComponent), but it can be indirectly, as
//a member of a SinglePatternList.
//*******************************************************************************************
class SinglePattern {
	//Structure of the pattern
	std::vector<PatternComponent*> components;
	//Other information gathered for heuristic purposes
	std::string* fixed_prefix;	//only used at the top level
public:
	//Can be null, e.g A,,C
	SinglePattern() : fixed_prefix(NULL) {}
	~SinglePattern();
	int Parse(const std::string& pattstring, int pattoffset, bool toplevel);
	std::string* HandOverFixedPrefix();

	DEBUG_BREAKDOWN_FUNCTION
	BitMap Match(const std::string& matchstring, int stroffset, int pattpos = 0);

	//Used to create temporaries during RepeatedPatternList::Match
	void AddComponent(PatternComponent*);
	void Clear();
};
	
//*******************************************************************************************
//Single character type components
//*******************************************************************************************
//Certain characters are disallowed such as full stops (pre V51)
class AnyCharacter : public PatternComponent {
	char thechar;
public:	
	AnyCharacter(char);	
	DEBUG_BREAKDOWN_FUNCTION
	BitMap Match(const std::string& matchstring, int stroffset);
};
//!x: Anything character at all can be one of these - must match exactly
class EscapeCharacter : public PatternComponent {
	char thechar;
public:	
	EscapeCharacter(char);	
	DEBUG_BREAKDOWN_FUNCTION
	BitMap Match(const std::string& matchstring, int stroffset);
};
//=xx: As above, but it is specified by 2 digits from 0-9 and A-F
class HexCharacter : public PatternComponent {
	char thechar;
public:	
	HexCharacter(char);
	DEBUG_BREAKDOWN_FUNCTION
	BitMap Match(const std::string& matchstring, int stroffset);
};
//+: Any chararacter matches a placeholder
struct Placeholder : public PatternComponent {
	DEBUG_BREAKDOWN_FUNCTION
	BitMap Match(const std::string& matchstring, int stroffset);
};
//@: All letters match, both upper and lower case
struct AlphaPlaceholder : public PatternComponent {
	DEBUG_BREAKDOWN_FUNCTION
	BitMap Match(const std::string& matchstring, int stroffset);
};
//#: Just digits match
struct NumericPlaceholder : public PatternComponent {
	DEBUG_BREAKDOWN_FUNCTION
	BitMap Match(const std::string& matchstring, int stroffset);
};
//(a-b): Any character in the range match (either char may be given as =xx)
class CharacterRange : public PatternComponent {
	unsigned char minimum_value;	
	unsigned char maximum_value;
public:	
	CharacterRange(char, char);
	DEBUG_BREAKDOWN_FUNCTION
	BitMap Match(const std::string& matchstring, int stroffset);
};

//*******************************************************************************************
//Variable-number-of-characters type components.  These are more awkward, since they may
//match several substrings of the matchstring.  e.g. (A,ABC) matches substrings of length
//1 and 3 when "ABC" is matched to it.
//*******************************************************************************************
//The most popular pattern in the world, and most awkward!  Matches zero to infinity chars.
struct Wildcard : public PatternComponent {
	DEBUG_BREAKDOWN_FUNCTION
	BitMap Match(const std::string& matchstring, int stroffset);
};

//This object is a comma-separated list of atomic paterns.  It can occur on its own, e.g. 
//"J(ACK,OHN)SON", as part of a repeated list (e.g. "/3(A,B,C)"), or at the top level, 
//where a complete pattern such as "A,B,C" is allowed, implying "(A,B,C)".
class SinglePatternList : public PatternComponent {
	std::vector<SinglePattern*> thelist;
	std::string* fixed_prefix;	//only used at the top level - helps with database searches
//*Note* Potential for assorted match-speeding info to be gathered during parse too

public:	
	//Can be null e.g. /3(), although this would probably only be useful in generated code.
	//In any case, the object always starts empty, then is built up by Parse().
	SinglePatternList() : fixed_prefix(NULL) {}
	~SinglePatternList();
	int Parse(const std::string& pattstring, int pattoffset = 0, bool toplevel = false);

	//V3.02
	void ClearFixedPrefix() {if (fixed_prefix) delete fixed_prefix; fixed_prefix = NULL;}

	DEBUG_BREAKDOWN_FUNCTION
	BitMap Match(const std::string& matchstring, int stroffset);
	std::string* FixedPrefix() {return fixed_prefix;}
	bool IsEmpty() {return thelist.empty();}
};

//As above, but with a repeat count.  Never at the top level - "/3(A,B)" is interpreted as
//"(/3(A,B))"
class RepeatedPatternList : public PatternComponent {
	int minimum_count;
	int maximum_count;
	SinglePatternList* thelist;
public:	
	RepeatedPatternList() : thelist(NULL) {};
	~RepeatedPatternList();
	int Parse(const std::string& pattstring, int pattoffset = 0);

	DEBUG_BREAKDOWN_FUNCTION
	BitMap Match(const std::string& matchstring, int stroffset);
};

//*******************************************************************************************
//The high-level user interface class is just a SinglePatternList, but with a constructor
//that parses the complete pattern and constructs all the sub-component objects.  In addition
//it has a function IsLike() which returns a bool instead of a bitset.
//*******************************************************************************************
class Pattern {
	SinglePatternList* thepattern;

	//V3.0.  Decided to do something about those pesky users who so love wildcard searches
	//of the form *XXXXX*.  These are generally slow with the "full" pattern matcher because
	//of the leading wildcard, but are actually trivial to evaluate with std::string::find.
	//Note that similar tuning could be done for the matching of strings to patterns of the
	//form XXX* (i.e. simple TRAILING wildcard only) but they are already handled OK by the 
	//DBMS via the fixed prefix, so it would just be CPU tuning here in this class and it's
	//not really too bad as it is, it just takes a bit of a roundabout route :)
	bool simple_leading_wildcard;
	std::string simple_leading_wildcard_fixedpart;
	bool simple_leading_wildcard_also_trailing_wildcard;

	//V3.02
	bool no_case_option;

public:	
	Pattern();
	Pattern(const std::string&);
	Pattern(const char*);
	~Pattern();

	DEBUG_BREAKDOWN_FUNCTION

	//Calls Parse() for the top level list
	void SetPattern(const std::string& p);
	//Calls Match() for the top level list
	bool IsLike(const std::string& s) const;

	//Can be used to improve performance if many values are tested against one pattern.
	//Normally a user would not need this as the pattern class takes account of it as
	//appropriate.  However in the case of database searches it we can avoid a lot of
	//b-tree leaf retrievals if we take account of this when walking the tree.
	//std::string* FixedPrefix() { //V3.02
	std::string* FixedPrefixAndCase() {
		//V3.0
		//return thepattern->FixedPrefix();
		//V3.02
		//return (thepattern) ? thepattern->FixedPrefix() : NULL;
		return (thepattern && !no_case_option) ? thepattern->FixedPrefix() : NULL;
	}

	//Added much later for a similar reason.  Why did I chose to use a pointer that shows
	//null for both an empty pattern and one where there is no fixed prefix???!!! I should
	//probably rework throughout but I can't face it - it's hellishly complicated.
	//This test is only used in one place in the main DBMS anyway.
	bool IsNullPattern() {
		if (thepattern) 
			return thepattern->IsEmpty(); 
		//V3.0
		//return true;
		return !simple_leading_wildcard;
	}

	//V2 Jan 06
	static void ConvertFromSiriusStyle(std::string&);
	void SetPatternSiriusStyle(const std::string&);
};

}} //end namespace

#endif
