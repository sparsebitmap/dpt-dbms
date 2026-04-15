//********************************************************************************************
// Pattern parsing and matching functions.
// V2.24.  Tons of instances throughout the code but this file had the most so the comment
//         goes here:  All cases where ints are compared to unsigned ints are fudged through
//         the gcc compiler (which doesn't like it) by casting the signed side to unsigned,
//         (i.e. the well-defined official behaviour).  Behaviour will therefore be unchanged.
//********************************************************************************************

#include "stdafx.h"

#include "pattern.h"
#include "core.h"
#include "custom.h"
#include "except.h"
#include "dataconv.h"
#include "charconv.h"
#include "msg_util.h"

namespace dpt { namespace util {

//********************************************************************************************
std::string PattErrorString(std::string pattern, int pos, std::string details)
{
	return std::string("Error in pattern '")
		.append(pattern)
		.append("', ")
		.append("at or near position ")
		.append(util::IntToString(pos+1)) //convert from zero-based for readability
		.append(": ")
		.append(details);
}

//********************************************************************************************
//Helpers to construct result objects, each with a number of bits rounded up to the nearest
//multiple of 32.
//********************************************************************************************
BitMap ZerothBit(unsigned int sz) {
	int overhang = sz % 32;
	size_t rounded_size = (overhang) ? sz + 32 - overhang : sz;
	return BitMap(rounded_size).Set(0);
}
BitMap OnethBit(unsigned int sz) {
	int overhang = sz % 32;
	size_t rounded_size = (overhang) ? sz + 32 - overhang : sz;
	return BitMap(rounded_size).Set(1);
}
BitMap AllBits(unsigned int sz) {
	int overhang = sz % 32;
	size_t rounded_size = (overhang) ? sz + 32 - overhang : sz;
	return BitMap(rounded_size).SetAll();
}
BitMap NoBits(unsigned int sz) {
	int overhang = sz % 32;
	size_t rounded_size = (overhang) ? sz + 32 - overhang : sz;
	return BitMap(rounded_size);
}
BitMap Empty(unsigned int sz) {
	int overhang = sz % 32;
	size_t rounded_size = (overhang) ? sz + 32 - overhang : sz;
	return BitMap(NULL, rounded_size);
}

//********************************************************************************************
//Most of the meaty stuff happens in this class.
//********************************************************************************************
//Pseudo-constructors used in RepeatedPatternList::Match() to create temporary lists
void SinglePattern::AddComponent(PatternComponent* pc) 
{
	components.push_back(pc);
}
void SinglePattern::Clear() 
{
	components.clear();
}

//********************************************************************************************
//Reassign the fixed_prefix string object, if any, to the owning SinglePatternList
std::string* SinglePattern::HandOverFixedPrefix()
{
	std::string* fp = fixed_prefix;
	fixed_prefix = NULL;
	return fp;
}

//********************************************************************************************
SinglePattern::~SinglePattern()
{
	//Delete all subcomponents
	for (size_t x = 0; x < components.size(); x++)
		if (components[x]) 
			delete components[x];

	if (fixed_prefix) 
		delete fixed_prefix;
}

//********************************************************************************************
//Single character component constructors
//********************************************************************************************
AnyCharacter::AnyCharacter(char c) 
{
	//Pre V5.1 we would disallow certain characters such as full stops here. 
//	if (c == '.') {	//etc
//		std::string s = "Invalid character in pattern: '";
//		s.append(std::string(1,c)).append(1, '\'');
//		throw Exception(s.c_str());
//	}
	thechar = c;
}
//***************************
EscapeCharacter::EscapeCharacter(char c)
{
	//There has never been any validation on these - that's the whole point
	thechar = c;
}
//***************************
HexCharacter::HexCharacter(char c)
{
	//Likewise this can obviously be any character
	thechar = c;
}
//***************************
CharacterRange::CharacterRange(char c1, char c2)
{
	minimum_value = c1;
	maximum_value = c2;
}

//********************************************************************************************
SinglePatternList::~SinglePatternList()
{
//	std::string s;
//	std::list<std::string> ls;
//	ls = Breakdown();

//	std::list<std::string>::const_iterator lsci;
//	for (lsci = ls.begin(); lsci != ls.end(); lsci++) {
//		s.append((*lsci).c_str()).append("\r\n");
//	}
//	MessageBox(AfxGetApp()->m_pMainWnd->m_hWnd, s.c_str(), 0, 0);

	//delete all simple patterns in the list
	for (size_t x = 0; x < thelist.size(); x++)
		if (thelist[x])
			delete thelist[x];

	if (fixed_prefix) 
		delete fixed_prefix;
}

//********************************************************************************************
RepeatedPatternList::~RepeatedPatternList()
{
	if (thelist) delete thelist;
}












//********************************************************************************************
//Parsing functions.  (With the single character things, the constructor is also the 
//parser, since no elaborate control is required).
//********************************************************************************************
int SinglePattern::Parse(const std::string& pattstring, int pattoffset, bool toplevel)
{
	//At the top level we can cater for a common special case, of where the user 
	//enters patterns such as XX*.  In these cases not only can matching be made 
	//much faster by using simple string compares, but it is possible to assist a
	//database search by initially narrowing the range of btree leaves under consideration.
	std::string prefix;
	bool prefix_still_fixed = true;
	
	//Move along the pattern string
	for (;;) {
		//At the top level we can go right up to the end of the string with no terminator
		if ( (size_t)pattoffset == pattstring.length()) {
			if (toplevel) 
				break;
			else 
				throw Exception(UTIL_BAD_PATTERN, 
					PattErrorString(pattstring, pattoffset, 
					"Unexpected end-of-pattern in list member"));
		}

		char c = pattstring[pattoffset];
		//Terminators:
		if (c == ',' || c == ')') 
			break;

		//Create a new component based on the next character.
		if (c == '+') {
			components.push_back(new Placeholder); 
			prefix_still_fixed = false;
		}
		else if (c == '@') {
			components.push_back(new AlphaPlaceholder); 
			prefix_still_fixed = false;
		}
		else if (c == '#') {
			components.push_back(new NumericPlaceholder); 
			prefix_still_fixed = false;
		}
		else if (c == '*') {
			components.push_back(new Wildcard); 
			prefix_still_fixed = false;
		}
		
		//Now it gets interesting - this stuff is recursive
		//A non-repeated list
		else if (c == '(') {
			SinglePatternList* spl = new SinglePatternList;
			components.push_back(spl);
			pattoffset = spl->Parse(pattstring, pattoffset);
			prefix_still_fixed = false;
		}

		//A repeated list
		else if (c == '/') {
			RepeatedPatternList* rpl = new RepeatedPatternList;
			components.push_back(rpl);
			pattoffset = rpl->Parse(pattstring, pattoffset);
			prefix_still_fixed = false;
		}
		
		//Finally we have literal characters.
		else if (c == '-') {
			throw Exception(UTIL_BAD_PATTERN, 
				PattErrorString(pattstring, pattoffset, 
				"Stand-alone range character"));
		}
		else {
			//First get the first character in the (possible) range, bearing in mind ! and =
			char firstchar;
			enum {escape, hex, normal} chartype;
			
			if (c == '!') {
				chartype = escape;
				pattoffset++;
				if ((size_t)pattoffset == pattstring.length()) {
					throw Exception(UTIL_BAD_PATTERN, 
						PattErrorString(pattstring, pattoffset, 
						"Unexpected end-of-pattern in escape character"));
				}
				firstchar = pattstring[pattoffset];
			}
			else if (c == '=') {
				chartype = hex;
				pattoffset +=2;
				if ((size_t)pattoffset >= pattstring.length()) {
					throw Exception(UTIL_BAD_PATTERN, 
						PattErrorString(pattstring, pattoffset, 
						"Unexpected end-of-pattern in hex character"));
				}
				std::string s;

				try {
					s = util::HexStringToAsciiString
						(std::string(pattstring, pattoffset - 1, 2));
				}
				catch (Exception& e) {
					throw Exception(UTIL_BAD_PATTERN, 
						PattErrorString(pattstring, pattoffset, 
						e.What().c_str()));
				}

				firstchar = s[0];
			}
			else {
				chartype = normal;
				firstchar = pattstring[pattoffset];
			}

			//Peek to see if we have a range character (-) coming up next, and also
			//we can forget ranges if this is the last character in the pattern anyway.
			bool range(true);
			if ((size_t)(pattoffset + 1) == pattstring.length()) 	//this is end of string
				range = false;
			if (range) 
				if (pattstring[pattoffset + 1] != '-') 
					range = false;

			if (!range) {
				if (chartype == escape) 
					components.push_back(new EscapeCharacter(firstchar));
				else if (chartype == hex) 
					components.push_back(new HexCharacter(firstchar));
				else 
					components.push_back(new AnyCharacter(firstchar));

				//All these types are valid as part of a fixed pattern prefix
				if (prefix_still_fixed) 
					prefix.append(1, firstchar);
			}
			else {
				//Ranges must be the only members in a particular SinglePattern for some reason
				if (components.size() > 0) {
					throw Exception(UTIL_BAD_PATTERN, 
						PattErrorString(pattstring, pattoffset+1, 
						"Character range must be complete list member"));
				}
				//Also ranges are illegal at the top level - who knows why...
				if (toplevel) {
					throw Exception(UTIL_BAD_PATTERN, 
						PattErrorString(pattstring, pattoffset+1, 
						"Character ranges are not allowed at the outer level"));
				}
			
				//Now move along to the second character (if there is one...)
				pattoffset += 2; //past the hyphen too
				if ((size_t)pattoffset == pattstring.length()) {
					throw Exception(UTIL_BAD_PATTERN, 
						PattErrorString(pattstring, pattoffset, 
						"Unexpected end-of-pattern in character range"));
				}

				//Get the second character.  Again it may be a ! or a =
				c = pattstring[pattoffset];
				char secondchar;

				if (strchr(",()+*-/@#", c)) {
//				if (c == ',' || c == '(' || c == ')' || c == '+' || c == '*' 
//					|| c == '-' || c == '/' || c == '@' || c == '#') {
					throw Exception(UTIL_BAD_PATTERN, 
						PattErrorString(pattstring, pattoffset, 
						"Special character may not terminate range"));
				}
				else if (c == '!') {
					pattoffset++;
					if ((size_t)pattoffset == pattstring.length()) {
						throw Exception(UTIL_BAD_PATTERN, 
							PattErrorString(pattstring, pattoffset, 
							"Unexpected end-of-pattern in escape character"));
					}
					secondchar = pattstring[pattoffset];
				}
				else if (c == '=') {
					pattoffset +=2;
					if ((size_t)pattoffset >= pattstring.length()) {
						throw Exception(UTIL_BAD_PATTERN, 
							PattErrorString(pattstring, pattoffset, 
							"Unexpected end-of-pattern in hex character"));
					}
					std::string s;

					try {
						s = util::HexStringToAsciiString
							(std::string(pattstring, pattoffset - 1, 2));
					}
					catch (Exception& e) {
						throw Exception(UTIL_BAD_PATTERN, 
							PattErrorString(pattstring, pattoffset, 
							e.What().c_str()));
					}
					secondchar = s[0];
				}
				else {
					secondchar = pattstring[pattoffset];
				}

				//Peek ahead again to check that the next character is the end of the list 
				//member, to fulfil the same strange requirement as above.
				if ((size_t)(pattoffset + 1) == pattstring.length()) {
					throw Exception(UTIL_BAD_PATTERN, 
						PattErrorString(pattstring, pattoffset+1, 
						"Unexpected end-of-pattern in list member"));
				}
				if (pattstring[pattoffset+1] != ')' && pattstring[pattoffset+1] != ',') {
					throw Exception(UTIL_BAD_PATTERN, 
						PattErrorString(pattstring, pattoffset+1, 
						"Character range must be complete list member"));
				}

				//Finally is it a realistic range? (need unsigned values for this compare)
				unsigned char uc1 = firstchar;
				unsigned char uc2 = secondchar;
				if (uc1 >= uc2) {
					throw Exception(UTIL_BAD_PATTERN, 
						PattErrorString(pattstring, pattoffset, 
						"Character range is invalid"));
				}

				components.push_back(new CharacterRange(firstchar, secondchar));
				prefix_still_fixed = false;
			}
		}

		//Move on to the next character and start again from the top...
		pattoffset++;
	}

	//What about that prefix?
	if (prefix.length() > 0 && toplevel) 
		fixed_prefix = new std::string(prefix);

	return pattoffset;
}

//********************************************************************************************
int SinglePatternList::Parse(const std::string& pattstring, int pattoffset, bool toplevel)
{
	//Parameter pattoffset should point to the bracket.  If not it means that this is 
	//the very top level at which the list does not have brackets.  The parm will also 
	//therefore control whether end-of-string is accepted as a terminator later on.
	if (!toplevel)
		//move past the left bracket
		pattoffset++;

	//Now create and parse the individual members
	bool firstmember = true;
	for (;;) {

		//At the top level we go right up to the end, as there will be no right bracket
		if ((size_t)pattoffset == pattstring.length()) {
			if (toplevel) 
				break;
			else 
				throw Exception(UTIL_BAD_PATTERN, 
					PattErrorString(pattstring, pattoffset, 
					"Unexpected end-of-pattern in list"));
		}

		char c = pattstring[pattoffset];

		//Terminator: (the right bracket is obviously not required at the top level)
		if (c == ')') { 
			if (toplevel) 
				throw Exception(UTIL_BAD_PATTERN, 
					PattErrorString(pattstring, pattoffset, 
					"Too many closing right brackets"));
			else 
				break;
		}
		
		//On the second+ time round, we are here because a comma terminated the previous 
		//member, so skip it now
		if (firstmember) 
			firstmember = false;
		else 
			pattoffset++;

		//Parse new member and move parsing pointer to the character which terminated it.
		SinglePattern* sp = new SinglePattern;
		thelist.push_back(sp);
		pattoffset = sp->Parse(pattstring, pattoffset, toplevel);
	}

	//Copy up the prefix value if there was one (see SinglePattern::Parse for details).
	//Note that the special case doesn't apply if there was >1 list member.
	if (toplevel && thelist.size() == 1)
		fixed_prefix = thelist[0]->HandOverFixedPrefix();

	return pattoffset;
}

//********************************************************************************************
//These are the things of the form /n(...) or /n-m(...)
//********************************************************************************************
int RepeatedPatternList::Parse(const std::string& pattstring, int pattoffset)
{
	//First get the count component (everything before the left bracket)
	size_t bracketpos = pattstring.find('(',pattoffset);
	if (bracketpos == std::string::npos) {
		throw Exception(UTIL_BAD_PATTERN, 
			PattErrorString(pattstring, pattoffset, 
			"Pattern repeat character (/) but no list follows"));
	}

	//Now work out what the count/counts is/are
	std::string minstr;
	std::string maxstr;
	bool range;

	size_t hyphenpos = pattstring.find('-', pattoffset);

	if (hyphenpos == std::string::npos || hyphenpos > bracketpos) {
		//There is only one number
		range = false;
		minstr = pattstring.substr(pattoffset + 1, bracketpos - (pattoffset + 1));
		maxstr = minstr;
	}
	else {
		//There are two numbers
		range = true;
		minstr = pattstring.substr(pattoffset + 1, hyphenpos - (pattoffset + 1));
		maxstr = pattstring.substr(hyphenpos + 1, bracketpos - (hyphenpos + 1));
	}

	//Do some tests on the number or numbers
	std::string s = "Repeated pattern count invalid ";

	//Null counts are invalid on M204 (e.g. /(A-B))
	if (minstr.length() == 0) throw Exception(UTIL_BAD_PATTERN, 
		PattErrorString(pattstring, pattoffset, s.append("(minimum was not given)")));
	if (maxstr.length() == 0) throw Exception(UTIL_BAD_PATTERN, 
		PattErrorString(pattstring, pattoffset, s.append("(maximum was not given)")));
	
	//Use strtoul because then we can distinguish between 0 and an error (since 0 is valid)
	char* terminator;
	minimum_count = strtoul(minstr.c_str(), &terminator, 10);
	if (*terminator) throw Exception(UTIL_BAD_PATTERN, 
		PattErrorString(pattstring, pattoffset+1, s.append("(minimum is an invalid number)")));

	maximum_count = strtoul(maxstr.c_str(), &terminator, 10);
	if (*terminator) throw Exception(UTIL_BAD_PATTERN, 
		PattErrorString(pattstring, hyphenpos+1, s.append("(maximum is an invalid number)")));

	if (minimum_count > PATRPTMAX) throw Exception(UTIL_BAD_PATTERN, 
		PattErrorString(pattstring, pattoffset+1, s.append("(minimum exceeds limit)")));
	//This one will never happen as a negative sign will be taken as the range character!
	if (minimum_count < 0) throw Exception(UTIL_BAD_PATTERN, 
		PattErrorString(pattstring, pattoffset+1, s.append("(minimum is negative)")));
	if (maximum_count > PATRPTMAX) throw Exception(UTIL_BAD_PATTERN, 
		PattErrorString(pattstring, hyphenpos+1, s.append("(maximum exceeds limit)")));
	if (maximum_count < 0) throw Exception(UTIL_BAD_PATTERN, 
		PattErrorString(pattstring, hyphenpos+1, s.append("(maximum is negative)")));
	if (range && maximum_count <= minimum_count) throw Exception(UTIL_BAD_PATTERN, 
		PattErrorString(pattstring, hyphenpos+1, s.append("(maximum must exceed minimum)")));

	//Well, the counts look OK, so now let's parse the list component itself
	thelist = new SinglePatternList;
	return thelist->Parse(pattstring, bracketpos);
}












//********************************************************************************************
//Matching functions
//********************************************************************************************
//This is the most important matching function, in that it is the only place that any 
//systematic character-by-character scanning of strings takes place.  The manner of
//proceeding along the components in a pattern is to make a recursive call each time to
//scan the next component position.  The reason for this is that the results have to be 
//amalgamated.  Therefore one of the parameters is the component # (within this SinglePattern)
//********************************************************************************************
BitMap SinglePattern::Match(const std::string& matchstring, int stroffset, int pattpos)
{
	//First Match this component.  There may not be one at all, since even though this function
	//will not call itself recursively if there are no more components (see below), it *will*
	//be called by SinglePatternList::Match even if it is empty.
	//So this test can only succeed when pattpos is zero, in which case it's a match, 
	//since the null pattern is welcome within any string at any point!
	if ((size_t)pattpos == components.size())
		return ZerothBit(matchstring.length()+1);

	BitMap this_result(components[pattpos]->Match(matchstring, stroffset));

	//If there's no match for this component, or if no more components anyway, we're done.
	if (!this_result.Any() || (size_t)pattpos == components.size() - 1)
		return this_result;

	//OK, now modify the result of the match(es) for this component by moving along and 
	//incorporating any match(es) for the next component.  Start off clear.
	BitMap recursed_result(NoBits(matchstring.length()+1));

	//Only go along as far as there are further characters in the matchstring.  Therefore if
	//there are no more, and yet there *are* more pattern components, the match fails, since
	//recursed_result will remain empty (but also see comment later).
	for (size_t x = 0; x <= (matchstring.length() - stroffset); x++) {
		if (this_result.Test(x)) {

			//*Note*
			//There is scope for some heuristic work here to eliminate certain possibilities
			//without recursing further.  For example if there are 3 characters remaining in
			//the match string, and only fixed width pattern components, not equal to 3 in 
			//number, there is no point continuing.  This would involve a little extra work
			//during the pattern parsing stage.
			//Note that the comment at the top of this loop was a slight simplification, since
			//even if there are no more matchstring characters, if the remaining components 
			//all match to zero characters, that's OK (important example: "ABC*").  For that 
			//case we use a dummy string to avoid looking off the end of the real matchstring.
			BitMap next_result(Empty(matchstring.length()+1));

			if (x == matchstring.length() - stroffset) {
				next_result = Match(pattdummy, 0, pattpos + 1);
				//We must only use the zero bit here
				bool zb = next_result.Test(0);
				next_result.ResetAll();
				next_result.Set(0, zb);
			}
			else {
				next_result = Match(matchstring, stroffset + x, pattpos + 1);
			}

			//Shift any results left by the appropriate amount.  (e.g. a match of length 3
			//for the next component after a match of length 2 for this component means
			//a match of length 5 for the pair taken together).
			next_result <<= x;
			//Incorporate into the total result for this component position
			recursed_result |= next_result;
		}
	}

	return recursed_result;
}

//********************************************************************************************
//Single character match tests - fairly straightforward
//********************************************************************************************
BitMap AnyCharacter::Match(const std::string& matchstring, int stroffset)
{
	if (matchstring[stroffset] == thechar) return OnethBit(matchstring.length()+1); 
	else return NoBits(matchstring.length()+1);
}
//***************************
BitMap EscapeCharacter::Match(const std::string& matchstring, int stroffset)
{
	if (matchstring[stroffset] == thechar) return OnethBit(matchstring.length()+1); 
	else return NoBits(matchstring.length()+1);
}
//***************************
BitMap HexCharacter::Match(const std::string& matchstring, int stroffset)
{
	if (matchstring[stroffset] == thechar) return OnethBit(matchstring.length()+1); 
	else return NoBits(matchstring.length()+1);
}
//***************************
BitMap Placeholder::Match(const std::string& matchstring, int stroffset) 
{
	return OnethBit(matchstring.length()+1);
}
//***************************
BitMap AlphaPlaceholder::Match(const std::string& matchstring, int stroffset) 
{
	if (isalpha(matchstring[stroffset])) return OnethBit(matchstring.length()+1);
	else return NoBits(matchstring.length()+1);
}
//***************************
BitMap NumericPlaceholder::Match(const std::string& matchstring, int stroffset) 
{
	if (isdigit(matchstring[stroffset])) return OnethBit(matchstring.length()+1);
	else return NoBits(matchstring.length()+1);
}
//***************************
BitMap CharacterRange::Match(const std::string& matchstring, int stroffset) 
{
	//Remember to get an insigned value here
	unsigned char c = matchstring[stroffset];
	if (minimum_value <= c && c <= maximum_value) return OnethBit(matchstring.length()+1);
	else return NoBits(matchstring.length()+1);
}

//********************************************************************************************
//Variable-number-of-characters components match tests
//********************************************************************************************
//Wildcards are simple - all substrings are matches, including zero characters
BitMap Wildcard::Match(const std::string& matchstring, int stroffset) 
{
	return AllBits(matchstring.length()+1);
}

//********************************************************************************************
//Testing all the sub-patterns a list contains and amalgamate the results.
//********************************************************************************************
BitMap SinglePatternList::Match(const std::string& matchstring, int stroffset)
{
	//Empty lists match perfectly to strings zero bytes long!
	if (thelist.empty()) 
		return ZerothBit(matchstring.length()+1);

	//At the top level there are a couple of implications.
	bool toplevel(false);
	if (stroffset == TOPLEVEL) {
		toplevel = true;
		stroffset = 0;

		//Remember that prefix value we stored when parsing?  It can save work now.
		if (fixed_prefix) {
			unsigned int preflen = fixed_prefix->length();
			if (preflen > matchstring.length()) 
				return NoBits(matchstring.length()+1);

			//Rule the string out if it doesn't match the fixed prefix.
			//* * * Note: 
			//The fixed prefix is present for strings that may have any amount
			//of funny stuff after it, *NOT* just for wildcards, even though that
			//would be common.  Therefore we can only rule strings out, and not do 
			//a positive test here, although testing the wildcard will be straightforward.
			if (memcmp(matchstring.c_str(), fixed_prefix->c_str(), preflen) != 0) 
				return NoBits(matchstring.length()+1);
		}
	}

	//Match all the list members against the string at this point and amalgamate results
	BitMap result(NoBits(matchstring.length()+1));
	for (size_t x = 0; x < thelist.size(); x++) {

		//OR here reflects the fact that any of the list members can be used
		result |= thelist[x]->Match(matchstring, stroffset);
	
		//When processing the top level, we may as well quit as soon as one of a list matches
		if (toplevel) {
			if (result.Test(matchstring.length())) {
				return result;
			}
		}

		//In any case there is no need to carry on if the last SinglePattern was a valid
		//match for every substring of the string.  Here we test for the common case of a
		//stand-alone wildcard, which returns all ones.  We will miss cases where every 
		//substing of the match string was satisfied by a list member in some other way
		//e.g. (A,AB,ABC), but this is unlikely, and will still work fine after a bit more
		//processing.
		if (result.All())
			return result;
	}	
	
	return result;
}

//********************************************************************************************
//This is achieved by expanding a pattern into all the possible multiples of itself, so
//e.g. "/2(A++)" is treated exactly as if it were "(A++)(A++)".
//With a variable number of occurrences, e.g. "/1-3(A,B)" is treated exactly as if it were
//"((A,B),(A,B)(A,B),(A,B)(A,B)(A,B))".  Clearly this means that variable-occurrence lists
//are not going to be the quickest things to process!
//********************************************************************************************
BitMap RepeatedPatternList::Match(const std::string& matchstring, int stroffset)
{
	BitMap result(NoBits(matchstring.length()+1));

	for (int x = minimum_count; x <= maximum_count; x++) {

		//Create a temporary SinglePattern
		SinglePattern sp;

		//Add x copies of the repeated list to it
		for (int y = 0; y < x; y++) {
			sp.AddComponent(thelist);
		}

		//Now just match to the temporary
		result |= sp.Match(matchstring, stroffset);

		//Clear temporary before it goes out of scope, else it'll delete the components too
		sp.Clear();
	}

	return result;
}

//********************************************************************************************
//The user interface class
//********************************************************************************************
Pattern::Pattern() 
	: thepattern(NULL), simple_leading_wildcard(false), 
		no_case_option(false) {}

Pattern::Pattern(const std::string& p) 
	: thepattern(NULL), simple_leading_wildcard(false), 
		no_case_option(false) {SetPattern(p);}

Pattern::Pattern(const char* p) 
	: thepattern(NULL), simple_leading_wildcard(false), 
		no_case_option(false) {SetPattern(p);}

Pattern::~Pattern() {if (thepattern) delete thepattern;}

//********************************************************************************************
void Pattern::SetPattern(const std::string& pattern_string_input)
{
	//Clear out previous pattern
	if (thepattern) {
		delete thepattern;	
		thepattern = NULL;
	}

	const std::string* p = &pattern_string_input;
	int plen = p->length();

	//V3.02. Case-insensitive searches.
	no_case_option = false;
	std::string p_nocase;
	if (plen >= 7) {
		char buff[8];
		*((_int64*)buff) = *((_int64*)p->c_str()); //lol 8 byte copy :)
		buff[7] = 0;
		if (_stricmp(buff, "NOCASE:") == 0) {      //keyword allowed in mixed case

#ifdef _BBDBAPI
			if (CoreServices::CstFlags() & DISABLE_BIT_UL_STATEMENTS) {
				throw Exception(UTIL_BAD_PATTERN, 
					PattErrorString(*p, 0, "The NOCASE pattern option is "
						"currently disabled by the CSTFLAGS parameter setting"));
			}
#endif
			no_case_option = true;
			p_nocase = *p;
			p_nocase = p_nocase.substr(7);
			util::ToUpper(p_nocase);               //uppercase the pattern
			p = &p_nocase;
			plen -= 7;
		}
	}

	//V3.0.  See comment in header file.  Since these are going to be tuff to evaluate anyway
	//it's worth taking a little extra hit to catch a very common case.
	simple_leading_wildcard = false;
	if (plen > 1 && (*p)[0] == '*') {

		simple_leading_wildcard = true;
		int scanend = plen-1;
		simple_leading_wildcard_also_trailing_wildcard = false;

		if ((*p)[plen-1] == '*') {
			simple_leading_wildcard_also_trailing_wildcard = true;
			scanend--;
		}

		//Search for any other pattern characters in between
		for (int x = 1; x <= scanend; x++) {
			if (strchr(",()+*-/@#!=", (*p)[x])) {
				simple_leading_wildcard = false;
				break;
			}
		}

		if (simple_leading_wildcard) {
			simple_leading_wildcard_fixedpart = p->substr(1, scanend);
			return;
		}
	}

	//Parse the new one.  All the parsing functions throw exceptions as an easy way 
	//of exiting quickly to here - they're recursive.
	try {
		thepattern = new SinglePatternList();
		thepattern->Parse(*p, 0, true);
	}
	catch (dpt::Exception e) {
		if (thepattern) {delete thepattern;	thepattern = NULL;}
		throw e;
	}
}

//***************************
//V2 Jan 07.
void Pattern::SetPatternSiriusStyle(const std::string& p)
{
	std::string s(p);
	ConvertFromSiriusStyle(s);
	SetPattern(s);
}

//***************************
void Pattern::ConvertFromSiriusStyle(std::string& s)
{
	for (size_t i = 0; i < s.length(); i++) {
		char c = s[i];

		//Placeholder
		if (c == '?') {
			if (c == 0 || s[i-1] != '!')	//except if escaped
				s[i] = '+';
		}

		//Escape character
		else if (c == '"') {
			if (c == 0 || s[i-1] != '!')	//except if escaped
				s[i] = '!';
		}
	}
}

//********************************************************************************************
//Note that the name of this function is "back-to-front" in that you might expect it
//to read more nicely as "matchstring IsLike(patternstring)".  It doesn't.
//********************************************************************************************
bool Pattern::IsLike(const std::string& candidate_string_input) const
{
	const std::string* s = &candidate_string_input;

	//V3.02. See SetPattern() above
	std::string s_nocase;
	if (no_case_option) {
		s_nocase = *s;
		util::ToUpper(s_nocase);
		s = &s_nocase;
	}

	//V3.0. See SetPattern() above
	if (simple_leading_wildcard) {

		int lendiff = s->length() - simple_leading_wildcard_fixedpart.length();
		if (lendiff < 0)
			return false;

		//Scan whole string or only match against one section at the end
		int scanpos = (simple_leading_wildcard_also_trailing_wildcard) ? 0 : lendiff;

		return (s->find(simple_leading_wildcard_fixedpart, scanpos) != std::string::npos);
	}

	if (!thepattern) 
		return false;

	//If any sub-string, the full length of the match string, is successfully matched, 
	//it means success
	if (thepattern->Match(*s, TOPLEVEL).Test(s->length())) 
		return true;
	else 
		return false;
}

}}


















