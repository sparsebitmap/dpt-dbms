
#if !defined(BB_PARSING)
#define BB_PARSING

#include <string>
#include <vector>

namespace dpt { namespace util {

inline bool IsDigits(const std::string& s) {return (s.find_first_not_of("0123456789") == std::string::npos);}
inline bool IsInteger(const std::string& s) {return (s.find_first_not_of("0123456789-+") == std::string::npos);}
bool IsAlpha(const std::string&);

//Parse out a word from a string, with cursor advancement if required
std::string	GetWord(const std::string&, const int, const char = ' ', 
					const int = 0, int* = NULL, int* = NULL);
std::string GetWord(const std::string&, int*, int* = NULL);
//V2.11 Jan 08.  Added this version to allow variable separator chars (like Tokenize)
std::string	GetWord(const std::string&, const int, const std::string&, 
					const int = 0, int* = NULL, int* = NULL);
std::string GetWord(const std::string&, const std::string&, int*, int* = NULL);

//Where does the nth word start in another word
int PositionOfWord(const std::string&, const int, const char = ' ', const int = 0);

//Perhaps over-generalized function to separate a string into several substrings.
int Tokenize
	(std::vector<std::string>& result,
	const std::string& thestring, 
	const std::string& delims = std::string(1, ' '), 
	const bool drop_nulls = true, 
	const char quote_char = 0, //V2: -1 is special value meaning ' or "
	const bool dequote = false, 
	const bool deblank = true, 
	const int start_offset = 0,
	const int num_reqd = -1,
	int* count_only = NULL, //V2: option to not build the full results
	bool delims_as_string = false); //V2: consider delims together (e.g. CRLF)

//Remove a bracketed expression from another
bool ParseOutNextBrackets(std::string&, std::string&, int = 0, bool = false);

//Remove any string from another
bool ParseOutNextString(std::string&, const std::string&, const int = 0, int* = NULL);

//As it says on the tin
bool OneOf(const std::string&, const std::string&, const char delim = '/');
int CountSharedChars(const char*, int, const char*, int);

//V2.14 Jan 09.
_int64 GetStorageQuantity(const std::string&, bool round=false); //round: 1K = 1000, not 1024

//V3.0
int BufferFind(const char* buff, int bufflen, const char* target, int targetlen); 

}} //close namespace

#endif
