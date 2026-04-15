
#include "stdafx.h"

#include "parsing.h"
#include "dataconv.h"
#include "charconv.h"
#include "except.h"
#include "msg_util.h"
#include "string.h"
#include "assert.h"

namespace dpt { namespace util {

//******************************
bool IsAlpha(const std::string& s) 
{
	const char* p = s.c_str();
	const char* end = p + s.length();

	for (; p != end; p++) {
		if (!isalpha(*p))
			return false;
	}

	return true;
}

//***********************************************************************************
//NB. When we "go off the end" of the string being parsed we set the cursor to 
//npos rather than the terminating zero character.  Either way would have been fine
//I guess - too late to change now.
//***********************************************************************************
std::string	GetWord(const std::string& s, const int required, const char delim, 
					const int startpos, int* movecursor, int* inwp)
{

//* * NB. * * Changes to this function are candidates for the multi-separator version below.

	int dummy;
	int* word_pos = &dummy;
	if (inwp) {
		word_pos = inwp;
		*word_pos = std::string::npos;
	}

	if (required < 1) 
		return std::string();

	if ((size_t)startpos >= s.length() || startpos < 0) {
		if (movecursor) 
			*movecursor = std::string::npos;
		return std::string();
	}

	int count = 0;

	*word_pos = s.find_first_not_of(delim, startpos);		//start of first word

	for (;;) {
		if ((size_t)(*word_pos) == std::string::npos) {				//not enough words
			if (movecursor)	
				*movecursor = std::string::npos;
			return std::string();
		}

		count++;
		int delim_pos = s.find(delim, *word_pos + 1);		//next delimiter

		if ((size_t)delim_pos == std::string::npos) {				//last word and no trailing spaces
			if (movecursor) 
				*movecursor = std::string::npos;

			if (count == required)
				return s.substr(*word_pos);					//last word was the one required
			else
				return std::string();						//not enough words
		}

		if (count == required) {							//found required word
			if (movecursor) 
				*movecursor = delim_pos;
			return s.substr(*word_pos, delim_pos - *word_pos);
		}

		*word_pos = s.find_first_not_of(delim, delim_pos);	//Locate next word
	}
}

//***********************************************************************************
//V2.211. Jan 08.
std::string	GetWord(const std::string& s, const int required, const std::string& delims, 
					const int startpos, int* movecursor, int* inwp)
{

//* * NB. * * Exactly the same as the single-separator version above

	int dummy;
	int* word_pos = &dummy;
	if (inwp) {
		word_pos = inwp;
		*word_pos = std::string::npos;
	}

	if (required < 1) 
		return std::string();

	if ((size_t)startpos >= s.length() || startpos < 0) {
		if (movecursor) 
			*movecursor = std::string::npos;
		return std::string();
	}

	int count = 0;

	*word_pos = s.find_first_not_of(delims, startpos);		//start of first word

	for (;;) {
		if ((size_t)(*word_pos) == std::string::npos) {				//not enough words
			if (movecursor)	
				*movecursor = std::string::npos;
			return std::string();
		}

		count++;
		int delim_pos = s.find_first_of(delims, *word_pos + 1);	//next delimiter

		if ((size_t)delim_pos == std::string::npos) {				//last word and no trailing spaces
			if (movecursor) 
				*movecursor = std::string::npos;

			if (count == required)
				return s.substr(*word_pos);					//last word was the one required
			else
				return std::string();						//not enough words
		}

		if (count == required) {							//found required word
			if (movecursor) 
				*movecursor = delim_pos;
			return s.substr(*word_pos, delim_pos - *word_pos);
		}

		*word_pos = s.find_first_not_of(delims, delim_pos);	//Locate next word
	}
}

//***********************************************************************************
//Shortcuts for these very common cases 
std::string	GetWord(const std::string& s, int* c, int* w)
{
	return GetWord(s, 1, ' ', *c, c, w);
}
std::string	GetWord(const std::string& s, const std::string& d, int* c, int* w)
{
	return GetWord(s, 1, d, *c, c, w);
}


//***********************************************************************************
//Sometimes useful when GetWord() is used for convenience in command parsing, and we
//want to know the position of the next word without actually taking it.
//It works much like Word().
//If there are insufficient words, it returns std::string::npos.
//***********************************************************************************
int PositionOfWord(const std::string& s, const int required, 
				   const char delim, const int startpos)
{
	if (required < 1) 
		return std::string::npos;

	if ((size_t)startpos >= s.length() || startpos < 0)
		return std::string::npos;

	int delim_pos = 0;
	int word_pos = s.find_first_not_of(delim, startpos);	//start of first word
	int found = 1;

	while (found < required && (size_t)word_pos != std::string::npos && (size_t)delim_pos != std::string::npos) {
		delim_pos = s.find(delim, word_pos + 1);			//next delimiter
		word_pos = s.find_first_not_of(delim, delim_pos);	//next word
		found++;
	}

	return word_pos;
}

/***********************************************************************************
Behaviour of this function:
This function locates tokens, separated by those characters specified as delimiters.  
For example, AB,CD;EF returns 3 tokens when comma and semicolon are used as delimiters. 
The return value is the number of characters used from the string.
Optional parameters, in order of anticipated commonness at the time I coded it.
Drop nulls     : This will remove any null tokens from the result, so that in the
                 earlier example, AB,,;CD;EF would still return 3 tokens, not 5.
Quote character: This allows delimiter characters to be enclosed in quotes if
                 required.  e.g. AB','CD,EF could return just 2 tokens with comma
                 as a delimiter, namely 'AB,CD' and 'EF'.  
				 The last token goes right up to the end if quotes are mismatched.
Strip quotes   : Used in conjunction with the above.  It says whether to take the quotes
                 off the tokens returned (since the caller may want to do something with 
				 them).  The above example had the quotes stripped out.
Deblank        : The resulting tokens have leading and trailing blanks removed.
Start position : This allows the scan to begin at a certain place.
Number required: This allows the scan to abort after, say, one token is found.
Count only     : Don't return tokens just count them
Delims as strng: The delims parm is treated as a single string which will be the
                 token delimiter, instead of a selection of single characters, each
				 of which is a valid token delimiter.  E.g. if delims is "\r\n",
				 we can accept any '\r' or '\n', or insist on a full "\r\n".
***********************************************************************************/

//V3.0.  Users are bound to want to use $TOKENIZE on BLOB fields, so I've removed
//the restriction on a maximum 32K input string, and imposed a limit on the built
//token lengths (and only then because the code uses a stack buffer and I don't want
//to fiddle around with it now).  Such long individual tokens are in any case not
//really in the spirit of "tokens", which are generally small things.
#define TOKENIZE_MAX_TOKEN_LENGTH 65536 

static inline void TokenLenCheck(int current_token_length) {
	if (current_token_length == TOKENIZE_MAX_TOKEN_LENGTH)
		throw Exception(BUG_STRING_TOO_LONG, "Tokenize() maximum token length exceeded");
}

int Tokenize
(std::vector<std::string>& result,
 const std::string& thestring, 
 const std::string& delims, 
 const bool drop_nulls,
 const char quote_char, 
 const bool dequote, 
 const bool deblank, 
 const int start_offset,
 const int num_reqd,
 int* count_only,
 bool delims_as_string)
{
//V2 Sep 06
	const char SINGLE_OR_DOUBLE = -1;
	bool delim_is_quote = false;
	if (quote_char == SINGLE_OR_DOUBLE)
		delim_is_quote = (delims.find_first_of("\'\"") != std::string::npos);
	else if (quote_char)
		delim_is_quote = (delims.find(quote_char) != std::string::npos);

	//Clear the results
	if (count_only)
		*count_only = 0;
	else if (result.size() > 0)
		result.clear();

	//Initialize control variables
	int len = thestring.length();
	if (len == 0) 
		return 0;

	//It would screw up the algorithm if the same character was a quote and a delimiter.
	//Using space as a delimiter is OK, rendering deblank inoperative.
	if (delim_is_quote)
		throw Exception(BUG_MISC,
			"Bug: Quote character in Tokenize() can not be a delimiter too");

	//This helps synergise with std::string::npos in a few places
	if (start_offset == -1)
		return -1;
	else if (start_offset >= len || start_offset < -1) 
		throw Exception(BUG_MISC,
			"Bug: Start offset in Tokenize() is outside the string");

	//V3.0. See comments at top.
	//if (len > 32767) 
	//	throw Exception(BUG_STRING_TOO_LONG,
	//		"Input string to Tokenize() exceeds maximum of 32767 characters");

	//Do all the string work with buffers - may as well make a token effort ha ha.
	const char* str_start = thestring.c_str();
	const char* str_end = str_start + len;
	char current_token[TOKENIZE_MAX_TOKEN_LENGTH] = {0};
	int current_token_length = 0;
	int trailing_spaces = 0;
	char in_quoted_string = 0;
	bool repeated_quote = false;

	//The byte-by-byte approach is simplest with this number of control parameters.
	//More efficiency might be gained by coding specific cases, e.g. without quotes etc.,
	for (const char* cursor = str_start + start_offset; cursor < str_end; ++cursor) {

		char c = *cursor;

		//Quotes, if active
		bool is_quote_char = false;

		//V2 Sep 06.  Strings must end with by the same type of quote as they started with.
		if (quote_char) {
			if (in_quoted_string)
				is_quote_char = (c == in_quoted_string);
			else if (quote_char == SINGLE_OR_DOUBLE)
				is_quote_char = (c == '\'' || c == '\"');
			else
				is_quote_char = (c == quote_char);
		}

		if (is_quote_char) {
			if (in_quoted_string) {
				//Repeated quote characters are spotted by peeking ahead.  Such pairs will
				//appear in the token unrepeated, as per standard convention.
				if (!repeated_quote) {                     //might be the first of a pair
					if (cursor < (str_end -1)) {           //only peek if enough chars left
						if (*(cursor+1) == c) {            //peek
							repeated_quote = true;         //it is, so set flag for next one
							continue;
						}
					}
						
					//End of the string we were in
					in_quoted_string = 0;
				}
			}
			else {
				//Start of a new string
				in_quoted_string = c;	
			}

			//If we're dequoting, just ignore the quote.  However, only drop one of a pair
			//of double quotes.  'O''Malley' would come out as O'Malley
			if (dequote && !repeated_quote) 
				continue;
		}

		//Non-quote characters
		else {

			//Delimiters - end of current token
			//Nov 06.  Allow the delimiters now to be treated together (e.g. CRLF).
			bool at_delimiter = false;
			if (delims_as_string) {
				if (cursor <= (str_end - delims.length())) {
					if (!memcmp(cursor, delims.data(), delims.length())) {
						at_delimiter = true;
						cursor += delims.length() - 1;
					}
				}
			}
			else {
				at_delimiter = (strchr(delims.c_str(), c) != NULL); 
			}

			if (at_delimiter && !in_quoted_string) {
				//Drop null tokens if so requested
				if (drop_nulls && current_token_length == 0)
					continue;
				
				//Terminate token and append to the result
				current_token[current_token_length] = 0;

				int num_found;
				if (count_only) {
					(*count_only)++;
					num_found = *count_only;
				}
				else {
					result.push_back(current_token);
					num_found = result.size();
				}

				//Quit if we have enough tokens now
				if (num_found == num_reqd)
					return cursor - str_start + start_offset;

				//We can forget those pending trailing spaces now too
				trailing_spaces = 0;
				current_token_length = 0;
				continue;
			}

			//Spaces.  These are checked separately so we can store them up in case
			//we need to remove them from the end of token because of the deblank option.
			//NB. Only get to here if one of the delimiters wasn't a space.
			else if (c == ' ') {
				if (deblank) {
					//Drop leading spaces.  Save up other spaces in case they're not needed
					if (current_token_length)
						++trailing_spaces;

					continue;
				}
			}
		}

		//------------------------------------------------
		//A (nonblank) character is to be added to the current token.
		//Add on any saved-up spaces first, now we know they're not trailers.
		if (deblank && trailing_spaces) {
			while (trailing_spaces > 0) {

				TokenLenCheck(current_token_length); //V3.0

				current_token[current_token_length] = ' ';
				--trailing_spaces;
				++current_token_length;
			}
		}

		TokenLenCheck(current_token_length); //V3.0

		current_token[current_token_length] = c;
		++current_token_length;
		if (repeated_quote) 
			repeated_quote = false;
	}

	//Scanned the whole string.  Add any last token and return
	if (!drop_nulls || current_token_length != 0) {

		TokenLenCheck(current_token_length); //V3.0

		current_token[current_token_length] = 0;
		if (count_only)
			(*count_only)++;
		else
			result.push_back(current_token);
	}

	return str_end - str_start + start_offset;
}

//*****************************************************************************************
//This function removes the outermost bracketed expression within a line, placing it
//(without the brackets) in a second parameter.  The result is whether an expression
//was found.  (Testing for null in a result string would miss empty expressions).
//It's used to get hold of options like in D FILE (PARMS) in which the brackets are
//allowed anywhere - e.g. D (PARMS) FILE is OK. 
//*****************************************************************************************
bool ParseOutNextBrackets
(std::string& text_inout, std::string& expr_out, int startpos, bool ignore_quoted)
{
tryagain:
	size_t b1 = text_inout.find('(', startpos);
	if (b1 == std::string::npos) return false;

	size_t b2 = text_inout.find(')', b1);
	if (b2 == std::string::npos) return false;

	//DPT V1.1 - in commands like D FIELD there may be bracketed options for the command
	//but the field name might contain brackets which should be "hideable".
	if (ignore_quoted) {
		bool either_quoted = false;
		std::vector<bool>* flags = new std::vector<bool>(text_inout.length(), false);
		try {
			bool inside = false;
			for (size_t x = 0; x <= b2; x++) {
				if (text_inout[x] == '\'')
					inside = !inside;
				(*flags)[x] = inside;
			}

			if ((*flags)[b1] || (*flags)[b2])
				either_quoted = true;

			delete flags;
		}
		catch (...) {
			delete flags;
			throw;
		}

		if (either_quoted) {
			startpos = b2;
			goto tryagain;
		}
	}

	//There is a bracketed expression, so get it, then remove it from the original
	expr_out = text_inout.substr(b1+1, b2-b1-1);

	//Skip this if the two strings were the same
	if (&text_inout != &expr_out)
		text_inout.erase(b1, b2-b1+1);

	return true;
}

//*****************************************************************************************
bool ParseOutNextString
(std::string& text_inout, const std::string& expr_in, const int startpos, int* foundpos)
{
	size_t spos = text_inout.find(expr_in, startpos);
	if (spos == std::string::npos)
		return false;

	text_inout.erase(spos, expr_in.length());

	//Note that the location may be 1 past the end of the string now, hence this bit
	if (foundpos) {
		if (spos == text_inout.length())
			*foundpos = std::string::npos;
		else
			*foundpos = spos;
	}

	return true;
}

//*****************************************************************************************
//Could do this using WORD, but since ONEOF is quite common that would be unfair speedwise.
//*****************************************************************************************
bool OneOf(const std::string& s, const std::string& choices, const char delim)
{
	int delim_pos = -1;
	
	do {
		int choice_start_pos = delim_pos + 1;
		delim_pos = choices.find(delim, choice_start_pos);

		int choice_length;
		if ((size_t)delim_pos == std::string::npos)
			choice_length = choices.length() - choice_start_pos;
		else
			choice_length = delim_pos - choice_start_pos;

		//Use memcmp - saves initializing another std::string.  String compare would
		//just do a similar thing anyway.
		if (s.length() == (size_t)choice_length)
			if (!memcmp(s.c_str(), choices.c_str() + choice_start_pos, choice_length))
				return true;

	} while ((size_t)delim_pos != std::string::npos);

	return false;
}

//*****************************************************************************************
int CountSharedChars(const char* s1, int s1len, const char* s2, int s2len)
{
	//Compare characters till one runs out or there is a difference
	int x;
	for (x = 0; x < s1len && x < s2len; x++)
		if (s1[x] != s2[x])
			break;
	
	return x;
}

//*******************************************************************************************
_int64 GetStorageQuantity(const std::string& sin, bool round)
{
	std::string s = sin;
	UnBlank(s, true);
	ToUpper(s);

	int oneK = (round) ? 1000 : 1024;

	//Trailing b or B is ignored
	if (s.length() > 0 && s[s.length()-1] == 'B')
		s.resize(s.length()-1);

	//Note any multiplier
	_int64 multiplier = 1;
	if (s.length() > 0) {
		switch (s[s.length()-1]) 
		{
			case 'T': multiplier *= oneK;
			case 'G': multiplier *= oneK;
			case 'M': multiplier *= oneK;
			case 'K': multiplier *= oneK;
		}
		if (multiplier != 1)
			s.resize(s.length()-1);
		else if (!isdigit(s[s.length()-1]))
			throw Exception(UTIL_DATACONV_BADFORMAT, "Invalid storage units");
	}

	if (s.length() == 0)
		throw Exception(UTIL_DATACONV_BADFORMAT, "Missing or invalid storage quantity");

	_int64 result = StringToInt64(s) * multiplier;

	if (result < 0)
		throw Exception(UTIL_DATACONV_RANGE, "Negative storage quantity is invalid");

	return result;
}

//*******************************************************************************************
int BufferFind(const char* buff, int bufflen, const char* target, int targetlen)
{
	if (bufflen < 0 || targetlen < 0 || targetlen > bufflen)
		throw Exception(BUG_MISC, "Bug: invalid buffer and/or target length in BufferFind()");

	//A moving test "window"
	const char* last_teststart = buff + bufflen - targetlen;
	for (const char* teststart = buff; teststart <= last_teststart; teststart++) {

		//Test each character in the window
		const char* testend = teststart + targetlen;
		const char* targptr = target;
		const char* buffptr = teststart;
		for (; buffptr != testend; buffptr++, targptr++) {
			if (*buffptr != *targptr)
				break;
		}

		//Matched whole window
		if (buffptr == testend)
			return teststart - buff;
	}

	return -1;
}


}} //close namespace