#ifdef _DEBUG
//********************************************************************************************
//These functions are here to help with debugging only
//********************************************************************************************
namespace dpt { namespace util {

//assume no multithreading for debug!
int tabstop = 0;

std::string Tab() {
	return std::string(tabstop * 4, '-');
}

//**************************************
std::list<std::string> SinglePattern::Breakdown() {
	tabstop++;

	std::list<std::string> v;
	v.push_back(Tab().append("SinglePattern "));

	if (components.empty()) {
		tabstop++;
		v.push_back(Tab().append("(Empty)"));
		tabstop--;
	}
	else {
		for (int x = 0; x < components.size(); x++) {
			v.splice(v.end(),(components[x]->Breakdown()));
		}
	}

	tabstop--;
	return v;
}

//**************************************
std::list<std::string> SinglePatternList::Breakdown() {
	tabstop++;

	std::list<std::string> v;
	v.push_back(Tab().append("SinglePatternList "));

	if (thelist.empty()) {
		tabstop++;
		v.push_back(Tab().append("(Empty)"));
		tabstop--;
	}
	else {
		if (fixed_prefix) {
			tabstop++;
			v.push_back(Tab().append("Top level fixed prefix: ").append(*fixed_prefix));
			tabstop--;
		}
		for (int x = 0; x < thelist.size(); x++) {
			v.splice(v.end(),(thelist[x]->Breakdown()));
		}
	}

	tabstop--;
	return v;
}

//**************************************
std::list<std::string> RepeatedPatternList::Breakdown() {
	tabstop++;

	std::list<std::string> v;
	v.push_back(Tab()
		.append("RepeatedPatternList ")
		.append(util::IntToString(minimum_count))
		.append(" - ")
		.append(util::IntToString(maximum_count)));

	v.splice(v.end(),thelist->Breakdown());

	tabstop--;
	return v;
}

//**************************************
std::list<std::string> AnyCharacter::Breakdown() {
	tabstop++;
	
	std::list<std::string> v;
	v.push_back(Tab().append("AnyCharacter ('").append(std::string(1,thechar)).append("')"));
	
	tabstop--;
	return v;
}

//**************************************
std::list<std::string> EscapeCharacter::Breakdown() {
	tabstop++;
	
	std::list<std::string> v;
	char buff[64];
	unsigned char uc = thechar;
	sprintf(buff, "('%c' / ASCII %d / 0x%X)", thechar, uc, uc);
	v.push_back(Tab().append("EscapeCharacter ").append(buff));
	
	tabstop--;
	return v;
}

//**************************************
std::list<std::string> HexCharacter::Breakdown() {
	tabstop++;
	
	std::list<std::string> v;
	char buff[32];
	unsigned char uc = thechar;
	sprintf(buff, "('%c' / ASCII %d / 0x%X)", thechar, uc, uc);
	v.push_back(Tab().append("HexCharacter ").append(buff));
	
	tabstop--;
	return v;
}

//**************************************
std::list<std::string> Placeholder::Breakdown() {
	tabstop++;
	
	std::list<std::string> v;
	v.push_back(Tab().append("Placeholder"));
	
	tabstop--;
	return v;
}

//**************************************
std::list<std::string> AlphaPlaceholder::Breakdown() {
	tabstop++;
	
	std::list<std::string> v;
	v.push_back(Tab().append("AlphaPlaceholder"));
	
	tabstop--;
	return v;
}

//**************************************
std::list<std::string> NumericPlaceholder::Breakdown() {
	tabstop++;
	
	std::list<std::string> v;
	v.push_back(Tab().append("NumericPlaceholder"));
	
	tabstop--;
	return v;
}

//**************************************
std::list<std::string> Wildcard::Breakdown() {
	tabstop++;
	
	std::list<std::string> v;
	v.push_back(Tab().append("Wildcard"));
	
	tabstop--;
	return v;
}

//**************************************
std::list<std::string> CharacterRange::Breakdown() {
	tabstop++;
	
	std::list<std::string> v;
	char buff[32];
	sprintf(buff, "'%c'-'%c' (ASCII %d-%d)", 
		minimum_value,
		maximum_value,
		minimum_value, 
		maximum_value
		);
	v.push_back(Tab().append("CharacterRange, ").append(buff));
	
	tabstop--;
	return v;
}

std::list<std::string> Pattern::Breakdown()
{
	if (simple_leading_wildcard) {
		std::list<std::string> v;
		v.push_back("Simple leading wildcard match:");
		v.push_back(std::string("  *")
			.append(simple_leading_wildcard_fixedpart)
			.append(simple_leading_wildcard_also_trailing_wildcard ? "*" : ""));
		return v;
	}
	else {
		tabstop = -1;
		return thepattern->Breakdown();
	}
}

}}


#endif
