
#include "stdafx.h"
#include "charconv.h"

#include "except.h"
#include "msg_util.h"

namespace dpt { namespace util {

//*******************************************************************************************
//These two tables are based on the common entries in EBCDIC code page 500 and Windows
//page 1252, with the rest (32 each way) mapped to a period in the other character set.
//*******************************************************************************************
unsigned char A_TO_E_CODE_PAGE[257] = {
         /* 00  01  02  03  04  05  06  07  08  09  0A  0B  0C  0D  0E  0F  */
/* 00 */ "\x00\x01\x02\x03\x37\x2d\x2e\x2f\x16\x05\x25\x0b\x0c\x0d\x0e\x0f"
/* 10 */ "\x10\x11\x12\x13\x3c\x3d\x32\x26\x18\x19\x3f\x27\x1c\x1d\x1e\x1f"
/* 20 */ "\x40\x4f\x7f\x7b\x5b\x6c\x50\x7d\x4d\x5d\x5c\x4e\x6b\x60\x4b\x61"
/* 30 */ "\xf0\xf1\xf2\xf3\xf4\xf5\xf6\xf7\xf8\xf9\x7a\x5e\x4c\x7e\x6e\x6f"
/* 40 */ "\x7c\xc1\xc2\xc3\xc4\xc5\xc6\xc7\xc8\xc9\xd1\xd2\xd3\xd4\xd5\xd6"
/* 50 */ "\xd7\xd8\xd9\xe2\xe3\xe4\xe5\xe6\xe7\xe8\xe9\x4a\xe0\x5a\x5f\x6d"
/* 60 */ "\x79\x81\x82\x83\x84\x85\x86\x87\x88\x89\x91\x92\x93\x94\x95\x96"
/* 70 */ "\x97\x98\x99\xa2\xa3\xa4\xa5\xa6\xa7\xa8\xa9\xc0\xbb\xd0\xa1\x07"
/* 80 */ "\x4b\x4b\x4b\x4b\x4b\x4b\x4b\x4b\x4b\x4b\x4b\x4b\x4b\x4b\x4b\x4b"
/* 90 */ "\x4b\x4b\x4b\x4b\x4b\x4b\x4b\x4b\x4b\x4b\x4b\x4b\x4b\x4b\x4b\x4b"
/* A0 */ "\x41\xaa\xb0\xb1\x9f\xb2\x6a\xb5\xbd\xb4\x9a\x8a\xba\xca\xaf\xbc"
/* B0 */ "\x90\x8f\xea\xfa\xbe\xa0\xb6\xb3\x9d\xda\x9b\x8b\xb7\xb8\xb9\xab"
/* C0 */ "\x64\x65\x62\x66\x63\x67\x9e\x68\x74\x71\x72\x73\x78\x75\x76\x77"
/* D0 */ "\xac\x69\xed\xee\xeb\xef\xec\xbf\x80\xfd\xfe\xfb\xfc\xad\xae\x59"
/* E0 */ "\x44\x45\x42\x46\x43\x47\x9c\x48\x54\x51\x52\x53\x58\x55\x56\x57"
/* F0 */ "\x8c\x49\xcd\xce\xcb\xcf\xcc\xe1\x70\xdd\xde\xdb\xdc\x8d\x8e\xdf"};

unsigned char E_TO_A_CODE_PAGE[257] = {
         /* 00  01  02  03  04  05  06  07  08  09  0A  0B  0C  0D  0E  0F  */
/* 00 */ "\x00\x01\x02\x03\x2e\x09\x2e\x7f\x2e\x2e\x2e\x0b\x0c\x0d\x0e\x0f"
/* 10 */ "\x10\x11\x12\x13\x2e\x2e\x08\x2e\x18\x19\x2e\x2e\x1c\x1d\x1e\x1f"
/* 20 */ "\x2e\x2e\x2e\x2e\x2e\x0a\x17\x1b\x2e\x2e\x2e\x2e\x2e\x05\x06\x07"
/* 30 */ "\x2e\x2e\x16\x2e\x2e\x2e\x2e\x04\x2e\x2e\x2e\x2e\x14\x15\x2e\x1a"
/* 40 */ "\x20\xa0\xe2\xe4\xe0\xe1\xe3\xe5\xe7\xf1\x5b\x2e\x3c\x28\x2b\x21"
/* 50 */ "\x26\xe9\xea\xeb\xe8\xed\xee\xef\xec\xdf\x5d\x24\x2a\x29\x3b\x5e"
/* 60 */ "\x2d\x2f\xc2\xc4\xc0\xc1\xc3\xc5\xc7\xd1\xa6\x2c\x25\x5f\x3e\x3f"
/* 70 */ "\xf8\xc9\xca\xcb\xc8\xcd\xce\xcf\xcc\x60\x3a\x23\x40\x27\x3d\x22"
/* 80 */ "\xd8\x61\x62\x63\x64\x65\x66\x67\x68\x69\xab\xbb\xf0\xfd\xfe\xb1"
/* 90 */ "\xb0\x6a\x6b\x6c\x6d\x6e\x6f\x70\x71\x72\xaa\xba\xe6\xb8\xc6\xa4"
/* A0 */ "\xb5\x7e\x73\x74\x75\x76\x77\x78\x79\x7a\xa1\xbf\xd0\xdd\xde\xae"
/* B0 */ "\xa2\xa3\xa5\xb7\xa9\xa7\xb6\xbc\xbd\xbe\xac\x7c\xaf\xa8\xb4\xd7"
/* C0 */ "\x7b\x41\x42\x43\x44\x45\x46\x47\x48\x49\xad\xf4\xf6\xf2\xf3\xf5"
/* D0 */ "\x7d\x4a\x4b\x4c\x4d\x4e\x4f\x50\x51\x52\xb9\xfb\xfc\xf9\xfa\xff"
/* E0 */ "\x5c\xf7\x53\x54\x55\x56\x57\x58\x59\x5a\xb2\xd4\xd6\xd2\xd3\xd5"
/* F0 */ "\x30\x31\x32\x33\x34\x35\x36\x37\x38\x39\xb3\xdb\xdc\xd9\xda\x2e"};

//******************************
void SetCodePageA2E(const std::string& s)
{
	if (s.length() == 256)
		memcpy(A_TO_E_CODE_PAGE, s.c_str(), 256);
	else	
		throw Exception(UTIL_DATACONV_BADFORMAT, 
			"Invalid new code page - must contain 256 entries");
}

//******************************
void SetCodePageE2A(const std::string& s)
{
	if (s.length() == 256)
		memcpy(E_TO_A_CODE_PAGE, s.c_str(), 256);
	else	
		throw Exception(UTIL_DATACONV_BADFORMAT, 
			"Invalid new code page - must contain 256 entries");
}

//******************************
char* AsciiToEbcdic(char* s, int len) 
{
	//V2.20.  Jul 09.  Tuned this slightly for the more common case of length given.
	if (len == -1)
		len = strlen(s);

	char* ptr = s;
	char* end = s + len;
	while (ptr != end) {
		*ptr = AsciiToEbcdic(*ptr);
		ptr++;
	}

/*
	//This could all be formulated on the for statement but it would be ugly
	int ct = 0;
	char* ptr = s;
	for (;;) {
		if (len >= 0) {			//EOS condition: length was given 
			if (ct == len)
				break;
			ct++;
		}
		else {
			if (*ptr == '\0')	//EOS condition: zero terminated
				break;
		}
		*ptr = AsciiToEbcdic(*ptr);
		ptr++;
	}
*/
	return s;
}

//******************************
//See comments above in AsciiToEbcdic
char* EbcdicToAscii(char* s, int len) 
{
	if (len == -1)
		len = strlen(s);

	char* ptr = s;
	char* end = s + len;
	while (ptr != end) {
		*ptr = EbcdicToAscii(*ptr);
		ptr++;
	}

/*	Experimenting with compiled instruction count

	int ct = 0;
	char* ptr = s;
	for (;;) {
		if (len >= 0) {
			if (ct == len)
				break;
			ct++;
		}
		else {
			if (*ptr == '\0')
				break;
		}
		*ptr = EbcdicToAscii(*ptr);
		ptr++;
	}
*/
	return s;
}

//******************************
void AsciiToEbcdic(std::string& s) 
{
	for (size_t x = 0; x < s.length(); x++)
		s[x]=AsciiToEbcdic(s[x]);
}

//******************************
void EbcdicToAscii(std::string& s) 
{
	for (size_t x = 0; x < s.length(); x++)
		s[x]=EbcdicToAscii(s[x]);
}

//******************************
char* ToUpper(char* s, int len) 
{
	//See comments in AsciiToEbcdic
	int ct = 0;
	char* ptr = s;
	for (;;) {
		if (len >= 0) {
			if (ct == len)
				break;
			ct++;
		}
		else {
			if (*ptr == '\0')
				break;
		}
		*ptr = ToUpper(*ptr);
		ptr++;
	}

	return s;
}

//******************************
char* ToLower(char* s, int len) 
{
	//See comments in AsciiToEbcdic
	int ct = 0;
	char* ptr = s;
	for (;;) {
		if (len >= 0) {
			if (ct == len)
				break;
			ct++;
		}
		else {
			if (*ptr == '\0')
				break;
		}
		*ptr = ToLower(*ptr);
		ptr++;
	}

	return s;
}

//******************************
void ToUpper(std::string& s) 
{
	int len = s.length();
	for (int x=0; x<len; x++) {
		char c = s[x];
		char C = ToUpper(c);
		//Most common case is often that no change is required - avoid std::string overhead
		if (c != C)
			s[x]=C;
	}
}

//******************************
void ToLower(std::string& s, bool initial_cap) 
{
	int len = s.length();
	for (int x=0; x<len; x++) {
		char C = s[x];

		char c;

		//V2.20 this can be nice (e.g. like the UL function $LOWER)
		if (initial_cap && x == 0) {
			c = ToUpper(C);
		}
		else {
			c = ToLower(C);
		}

		//A common case is when no change is required - avoid std::string overheads
		if (c != C)
			s[x]=c;
	}
}

//******************************
int MemCmpNoCase(
    const char* buff1, 
    const char* buff2, 
    int buffLen)
{
	//Check the parameters
	if (buffLen < 0)
		throw Exception(UTIL_DATACONV_BADFORMAT, "MemCmpNoCase() - negative scan length");
	
	if (buff1 == NULL || buff2 == NULL)
		return NULL;

	int result = 0;

	//Set up a loop
	const char* ptr = buff1;
	const char* targetPtr = buff2;
	const char* buffEnd = buff1 + buffLen;

	/*
	Note that we must standardise the case of the two strings. This gives us the
	option of uppercasing both strings before calling memcmp, or trying to be
	a little smarter. CRT memcmp is obviously highly tuned, the main shortcut
	being that it will use multi-byte-compare machine instructions for long
	strings rather than comparing a byte at a time. The question of how far 
	into the string the first difference lies is really going to depend on the 
	data, but you have to assume that a lot of the time, especially in later 
	stages of a sort, it will be towards the end of the string. Nevertheless 
	the algorithm below is fairly fast, and benefits from the fact that it
	will shortcut out as soon as it finds a difference. Clearly in a sort situation
	there are other algorithm designs you might sit on top of the compare func, but
	here we have just the two strings and have to do what we can in this context. 
	To uppercase both (assuming possible hex  zeros) would mean 2 loops a byte at 
	a time, one through each, before even calling  memcmp, since strupr relies on 
	the zero terminator, and would also mean doing both strings in their entirety 
	(imagine the waste where you had 2 strings of 100 bytes each, where they 
	differed in the first byte tested).
	So in conclusion it is so clearly better (in my mind!) to just do 1 loop for 
	both the conversion and the comparison that I'm not even going to bother
	running benchmarks! At least not right now :-)
	*/
	while (ptr < buffEnd)
	{
		//Arbitrary choice to go to upper and not lower
		char thisChar = ToUpper(*ptr);
		char targetChar = ToUpper(*targetPtr);

		if (thisChar == targetChar)
		{
			//Continue while they're still holding equal
			ptr++;
			targetPtr++;
		}
		else
		{
			//Break as soon as we get a difference. Report -1 for LHS lower.
			result = (thisChar < targetChar) ? -1 : 1;
			break;
		}
	}

	return result;
}

}} //close namespace
