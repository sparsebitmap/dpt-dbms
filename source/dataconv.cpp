
#include "stdafx.h"

#include "dataconv.h"

#include "string.h"
#include <math.h>
#include "float.h"
#include "windows.h"

#include "bbfloat.h"
#include "charconv.h"
#include "except.h"
#include "msg_util.h"

static std::string WHITECHARS = " \t\n\r\f\v";

namespace dpt { namespace util {

//***********************************************************************************
//Integer/string conversions
//***********************************************************************************
int StringToInt(const std::string& s, bool* notify_bad)
{
	if (notify_bad)
		*notify_bad = false;

	bool out_of_range = false;
	int len = s.length();
	if (len > 11) {
		out_of_range = true;
	}
	else if (len == 11) {
		if (s[0] == '-') {
			if (s > "-2147483648")
				out_of_range = true;
		}
		else {
			out_of_range = true;
		}
	}
	else if (len == 10) {
		if (s[0] != '-' && s > "2147483647") 
			out_of_range = true;
	}

	if (out_of_range) {
		if (notify_bad) {
			*notify_bad = true;
			return 0;
		}
		throw Exception(UTIL_DATACONV_RANGE, 
			std::string("Decimal number '")
			.append(s).append("' is too long to be a signed 31 bit value"));
	}

	//V2 Oct 2006
	//Astonishingly I hadn't noticed before now that atoi allows e.g. 123CRAP = 123,
	//so replaced with strtol.

/*
	if (s.find_first_not_of('0') == std::string::npos)
		return 0;

	if (s.find_first_not_of(" \t+-1234567890") != std::string::npos)
		throw Exception(UTIL_DATACONV_BADFORMAT,
			std::string("Invalid character in decimal integer: ").append(s));

	//atoi() does not recognize floating point numbers
	if (s.find_first_of("Ee") != std::string::npos)
		throw Exception(UTIL_DATACONV_BADFORMAT,
			std::string("E-format number is not allowed here: ").append(s));

	int result = atoi(s.c_str());

	//We tested for zero above, so this must be an error
	if (result == 0)
		throw Exception(UTIL_DATACONV_BADFORMAT,
			std::string("Invalid decimal integer: ").append(s));
*/
	char* terminator;
	int result = strtol(s.c_str(), &terminator, 10);
	if (*terminator) {

		//V2.11 Jan 08.  Possibly should repeat the "fix" from StringToPlainDouble()
		//- see later - but this function is used so widely I want to avoid breaking
		//anything that might rely on trailing spaces being invalid.  Something for
		//the back burner.
		//if (... etc - check for trailing spaces only) which would be OK

		if (notify_bad) {
			*notify_bad = true;
			return 0;
		}
		throw Exception(UTIL_DATACONV_BADFORMAT,
			std::string("Invalid decimal integer: ").append(s));
	}

	return result;
}

//***********************************************************************************
std::string IntToString(const int i)
{
	char buff[32];
	sprintf(buff, "%d", i);
	return buff;
}

//***********************************************************************************
std::string VectorOfIntToString(std::vector<int>& v)
{
	std::string result;

	for (size_t x = 0; x < v.size(); x++) {
		if (x > 0)
			result.append(1, '/');
		result.append(IntToString(v[x]));
	}

	return result;
}

//***********************************************************************************
unsigned long StringToUlong(const std::string& s, bool* notify_bad)
{
	if (notify_bad)
		*notify_bad = false;

	bool out_of_range = false;
	int len = s.length();
	if (len > 10) {
		out_of_range = true;
	}
	else if (len == 10) {
		if (s > "4294967295")
			out_of_range = true;
	}

	if (out_of_range) {
		if (notify_bad) {
			*notify_bad = true;
			return 0;
		}
		throw Exception(UTIL_DATACONV_RANGE, 
			std::string("Decimal number '")
			.append(s).append("' is too long to be an unsigned 32 bit value"));
	}

	char c = s[0];
	if (c == ' ') {
		size_t pos = s.find_first_not_of(' ');
		if (pos != std::string::npos) 
			c = s[pos];
	}

	if (c == '-') {
		if (notify_bad) {
			*notify_bad = true;
			return 0;
		}
		throw Exception(UTIL_DATACONV_RANGE, 
			std::string("Negative value '")
			.append(s).append("' can't be used as an unsigned decimal integer"));
	}

	char* terminator;
	unsigned long result = strtoul(s.c_str(), &terminator, 10);
	if (*terminator) {
		if (notify_bad) {
			*notify_bad = true;
			return 0;
		}
		throw Exception(UTIL_DATACONV_BADFORMAT,
			std::string("Invalid decimal integer: ").append(s));
	}

	return result;
}

//***********************************************************************************
//Unsigned int/string conversions
//***********************************************************************************
std::string UlongToString(const unsigned long i)
{
	char buff[32];
	sprintf(buff, "%lu", i);
	return buff;
}

//***********************************************************************************
//64 bit integer/string conversions
//***********************************************************************************
std::string Int64ToString(const _int64& i)
{
	char buff[32];
	sprintf(buff, "%I64d", i);
	return buff;
}

//***********************************************************************************
_int64 StringToInt64(const std::string& s, bool* notify_bad)
{
	if (notify_bad)
		*notify_bad = false;

	//See note above about range checking.
	bool out_of_range = false;
	int len = s.length();
	if (len > 20)
		out_of_range = true;
	else if (len == 20) {
		if (s[0] == '-') {
			if (s > "-9223372036854775808")
				out_of_range = true;
		}
		else {
			out_of_range = true;
		}
	}
	else if (len == 19) {
		if (s[0] != '-' && s > "9223372036854775807") 
			out_of_range = true;
	}

	if (out_of_range) {
		if (notify_bad) {
			*notify_bad = true;
			return 0;
		}
		throw Exception(UTIL_DATACONV_RANGE, 
			std::string("Decimal number '")
			.append(s).append("' is too long to be a signed 63 bit value"));
	}

	//Null string is treated as zero
	if (s.find_first_not_of('0') == std::string::npos)
		return 0;

	_int64 result = _atoi64(s.c_str());
	//We tested for zero above, so this must be an error
	if (result == 0) {
		if (notify_bad) {
			*notify_bad = true;
			return 0;
		}
		throw Exception(UTIL_DATACONV_BADFORMAT,
			std::string("Invalid decimal number: ").append(s));
	}

	return result;
}

//***********************************************************************************
//Double/string conversions for utility only - bbfloat is used in the UL RTVM.
//V2.24 Numerous return statements have casts (e.g. (int) or (_int64) added now to
//keep gcc quiet.
//***********************************************************************************
double StringToPlainDouble(const std::string& input_token, bool* notify_bad, bool allow_hex)
{
	const std::string* parsed_token = &input_token;
	std::string amended_token;
	bool used_amended_token = false;

	if (notify_bad)
		*notify_bad = false;

	//Special case
	if (input_token.length() == 0)
		return 0.0;

	//V3.03. Custom extension, optionally allow hex literals.
	if (allow_hex && input_token.length() > 2) {
		const char* pc = input_token.c_str();
		char hz = *pc;
		char hx = *(pc+1);
		if (hz == '0' && (hx == 'x' || hx == 'X')) {
			return HexStringToInt64(pc+2);
		}
	}
		
	//V2.24 gcc (V4ish) quite rightly doesn't like this being non-const.  A few changed lines below.
	//char* terminator = NULL;
	static const char* asterisk = "*"; 
	const char* terminator = NULL;
	double result(0.0);
	errno = 0;

	//First take care of a difference between M204 and strtod(), in that there can 
	//be a space between the leading sign and the start of the number on M204.  Bit
	//of a drag but there you go.  Leading and trailing spaces are acceptable on either
	//platform.
	size_t first_char_pos = input_token.find_first_not_of(' ');
	if (first_char_pos != std::string::npos) {
		char first_char = input_token[first_char_pos];

		if (first_char == '-' || first_char == '+') {

			//Stand-alone sign - invalid
			if (first_char_pos == input_token.length() - 1)
				//terminator = "*"; //see gcc comment above
				terminator = asterisk;  

			else if (input_token[first_char_pos + 1] == ' ') {
				size_t next_char_pos = input_token.find_first_not_of(' ', first_char_pos + 1);

				//Stand-alone sign plus space(s) - invalid
				if (next_char_pos == std::string::npos)
				//terminator = "*"; //see gcc comment above
				terminator = asterisk;  

				//Remove intervening space(s) after the sign - use a temporary string
				else {
					amended_token = input_token;
					amended_token[next_char_pos - 1] = first_char;
					amended_token[first_char_pos] = ' ';
					parsed_token = &amended_token;

					//see gcc comment above
					//result = strtod(parsed_token->c_str(), &terminator);
					char* trm = NULL;
					result = strtod(parsed_token->c_str(), &trm);
					terminator = trm;
					used_amended_token = true;
				}
			}
		}
	}

	if (!used_amended_token && terminator == NULL) {
		//see gcc comment above
		//result = strtod(parsed_token->c_str(), &terminator);
		char* trm = NULL;
		result = strtod(parsed_token->c_str(), &trm);
		terminator = trm;
	}

	if (terminator) if (*terminator) {

		//V2.11.  Jan 08.  All trailing spaces is OK.  Spaces followed by miscellaneous
		//junk is not.  See similar comment in StringToInt() earlier where I decided to
		//leave it invalid.  This is the one used in UL expressions when converting a
		//string value to a FLOAT result, which is where the problem was noticed.
		bool all_trailing_spaces = false;
		if (*terminator == ' ') {
			for (;;terminator++) {
				if (*terminator == 0) {
					all_trailing_spaces = true;
					break;
				}
				else if (*terminator != ' ')
					break;
			}
		}

		if (!all_trailing_spaces) {
			if (notify_bad) {
				*notify_bad = true;
				return 0.0;
			}

			throw Exception(UTIL_DATACONV_BADFORMAT,
				std::string("Invalid numeric literal: '")
				.append(input_token)
				.append("' is not a number"));
		}
	}

	//V2.11.  Jan 08.  Someone has finally confirmed that D/d are not allowed on M204.
	//Warren Kring says it's only E/e.  Shame about find() but what else can we do?
	if (parsed_token->find_last_of("Dd") != std::string::npos) {
		if (notify_bad) {
			*notify_bad = true;
			return 0.0;
		}
		throw Exception(UTIL_DATACONV_BADFORMAT,
			std::string("Invalid numeric literal: '")
			.append(input_token)
			.append("' exponent here must use E or e, not D or d"));
	}

	if (errno == ERANGE) {
		if (notify_bad) {
			*notify_bad = true;
			return 0.0;
		}
		throw Exception(UTIL_DATACONV_RANGE, 
			std::string("Invalid numeric literal: '")
			.append(input_token)
			.append("' is too large/small for 64-bit floating point (IEEE 754 format)"));
	}

	return result;
}

//**********************************************************************************
std::string PlainDoubleToString(const double& n)
{
	//In theory the exponent could go up to 308
	char buff[336];

	//The max dsf would be 17 or so
	sprintf(buff, "%.20G", n);

	return buff;
}

//******************************************************************************************
int DoubleToInt32(const double& d, const int rounding_mode)
{
	//A common case
	if (d == 0.0)
		return 0;

	//V2.06 Jul 07.  modf call unnecessary in truncate mode, so delay it as it's a little
	//slower (at least on this machine) than truncation by assignment to int.
//	double intpart;
//	double fracpart = modf(d, &intpart);

	//double->int is defined as truncation in C++, although as usual with FP maths it can be
	//confusing.  It seems the C++ compiler is quite good when a numeric integer literal 
	//is specified at compile time - it makes sure the internal representation exceeds the
	//integer slightly, but after computations it obviously may no longer apply.  For example
	//1.23/1.23 when evaluated may give 0.99999999999. Etc, and this func will return 0
	//if the TRUNC parameters is specified.  Nothing to do about that.
	if (rounding_mode == BB_DTOI_TRUNC)
		return (int) d;

	double intpart;
	double fracpart = modf(d, &intpart);

	//I don't know whether the intpart could ever truncate differently to the complete
	//number.  0.xx could easily differ in the tiny binary places to N.xx, but could 
	//intpart ever come out from modf() as N-1.999... ?  Dunno - assume not.
	//Maybe not even possible for fracpart to be zero.
	if (fracpart == 0.0) 
		return (int) d;

	//With NEAREST truncation may be OK anyway
	if (rounding_mode == BB_DTOI_NEAREST) {
		if (fabs(fracpart) < 0.5) //x.5 is rounded up, hence < test here
			return (int) d;
	}

	//Round up: increment truncated value according to sign of the argument
	int i = (int) intpart;
	if (d > 0.0)
		return i + 1;
	else
		return i - 1;
}

//******************************************************************************************
_int64 DoubleToInt64(const double& d, const int rounding_mode)
{
	//Virtually identical in all ways to the above function. See comments above.
	if (d == 0.0)
		return 0;

	if (rounding_mode == BB_DTOI_TRUNC)
		return (_int64) d;

	double intpart;
	double fracpart = modf(d, &intpart);

	if (fracpart == 0.0) 
		return (_int64) d;

	if (rounding_mode == BB_DTOI_NEAREST) {
		if (fabs(fracpart) < 0.5)
			return (_int64) d;
	}

	_int64 i = (_int64) intpart;
	if (d > 0.0)
		return i + 1;
	else
		return i - 1;
}

//******************************************************************************************
double RoundDouble(const double& d, const int idp, const int rounding_mode)
{
/* * * 
This was my first idea but while neater it's *much* slower - I guess the conversion
to string and back is responsible for that.  Plus you can only round to nearest (see 
comments in bbfloat.h about rounding mode problems). 

	char format[16];

	//e.g. %.4f
	//4 is the "precision" specification, which with type 'f' means rounding to nearest
	sprintf(format, "%%.%.df", idp);

	char buff[256];
	sprintf(buff, format, d);

	return strtod(buff, NULL);
* * */

	//This was my second attempt and uses all numeric operations.  Interestingly the
	//repeated multiply and divide by 10 are no slower than doing a pow(10,dp) 
	//call at the top and using that, even for high dp values.  Perhaps
	//the underlying FP routines and/or chip are optimized for multiply/divide by 10.

	//See prev func comments - this is very similar.
	if (d == 0.0)
		return 0.0;

	if (idp == 0 && rounding_mode == BB_DTOI_TRUNC) {
		//int i = (int) d;
		_int64 i = (_int64) d; //V2.24.  Incorrect behaviour of FIXED DP 0 in UL.
		return i;
	}

	double intpart;
	double fracpart = modf(d, &intpart);

	if (fracpart == 0.0)
		return d;

	//V2.06 Jul 07.  A limitation of my algorithm.  Could throw I guess.
	int dp = idp;
	if (dp > 19)
		dp = 19;

	//Example: at this point fracpart = 0.12345
	int x;
	for (x = 0; x < dp; x++)
		fracpart *= 10.0;
	//fracpart = 12.345 (if 2 dp)

	//V2.06.  Jul 07.  Use int64.  Still no good for dp > 19, but that's irrelevant in
	//User Language, and better than int anyway!
//	int ifrac = dpt::util::DoubleToInt(fracpart, rounding_mode);
	_int64 ifrac = dpt::util::DoubleToInt64(fracpart, rounding_mode);
	//ifrac = 12 (down or nearest) or 13 (up)

	fracpart = ifrac;
	//fracpart = 12.000... or 13.000...

	for (x = 0; x < dp; x++)
		fracpart /= 10;
	//fracpart = 0.12000... or 0.13000...

	return intpart + fracpart;
}

//***********************************************************************************
//Unsigned long/ASCII conversions (negative hex format is rarely used)
//***********************************************************************************
std::string UlongToHexString(const unsigned long i, int padlen, bool wrapper)
{
	if (padlen < 0)
		padlen = 0;

	char format[32];
	if (padlen == 0) strcpy(format, "%X");           //just as many digits as there are
	else sprintf(format, "%%0.%dX", padlen);         //pad to the left with zeros

	char format2[32];
	if (wrapper) sprintf(format2, "X'%s'", format);  //wrap with prefix, e.g. X'FFF'
	else strcpy(format2, format);                    //don't wrap, e.g. FFF

	char buff[256];
	sprintf(buff, format2, i);

	return buff;
}

//***********************************************************************************
//V3.0. For printing diagnostic offsets of large sequential files.
std::string Int64ToHexString(const _int64 i, int padlen, bool wrapper)
{
	if (padlen < 0)
		padlen = 0;

	//I can't find a sprintf option to handle 64 bits to hex, so do it in 2 chunks:
	LARGE_INTEGER li;
	li.QuadPart = i;

	std::string shi;
	std::string slo;

	//Get a value with no leading zeros
	if (li.HighPart != 0) {
		slo = UlongToHexString(li.LowPart, 8, false); //pad
		shi = UlongToHexString(li.HighPart, 0, false);
	}
	else {
		slo = UlongToHexString(li.LowPart, 0, false); //don't pad
	}

	std::string slarge = std::string(shi).append(slo);

	//Prepend leading zeros if required (the int/sprintf version does not truncate)
	int numzeros = padlen - slarge.length();
	if (numzeros > 0)
		slarge = std::string(numzeros, '0').append(slarge);

	if (wrapper)
		slarge = std::string("X'").append(slarge).append("'");

	return slarge;
}


//***********************************************************************************
unsigned long HexStringToUlong(const std::string& s)
{
	char* terminator;
	errno = 0;
	unsigned long i = strtoul(s.c_str(),&terminator,16);

	//Failure of strtoul is indicated by the second parameter getting the bad digit:
	if (*terminator) {
		throw Exception(UTIL_DATACONV_BADFORMAT,
			std::string("Invalid hex digit in '")
			.append(s)
			.append("', bad digit: '")
			.append(terminator)
			.append(1, '\''));
	}
	
	if (errno == ERANGE) {
		throw Exception(UTIL_DATACONV_RANGE, 
			std::string("Hex value is too large for unsigned 32 bit integer: ").append(s));
	}

	return i;
}

//***********************************************************************************
_int64 HexStringToInt64(const std::string& s)
{
	_int64 low = 0;
	_int64 high = 0;
	_int64 whole = 0;

	std::string slow;
	std::string shigh;

	//Divide up the right-hand (low) 8 chars and the rest
	slow = s;
	if (s.length() > 8) {
		slow = s.substr(s.length() - 8);
		shigh = s.substr(0, s.length() - 8);
	}

	low = HexStringToUlong(slow);
	if (shigh.length() > 0)
		high = HexStringToUlong(shigh);

	whole = (high << 32) + low;

	return whole;
}

//***********************************************************************************
//ASCII expansions
//***********************************************************************************
std::string AsciiStringToHexString(const std::string& charformat, bool format)
{
	std::string result;
	int outlen = charformat.length() * 2;
	if (format)
		outlen += 3;
	result.reserve(outlen);

	if (format)
		result = "X\'";

	for (size_t i = 0; i < charformat.length(); i++) {
		char hexchar[3];
		unsigned char c = charformat[i];
		sprintf(hexchar, "%02X", c); 
		result.append(hexchar);
	}

	if (format)
		result.append("\'");

	return result;
}

//***********************************************************************************
std::string HexStringToAsciiString(const std::string& sin)
{
	std::string hexformat = sin;
	std::string result;

	int len = hexformat.length();

	//Hex strings should ideally have an even number of digits in this context
	if (len % 2) {

		//V2.16 I've decided to be a little more forgiving and allow leading zero to be 
		//implicit.  I can't find anything that this will break, and it should improve
		//user-friendliness in some cases.  Also brings $X2C and $HEXA into equivalence 

//		throw Exception(UTIL_DATACONV_BADFORMAT, 
//			std::string("Hex string length should be even for convert: '")
//			.append(hexformat)
//			.append(1, '\''));

		//Not fast, but we're not in a tuned module here
//		hexformat.insert(0, '0'); //V2.16a.  VCC incorrectly allows this overload.
		hexformat.insert(0, "0");
		len++;
	}

	result.reserve(len / 2);
	
	char hexchar[3];
	hexchar[2] = 0;
	char* terminator;
	for (int i = 0; i < len; i+=2) {

		//get next 2 characters
		hexchar[0] = hexformat[i];

		//Spaces are not allowed (we must test, as strtoul allows leading spaces)
		if (hexchar[0] == ' ') {
			throw Exception(UTIL_DATACONV_BADFORMAT,
				std::string("Space character in hex value '") 
			.append(hexformat)
			.append(1, '\''));
		}

		hexchar[1] = hexformat[i+1];
	
		//convert to binary format
		char ch = strtoul(hexchar, &terminator, 16);

		//Failure of strtoul is indicated by the second parameter getting the bad digit:
		if (*terminator) {
			throw Exception(UTIL_DATACONV_BADFORMAT,
				std::string("Invalid hex digit in '")
				.append(hexformat)
				.append("', bad digit: '")
				.append(terminator)
				.append(1, '\''));
		}
		
		//add to result string
		result.append(1, ch);
	}

	return result;
}

//***********************************************************************************
//Got to be careful of access violations when calling this one
//***********************************************************************************
void MemDump(const char* addr, unsigned int len, 
	std::string* hexformat, std::string* charformat, bool suppress_nl)
{
	if (hexformat) 
		hexformat->reserve(len*2);
	if (charformat) 
		charformat->reserve(len);

	char* buff = new char[len+1];
	try {	
		memcpy(buff, addr, len);
		buff[len] = 0;

		//Replace hex 0 with a dot so we can treat it as a C string for convenience.
		//Also optionally suppress newlines for neater dump appearance.
		for (size_t i = 0; i < len; i++) {
			char c = buff[i];

			if (c == 0)
				buff[i] = '.';
			else if (suppress_nl && c == '\n')
				buff[i] = '.';
		}

		//Ascii format
		if (charformat)
			*charformat = buff;

		//Hex format
		if (hexformat) {
			*hexformat = buff;
			*hexformat = AsciiStringToHexString(*hexformat);

			//Put suppressed values back into the hex 
			for (size_t i = 0; i < len; i++) {
				char c = addr[i];

				if (c == 0) {
					(*hexformat)[2*i] = '0';
					(*hexformat)[2*i+1] = '0';
				}
				else if (suppress_nl && c == '\n') {
					(*hexformat)[2*i] = '0';
					(*hexformat)[2*i+1] = 'A';
				}
			}
		}
	}
	catch (...) {
		if (charformat) *charformat = "MemDump() access violation";
		if (hexformat) *hexformat = "MemDump() access violation";
		return;
	}

	delete buff;
}

//***********************************************************************************
//Produces a vector in good old MVS "dump" stylee:
//***********************************************************************************
void MemDump2(const char* addr, unsigned int len, std::vector<std::string>& result, 
			  bool show_absolute, bool suppress_nl)
{
	std::string char_dump;
	std::string hex_dump;
	result.clear();

	MemDump(addr, len, &hex_dump, &char_dump, suppress_nl);

	//Pad out with spaces so we don't go off the end during formatting
	int dumplines = char_dump.length() / 16;
	int overhang = char_dump.length() % 16;
	if (overhang) {
		dumplines++;
		char_dump.append("                ");
		hex_dump.append("                                ");
	}

	char buff[128];
	strcpy(buff, "xxxxxxxx | F1F1F1F1 F1F1F1F1 F1F1F1F1 F1F1F1F1 | abcdefghijklmnop | ");

	const char* h = hex_dump.data();
	const char* c = char_dump.data();
	int base = (show_absolute) ? reinterpret_cast<unsigned int>(addr) : 0;

	for (int x = 0; x < dumplines; x++) {
		int show_offset = base + x * 16 ;
		sprintf(buff, "%08X", show_offset);
		buff[8] = ' ';

		memcpy(buff+11, h   , 8);
		memcpy(buff+20, h+8 , 8);
		memcpy(buff+29, h+16, 8);
		memcpy(buff+38, h+24, 8);

		memcpy(buff+49, c, 16);

		memcpy(buff+68, buff, 8);
		buff[76] = 0;

		result.push_back(buff);

		h += 32;
		c += 16;
	}
}

//***********************************************************************************
//Miscellaneous string conversions
//***********************************************************************************
//***********************************************************************************
bool DeBlank(std::string& s, char blankchar)
{
	if (s.length() == 0)
		return false;

	size_t p = s.find_first_not_of(blankchar);
	if (p == std::string::npos) {
		s = std::string(); //all blanks
		return true;
	}

	bool any_erased = false;

	//remove leading blanks
	if (p != 0) {
		s.erase(0, p);
		any_erased = true;
	}

	//remove trailing blanks
	p = s.find_last_not_of(blankchar);
	if (p != s.length() - 1) {
		s.erase(p+1, std::string::npos);
		any_erased = true;
	}

	return any_erased;
}

//***********************************************************************************
//Exactly the same as the above, except all whitespace is removed.  
//Could have generalized or special-cased the above (e.g. char=-1 to dewhite), but
//to save breaking it:
bool DeWhite(std::string& s)
{
	if (s.length() == 0)
		return false;

	size_t p = s.find_first_not_of(WHITECHARS);
	if (p == std::string::npos) {
		s = std::string(); //all white
		return true;
	}

	bool any_erased = false;

	//remove leading white
	if (p != 0) {
		s.erase(0, p);
		any_erased = true;
	}

	//remove trailing white
	p = s.find_last_not_of(WHITECHARS);
	if (p != s.length() - 1) {
		s.erase(p+1, std::string::npos);
		any_erased = true;
	}

	return any_erased;
}

//***********************************************************************************
void UnBlank(std::string& s, bool total)
{
	DeBlank(s);

	int max_embedded = (total) ? 0 : 1;

	size_t space_pos = s.find(' ');
	while (space_pos != std::string::npos) {
		int next_non_space = s.find_first_not_of(' ', space_pos);

		int num_spaces = next_non_space - space_pos;

		if (num_spaces > max_embedded) {
			int remove_count = num_spaces - max_embedded;
			s.erase(space_pos, remove_count);
			next_non_space -= remove_count;
		}

		space_pos = s.find(' ', next_non_space);
	}
}

//***********************************************************************************
std::string	PadLeft(const std::string& s, const char padchar, const unsigned int size)
{
	if (size == 0) 
		return std::string();

	int padlen = size - s.length();
	if (padlen >  0) 
		return std::string(padlen, padchar).append(s);

	if (padlen == 0) 
		return s;

	//$Pad truncates from the *LEFT* if necessary
	return s.substr(-padlen, size); 
}

//*******************************************************************************************
std::string	PadRight(const std::string& s, const char padchar, const unsigned int size)
{
//actually std::string::resize() would do this all in one go I think

	if (size == 0) 
		return std::string();

	int padlen = size - s.length();
	if (padlen >  0) 
		return std::string(s).append(std::string(padlen, padchar));

	if (padlen == 0) 
		return s;

	//$Padr truncates from the *RIGHT* if necessary
	return s.substr(0, size);
}

//*******************************************************************************************
void Stretch(std::string& s, const int factor, const char padchar, bool prepad)
{
	if (factor <= 0) {
		s = std::string();
		return;
	}
	else if (factor == 1)
		return;

	int numchars = s.length();
	s.resize(numchars * factor, ' ');

	for (int i = numchars-1; i >= 0; i--) {
		char c = s[i];
		int offset_char, offset_pads;

		if (prepad) {
			offset_char = factor*i + (factor-1);
			offset_pads = offset_char - (factor-1);
		}
		else {
			offset_char = factor*i;
			offset_pads = offset_char + 1;
		}

		s[offset_char] = c;
		for (int j = 0; j < factor-1; j++)
			s[offset_pads + j] = padchar;
	}
}

//*******************************************************************************************
void WordWrap(std::vector<std::string>& paragraph, 
			  const std::string& intext, unsigned int width, unsigned int indent)
{
	if (intext.length() == 0)
		return;

	if (paragraph.size() == 0)
		paragraph.push_back(std::string());

	int avail = width - paragraph.back().length();

	//On the first line we can specify a shorter width
	if (paragraph.size() == 1)
		avail -= indent;

	//Any partial words on the current last line can be built up
	std::string temp(paragraph.back());
	avail += temp.length();
	paragraph.back() = std::string();
	temp.append(intext);

	//Then chop it into as many parts as necessary
	while (temp.length() > 0) {

		//Simple case - there is room for it all
		if ((size_t)avail >= temp.length()) {
			paragraph.back().append(temp);
			return;
		}

		//Skip past words that will fit in
		size_t spacepos = std::string::npos;
		size_t prevspacepos;
		for (;;) {
			prevspacepos = spacepos;
			spacepos = temp.find(' ', spacepos + 1);
			if (spacepos == std::string::npos || spacepos > (size_t)avail) {
				spacepos = prevspacepos;
				break;
			}
		}

		//There was a usable word break
		if (spacepos != std::string::npos && spacepos <= (size_t)avail) {
			paragraph.back().append(temp, 0, spacepos); //part before the space

			//Cut out the spaces at the break.  
			//* * *
			//There is a minor problem with this algorithm:
			//If the part of the line before the space exactly fills the paragraph width, 
			//and the rest of the line is spaces, they are dropped.  Then the concatenation
			//phase of the next line that comes in results in a word being created as if
			//there were no trailing spaces on this line.  Possibly an unlikely
			//scenario, and it shouldn't mess up PRINT N which is the main purpose
			//of this function.
			//* * *
			size_t next_non_space = temp.find_first_not_of(' ', spacepos);
			if (next_non_space == std::string::npos)
				return;

			paragraph.push_back(std::string());
			avail = width;

			temp.erase(0, next_non_space);
		}

		//No appropriate word break - chop mid word.
		else {

			//But only once we've tried it on a whole line
			if (paragraph.back().length() == 0) {
				paragraph.back().append(temp, 0, avail);
				temp.erase(0, avail);
			}

			paragraph.push_back(std::string());
			avail = width;
			continue;
		}
	}
}

//*******************************************************************************************
std::string PadOrElide(const std::string& ipstr, unsigned int oplen)
{
	//Need to have space for the dots
	if (oplen < 3)
		oplen = 3;

	std::string s(ipstr);

	//Truncate or pad
	if (s.length() < oplen)
		s.resize(oplen, ' ');
	else if (s.length() > oplen) {
		s.resize(oplen);
		s[oplen - 2] = '.';
		s[oplen - 1] = '.';
	}

	return s;
}

//*******************************************************************************************
std::string GenerateNextStringValue(const std::string& ipstr)
{
	if (ipstr.length() > 254)
		throw Exception(UTIL_DATACONV_BADFORMAT, "GNVS requires original string < 255 chars");

	char result[255];
	strcpy(result, ipstr.c_str());

	for (int modpos = ipstr.length() - 1; modpos >= 0; modpos --) {
		char& modchar = result[modpos];

		//Increment the final character of the string
		if (modchar != -1) {
			modchar++;
			return result;
		}

		//Or the one before that if it's already 255
		modchar = 0;
	}

	//All 255s, so lengthen the string with hex 01
	return std::string(ipstr).append(1, 1);
}

//***********************************************************************************
std::string	ZeroPad(const int num, const unsigned int size, bool truncate)
{
	char buff[512] = {0};
	int full_length = 0;

	//e.g. %.5d
	//5 here is the "precision" specification (cf width below), and means the 
	//value will be padded to the left with zeros.
	char format[8];
	sprintf(format, "%%.%.dd", size);

	full_length = sprintf(buff, format, num);

	//sprintf does not truncate if there was more than the requested number of chars
	int trunclen = 0;
	if (truncate) {
		trunclen = full_length - size;
		if (trunclen < 0)
			trunclen = 0;
	}

	return buff + trunclen;
}

//*******************************************************************************************
void Reverse(char* pfrom, int slen)
{
	char* pto = pfrom + slen - 1;

	while (pfrom < pto) {
		char temp = *pto;
		*pto = *pfrom;
		*pfrom = temp;

		pfrom++;
		pto--;
	}
}

//*******************************************************************************************
void Reverse(std::string& s)
{
	int pos1 = 0;
	int pos2 = s.length() - 1;

	//This is not nearly as efficient as the buffer-based one above
	while (pos1 < pos2) {
		char temp = s[pos2];
		s[pos2] = s[pos1];
		s[pos1] = temp;

		pos1++;
		pos2--;
	}
}

//*******************************************************************************************
std::string ThousandSep(const std::string& s)
{
	int slen = s.length();
	if (slen <= 3)
		return s;

	std::string sout(slen + (slen-1)/3, ',');

	int spos = slen-1;
	int soutpos = sout.length()-1;

	for (; spos >= 0; spos--, soutpos--) {
		sout[soutpos] = s[spos];
		if ( ((slen-spos) % 3) == 0)
			soutpos--;
	}

	return sout;
}

//*******************************************************************************************
int ReplaceChar(std::string& s, char c1, char c2)
{
	int num_replaced = 0;

	for (size_t i = s.find(c1); i != std::string::npos; i = s.find(c1, i)) {
		s[i] = c2;
		num_replaced++;
	}

	return num_replaced;
}

//*******************************************************************************************
int ReplaceString(std::string& s, const std::string& s1, const std::string& s2,
				  int startpos, int repoccs)
{
	if (s1 == s2 || s.length() == 0 || s1.length() == 0 ||
		startpos < 0 || (size_t)startpos >= s.length() || repoccs <= 0)
	{
		return 0;
	}

	int num_replaced = 0;
	size_t pos = startpos;

	for (;;) {
		pos = s.find(s1, pos);
		if (pos == std::string::npos)
			break;

		s.replace(pos, s1.length(), s2);

		num_replaced++;
		if (num_replaced == repoccs)
			break;

		//Avoid infinite loop if s2 contains s1
		pos += s2.length();
	}

	return num_replaced;
}

//*******************************************************************************************
int UnHexHex(std::string& s, char c)
{
	char hexchar[3];
	hexchar[2] = 0;
	int num_replaced = 0;

	for (size_t x = s.find(c);
			x != std::string::npos && x < s.length() - 2;
			x = s.find(c, x+1))
	{
		//Take the next two characters as a hex byte
		hexchar[0] = s[x+1];
		hexchar[1] = s[x+2];

		char* terminator;
		char ch = strtoul(hexchar, &terminator, 16);

		if (terminator == &hexchar[2]) {
			s.replace(x, 3, 1, ch);
			num_replaced++;
		}
	}

	return num_replaced;
}

//***********************************************************************************
std::string	SpacePad
(int num, const unsigned int size, bool showzero, bool truncate, bool commas)
{
	char buff[512] = {0};
	int full_length = 0;

	//e.g. %5.0d
	//5 is the "width" specification (cf precision above) which pads to the left 
	//with spaces.  The doc says the default precision is 1 but it seems to be 0.
	//I'm specifying 0 explicitly anyway, so that we then have the option of showing 
	//zero as per the function parameter.
	char format[8];
	sprintf(format, "%%%.d.0d", size); 

	full_length = sprintf(buff, format, num);

	//Zero input 
	if (showzero) {
		if (buff[size-1] == ' ')
			buff[size-1] = '0';
	}

	//V2.06: demarcate thousands (probably a standard function to do this but never mind).
	//See also InsertThousandSep() above which works on a string not a buffer.
	if (commas) {
		//Start at zero terminator
		char* pterm = strchr(buff, 0);
		int lenterm = 1;

		//Work back while there's more than 3 ungrouped digits
		while (pterm-3 > buff && *(pterm-4) != ' ') {
			pterm -= 3;
			lenterm += 3;
			memmove(pterm+1, pterm, lenterm);
			*pterm = ',';
			full_length++;
		}
	}

	//sprintf does not truncate if there was more than the requested number of chars
	int trunclen = 0;
	if (truncate) {
		trunclen = full_length - size;
		if (trunclen < 0)
			trunclen = 0;
	}

	return buff + trunclen;
}













//***********************************************************************************
//Number formatters shared by images and $functions.
//***********************************************************************************
int Binary
(std::string& result, double dval, int numbits, int fracbits, int opbytes, bool unsig)
{
	int opbits = opbytes * 8;

	if (numbits < 1			|| 
		opbits > 32			|| 
		numbits > opbits	|| 
		fracbits < 0		|| 
		fracbits > opbits	|| 
		fracbits > numbits
		)
		return NUMFORMAT_PARMERROR1;

	//This happens invisibly
	if (unsig)
		dval = fabs(dval);

	//Assumed binary places are added just by doubling the input value.  So for example
	//we want to output 3.25 and ask for 2 binary places.  We double twice to get 13.0
	//which is output as 1101 with 2 implied BP (i.e. 11.01)
	int factor = 1;
	factor <<= fracbits;
	dval *= factor;

	//Could in theory have overflowed from the doubling, although elsewhere numbers
	//greater than E75 are blocked so in practice this probably can't happen.
	if (!_finite(dval))
		return NUMFORMAT_BADIVAL1;

	//"Overflow" is also where insufficient bits were specified
	unsigned int max = UINT_MAX >> (32 - numbits);
	if (!unsig)
		max /= 2;
	if (dval > max)
		return NUMFORMAT_NOROOM;

	if (!unsig) {
		int min = INT_MIN >> (32 - numbits);
		if (dval < min)
			return NUMFORMAT_NOROOM;
	}

	//Note that this assumes little endian byte order
	char buff[4];
	if (unsig) {
		unsigned int i = (unsigned int) dval;
		* (reinterpret_cast<unsigned int*>(buff)) = i;
	}
	else {
		int i = (int) dval;
		* (reinterpret_cast<int*>(buff)) = i;
	}

	result = std::string(buff, opbytes);
	return NUMFORMAT_OK;
}

//***********************************************************************************
int Eformat(std::string& result, double value, int dsf, int dpreq)
{
	if (dsf < 1 || dsf > 15)
		return NUMFORMAT_PARMERROR1;
	if (dpreq < 0 || dpreq > 15)
		return NUMFORMAT_PARMERROR2;

	//I think this looks nicer than what it gives otherwise, e.g. 0.000E03
	if (value == 0.0) {
		result = "0";
		return NUMFORMAT_OK;
	}

	//Get the appropriate DSF...
	char format[16];
	sprintf(format, "%%.%.dG", dsf);

	//...by doing a 'G' sprintf and reversing it (intermediate format is irrelevant)
	char buff[128];
	sprintf(buff, format, value);
	value = strtod(buff, NULL);

	//I can't find a printf that will do this.  The 'E' format option always gives one
	//digit to the left and just drops any others without scaling the exponent.

	//So do it by hand.  Start with an E format number of the form n.nnnnnnnnnnnnnnnE+nnn
	int bufflen = sprintf(buff, "%.15E", value);
	char* firstdigit = (value < 0) ? buff + 1 : buff;

	//Remove trailing zeroes
	char* lastdigit = buff + bufflen - 6;
	char* pc;
	for (pc = lastdigit; *pc == '0'; pc--)
		;

	//Move the exponent back in after the above
	int delz = lastdigit - pc;
	if (delz > 0) {
		memmove(pc+1, pc+1+delz, 6); 
		bufflen -= delz;
	}

	//Count how many significant digits after the point now
	int dpsf;
	for (dpsf = 0; *pc != '.'; pc--)
		dpsf++;

	//Do we have to move it?
	int leftshift = dpreq - dpsf;

	//Move some digits to make way for the moved DP
	if (leftshift > 0) {

		//Add some leading zeroes so we end up with 0.0....
		memmove(firstdigit + leftshift, firstdigit, bufflen+2);
		bufflen += leftshift;
		memset(firstdigit, '0', leftshift);

		memmove(firstdigit+2, firstdigit+1, leftshift);
		*(firstdigit+1) = '.';
	}
	else {
		memmove(firstdigit+1, firstdigit+2, -leftshift);
		if (dpreq > 0)
			//Move point right
			*(firstdigit+1-leftshift) = '.';
		else {
			//Point may be no longer needed
			char* pe = buff + bufflen - 5;
			memmove(pe-1, pe, 6);
			bufflen--;
		}
	}

	//Alter the exponent to match (do this in any case to get a M204 style 2-digit
	//exponent with no plus sign).
	char* pe = buff + bufflen - 4;
	int e = atoi(pe);
	sprintf(pe, "%.2d", e + leftshift);

	result = buff;
	return NUMFORMAT_OK;
}

//***********************************************************************************
int FloatCast(char* buff, double dval, bool shorten)
{
	if (shorten)
		* (reinterpret_cast<float*>(buff)) = dval;
	else
		* (reinterpret_cast<double*>(buff)) = dval;

	return NUMFORMAT_OK;
}

//***********************************************************************************
int Pack(std::string& result, double dval, int totdigits, int decdigits, bool unsig)
{
	result = std::string();

	if (totdigits < 1 || totdigits > 18)
		return NUMFORMAT_PARMERROR1;

	if (decdigits < 0 || decdigits > totdigits)
		return NUMFORMAT_PARMERROR2;

	bool negative = false;
	if (dval < 0) {
		negative = true;
		dval = -dval;
	}

	//Use this class for the convenience of its DP stringizer function
	RangeCheckedDouble rdin;
	try {
		rdin = dval;
	}
	catch (...) {
		return NUMFORMAT_BADIVAL1;
	}

	//Get the integer and fractional parts as strings
	std::string sint = rdin.ToStringWithFixedDP(decdigits);
	std::string sdec;

	if (decdigits > 0) {
		int pos = sint.find('.');
		sdec = sint.substr(pos+1);

		if (pos == 1 && sint[0] == '0')
			sint.resize(0);
		else
			sint.resize(pos);
	}

	//How long will the value be in hex digits?
	int oplen_requested = totdigits + 1;

	int oplen_required = sint.length() + sdec.length() + 1;

	//Construct the output hex string with appropriate hex zero prefix
	int numzeros = oplen_requested - oplen_required;
	if (numzeros < 0)
		return NUMFORMAT_NOROOM;

	//Round up to an even number of characters for the hex representation
	if (oplen_requested % 2)
		numzeros++;

	std::string sout(numzeros, '0');
	sout.append(sint).append(sdec);

	if (unsig)
		sout.append(1, 'F');
	else
		sout.append(1, (negative) ? 'D' : 'C');

	//Convert to ascii
	result = util::HexStringToAsciiString(sout);
	return NUMFORMAT_OK;
}

//***********************************************************************************
int Unbin(double& dval, const std::string& bitval, int fracbits, bool unsig)
{
	dval = 0.0;

	int numbytes = bitval.length();
	if (numbytes < 1 || numbytes > 4)
		return NUMFORMAT_BADIVAL1;

	int numbits = 8 * numbytes;
	if (!unsig)
		numbits--;

	if (fracbits < 0 || fracbits > 32)
		return NUMFORMAT_PARMERROR1;
	if (fracbits > numbits)
		return NUMFORMAT_PARMERROR2;

	//As with the reverse function this assumes little-endian byte order
	unsigned char buff[4] = {0,0,0,0};
	memcpy(buff, bitval.c_str(), numbytes);
	
	if (unsig)
		dval = * (reinterpret_cast<const unsigned int*>(buff));
	else {
		//May as well make use of standard conversions
		if (numbytes == 4)
			dval = * (reinterpret_cast<const int*>(buff));
		else if (numbytes == 2)
			dval = * (reinterpret_cast<const short*>(buff));
		else if (numbytes == 1)
			dval = * (reinterpret_cast<const char*>(buff));
		else {
			char b2 = * (reinterpret_cast<const char*>(buff+2));
			if (b2 < 0)
				buff[3] = '\xff';
			dval = * (reinterpret_cast<const int*>(buff));
		}
	}

	//Rescale it by halving
	int factor = 1;
	factor <<= fracbits;
	dval /= factor;

	return NUMFORMAT_OK;
}

//***********************************************************************************
int UnfloatCast(double& result, const std::string& bitval)
{
	int numbytes = bitval.length();
	if (numbytes != 4 && numbytes != 8)
		return NUMFORMAT_BADIVAL1;

	if (numbytes == 4) 
		result = * (reinterpret_cast<const float*>(bitval.data()));
	else
		result = * (reinterpret_cast<const double*>(bitval.data()));

	return NUMFORMAT_OK;
}

//***********************************************************************************
int Unpack(double& result, const std::string& cval, int decdigs, int sig)
{
	enum {ANYSIGN = 0, SIGNED = 1, UNSIGNED = 2};
	if (sig < 0 || sig > 2)
		sig = ANYSIGN;

	if (cval.length() <= 0 || cval.length() > 10)
		return NUMFORMAT_BADIVAL1;

	if (decdigs < 0)
		return NUMFORMAT_PARMERROR1;

	std::string hval = util::AsciiStringToHexString(cval);

	//Convert the sign suffix to a minus sign if appropriate
	bool negative;
	char signchar = hval[hval.length() - 1];

	switch (signchar) {
	case 'F':
		if (sig == SIGNED)
			return NUMFORMAT_BADIVAL2;
		negative = false;
		break;
	case 'D':
		if (sig == UNSIGNED)
			return NUMFORMAT_BADIVAL2;
		negative = true;
		break;
	case 'C':
		if (sig == UNSIGNED)
			return NUMFORMAT_BADIVAL2;
		negative = false;
		break;
	default:
		return NUMFORMAT_BADIVAL2;
	}

	hval.resize(hval.length() - 1);
	if ((size_t)decdigs > hval.length())
		return NUMFORMAT_PARMERROR2;

	int zero = 0;

	//Reinsert the sign and decimal point
	if (decdigs > 0)
		hval.insert(hval.length() - decdigs, 1, '.');
	if (negative)
		hval.insert(zero, 1, '-');

	//Then reformat as a number
	try {
		result = util::StringToPlainDouble(hval);
	}
	catch (...) {
		return NUMFORMAT_BADIVAL3;
	}

	return NUMFORMAT_OK;
}

//***********************************************************************************
void EncodeNoCRLFValue(unsigned char* buff, int len)
{
	if (len < 0 || len > 127) {
		//V2.07 Sep 07.  Some detailed analysis (!) showed that 127, not 255, is the 
		//longest string that we can reliably expect this to work on (thanks Roger M).
		//The test for len<0 is also new and probably a good idea.
		throw Exception(UTIL_DATACONV_BADFORMAT, 
			"Invalid length parameter to EncodeNoCRLFValue()");
	}

	unsigned char* buffend = buff + len;

	//Scan for both CR and LF to be sure
	for (unsigned char* c = buff+1; c <= buffend; c++) {
		if (*c == '\n' || *c == '\r') {

			//Cycle all bytes in the whole buffer, including the control byte
			for (unsigned char* c2 = buff; c2 <= buffend; c2++)
				(*c2)++;

			//Call recursively 
			EncodeNoCRLFValue(buff, len);
		}
	}
}

//***********************************************************************************
//Reverses the above transformation.
void DecodeNoCRLFValue(unsigned char* buff, int len)
{
	unsigned char* buffend = buff + len;
	while (*buff > 0) {
		for (unsigned char* c2 = buff; c2 <= buffend; c2++)
			(*c2)--;
	}
}

}} //close namespace
