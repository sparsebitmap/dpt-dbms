/********************************************************************************************
Data conversions:

See also bbfloat.h for more specifically on M204-style handling of floating point.
See also charconv.h for ebcdic/ascii/unicode and endianism conversions.
********************************************************************************************/

#if !defined(BB_DATACONV)
#define BB_DATACONV

#include <string>
#include <vector>

namespace dpt {
namespace util {

//Decimal string <-> int
int StringToInt(const std::string& s, bool* notify_bad = NULL);
std::string IntToString(const int);
std::string VectorOfIntToString(std::vector<int>&);

//Decimal string <-> unsigned long
std::string UlongToString(const unsigned long);
unsigned long StringToUlong(const std::string&, bool* notify_bad = NULL);

//Decimal string <-> 64 bit integer (mainly used in file access)
std::string Int64ToString(const _int64&);
_int64 StringToInt64(const std::string&, bool* notify_bad = NULL);

//Decimal string <-> 64 bit floating point
//Important: these functions are here for general utility purposes, not for the important
//business of FLOAT processing in files and user langauge.  See bbfloat.h for that.
std::string PlainDoubleToString(const double&);
double StringToPlainDouble(const std::string&, bool* notify_bad = NULL, bool allow_hex = false);

//Numeric double conversions
const int BB_DTOI_TRUNC		= 1;
const int BB_DTOI_NEAREST	= 2;
const int BB_DTOI_ROUNDUP	= 3;
int DoubleToInt32(const double&, const int mode);
_int64 DoubleToInt64(const double&, const int mode);
double RoundDouble(const double&, const int, const int);

//Hex string <-> unsigned long (negative hex format is rarely used)
std::string UlongToHexString(const unsigned long, const int = 0, const bool = false);
unsigned long HexStringToUlong(const std::string&);
std::string Int64ToHexString(const _int64, const int = 0, const bool = false);
_int64 HexStringToInt64(const std::string&);

//ASCII expansions
std::string AsciiStringToHexString(const std::string&, bool = false);
std::string HexStringToAsciiString(const std::string&);
void MemDump(const char*, unsigned int, std::string* = NULL, std::string* = NULL, bool = false);
void MemDump2(const char*, unsigned int, std::vector<std::string>&, bool = false, bool = false);

//Miscellaneous string conversions
bool DeBlank(std::string&, char blankchar = ' '); //rcode is whether s was changed
bool DeWhite(std::string&);
void UnBlank(std::string&, bool); //true removes single embdedded spaces
void Stretch(std::string&, const int, const char = ' ', bool = true);
std::string	PadLeft(const std::string&, const char, const unsigned int);
std::string	PadRight(const std::string&, const char, const unsigned int);
void WordWrap(std::vector<std::string>&, const std::string&, unsigned int, unsigned int = 0);
std::string PadOrElide(const std::string&, const unsigned int);
std::string GenerateNextStringValue(const std::string&);
void Reverse(char* s, int slen); //handles hex zeros unlike _strrev
void Reverse(std::string&);      //""
std::string ThousandSep(const std::string&);

//Sometimes used to make parsing easier
int ReplaceChar(std::string&, char, char);
int ReplaceString(std::string&, const std::string&, const std::string&, int = 0, int = INT_MAX);
int UnHexHex(std::string&, char = '%'); //e.g. file paths or for hiding reserved characters

//Variations on padding to save the caller having to convert to numbers first
std::string	ZeroPad(const int, const unsigned int, bool = true);
std::string	SpacePad(const int, const unsigned int, bool = true, bool = true, bool = false);

//Number formatters, used in $functions and image item processing
int Binary(std::string&, double, int, int, int, bool);
int Eformat(std::string&, double, int, int);
int FloatCast(char*, double, bool = false);
int Pack(std::string&, double, int, int, bool); 
int Unbin(double&, const std::string&, int, bool);
int UnfloatCast(double&, const std::string&);
int Unpack(double&, const std::string&, int, int sig=0); //0=anysign, 1=signed, 2=unsigned

//Used in disk IO to avoid literal numbers
void EncodeNoCRLFValue(unsigned char*, int); //len includes control byte
void DecodeNoCRLFValue(unsigned char*, int); //''

//V2.26 share these
#ifndef BB_DATACONV_CONSTANTS
#define BB_DATACONV_CONSTANTS

const int NUMFORMAT_OK = 0;
const int NUMFORMAT_PARMERROR1 = 1;
const int NUMFORMAT_PARMERROR2 = 2;
const int NUMFORMAT_PARMERROR3 = 3;
const int NUMFORMAT_BADIVAL1 = 11;
const int NUMFORMAT_BADIVAL2 = 12;
const int NUMFORMAT_BADIVAL3 = 13;
const int NUMFORMAT_NOROOM = 21;

#endif

}} //close namespace

#endif
