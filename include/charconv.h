
//Character set, endianinism, etc. conversions

#if !defined(BB_CHARCONV)
#define BB_CHARCONV

#include <string>

namespace dpt { namespace util {

//ENDIANISM

inline void Swap(char* a, char* b) {char t=*a; *a=*b; *b=t;}
inline void ReverseInt16(short* i) {char* c=(char*)(i); Swap(c, c+1);}
inline void ReverseInt32(int* i) {char* c=(char*)(i); Swap(c, c+3); Swap(c+1, c+2);}
inline void ReverseDouble(double* d) {
	char* c=(char*)(d); Swap(c, c+7); Swap(c+1, c+6); Swap(c+2, c+5); Swap(c+3, c+4);}

//EBCDIC<->ASCII

extern unsigned char A_TO_E_CODE_PAGE[257];
extern unsigned char E_TO_A_CODE_PAGE[257];
void SetCodePageE2A(const std::string&);
void SetCodePageA2E(const std::string&);

//Single character
inline char AsciiToEbcdic(char c) {return A_TO_E_CODE_PAGE[(unsigned char)c];}
inline char EbcdicToAscii(char c) {return E_TO_A_CODE_PAGE[(unsigned char)c];}
//In-place string
char* AsciiToEbcdic(char*, int = -1); //give length if not zero-term
char* EbcdicToAscii(char*, int = -1); //or zero may be in data
void AsciiToEbcdic(std::string&);
void EbcdicToAscii(std::string&);
//String construction
inline std::string AsciiToEbcdic(const char* s) {std::string z(s); AsciiToEbcdic(z); return z;}
inline std::string EbcdicToAscii(const char* s) {std::string z(s); EbcdicToAscii(z); return z;}

//CASE 

//V2.12 Avoid CRT to ensure no locale lock required for every char if by chance it's not
//the default "C" locale.  Review this if ever we have e.g. French keywords (BÉGÎN!).
const signed char a_minus_A = 'a' - 'A';
inline char ToUpper(char c) {if ( (c >= 'a') && (c <= 'z') ) c -= a_minus_A; return c;}
inline char ToLower(char c) {if ( (c >= 'A') && (c <= 'Z') ) c += a_minus_A; return c;}
//In-place string
char* ToUpper(char*, int = -1); //give length if not zero-term
char* ToLower(char*, int = -1); //or zero may be in data
void ToUpper(std::string&);
void ToLower(std::string&, bool initial_cap = false);
//String construction
inline std::string ToUpper(const char* s) {std::string z(s); ToUpper(z); return z;}
inline std::string ToLower(const char* s, bool i = false) {std::string z(s); ToLower(z,i); return z;}

//Case-insensitive memcmp. Generally slower than CRT cos it must go byte by byte.
int MemCmpNoCase(const char* buff, const char* target, int buffLen);

}} //close namespace

#endif
