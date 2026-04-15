//************************************************************************************
//Assorted hash functions
//************************************************************************************

#if !defined(BB_HASH)
#define BB_HASH

#include <string>

namespace dpt {

//Integer digest functions:

//Variation 1 = scan input string in reverse.
//Variation 2 = final extra shuffle - see comments in cpp file.
#if defined(_BBHOST)
unsigned int StringHash_JS(const char*, int, int variation = 0);
unsigned int StringHash_ELF(const char*, int, int variation = 0);
unsigned int StringHash_SDBM(const char*, int, int variation = 0);
unsigned int StringHash_DJB(const char*, int, int variation = 0);
unsigned int StringHash_DEK(const char*, int, int variation = 0);
unsigned int StringHash_AP(const char*, int, int variation = 0);
#endif

//String digest functions:

//V2.16 April 09
class Hash_SHA1 {
public:
	static const unsigned int DIGEST_LENGTH;
	std::string Perform(const char*, unsigned int); //neat: create string
	void Perform(char*, const char*, unsigned int); //fast: mod caller's buffer
};


} //close namespace

#endif
