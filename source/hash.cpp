
#include "stdafx.h"

#include "hash.h"

#include "winutil.h"
#include "dataconv.h"
#include "except.h"
#include "msg_util.h"
#if defined(_BBHOST)
//Awkward compilation options so localize this to the host project.
#include "randommt.h" //#include "randomMT.h" : V2.24 case is less interchangeable on *NIX - Roger M.
#endif

namespace dpt { 

#if defined(_BBHOST)

//************************************************************************************
//1. First a couple of utility functions allowing some algorithm variations 
//************************************************************************************

//************************************************************************************
//Variation 1 is to scan the string in reverse.  Depending on the algorithm 
//and data this can give a better spread of return values.  For example it is 
//common for sets of strings to have significant shared parts at the start e.g. 
//"RECTYPE_CUST", "RECTYPE_TXN", or "TIME_120000.002", "TIME_120000.003", etc.
//Starting the hash on the right gives the differences time to cascade out.
//************************************************************************************
inline void StringHash_InitializeScanDirection
(int variation, int len, int& start, int& end, int& incr)
{
	if (variation & 1) {
		start = len - 1;
		end = -1;
		incr = -1;
	}
	else {
		start = 0;
		end = len;
		incr = 1;
	}
}

//************************************************************************************
//Variation 2 is to do a further integer "randomizing" operation on the result to
//get results more widely distributed across the integer range.  Some of the hash
//algorithms rarely produce low result values for typical input values (alphabetic
//characters), so while this tweak is something of an illusion in that it doesn't 
//increase the number of discrete integer return values, it might often be useful 
//since a hash table will not have INT_MAX cells but many less, and the user might
//code their cell-choice routine as: Cell = (hash value / (INT_MAX / num cells) ).
//************************************************************************************
inline int StringHash_IntegerRandomizeTweak(int i) 
{
	return RNG_PMSBDS_MT::Cycle(i);
}

//Shared code to return value
inline int StringHash_ReturnValue(unsigned int ui, int variation)
{
	int i = ui & 0x7FFFFFFF;

	if (variation & 2)
		return StringHash_IntegerRandomizeTweak(i);
	else
		return i;
}






//********************************************************************************
//2. Hash functions returning integer digest:
//********************************************************************************

//********************************************************************************
//Seem to be very few low numbers from this
//********************************************************************************
unsigned int StringHash_JS(const char* str, int len, int variation)
{
	if (len == -1)
		len = strlen(str);

	unsigned int hash = 1315423911;

	int start, end, incr;
	StringHash_InitializeScanDirection(variation, len, start, end, incr);

	for (int i = start; i != end; i += incr) 
		hash ^= ((hash << 5) + str[i] + (hash >> 2));

	return StringHash_ReturnValue(hash, variation);
}

//********************************************************************************
//Output values rarely seem to go into the high order 4 or 5 bits.
//********************************************************************************
unsigned int StringHash_ELF(const char* str, int len, int variation)
{
	if (len == -1)
		len = strlen(str);

	unsigned int hash = 0;
	unsigned int x    = 0;

	int start, end, incr;
	StringHash_InitializeScanDirection(variation, len, start, end, incr);

	for (int i = start; i != end; i += incr) {
		hash = (hash << 4) + str[i];

		if ( (x = hash & 0xF0000000L) != 0) {
			hash ^= (x >> 24);
			hash &= ~x;
		}
	}

	return StringHash_ReturnValue(hash, variation);
}

//********************************************************************************
//Not really tested
//********************************************************************************
unsigned int StringHash_SDBM(const char* str, int len, int variation)
{
	unsigned int hash = 0;

	int start, end, incr;
	StringHash_InitializeScanDirection(variation, len, start, end, incr);

	for (int i = start; i != end; i += incr) 
		hash = str[i] + (hash << 6) + (hash << 16) - hash;

	return StringHash_ReturnValue(hash, variation);
}


//********************************************************************************
//Not really tested
//********************************************************************************
unsigned int StringHash_DJB(const char* str, int len, int variation)
{
	unsigned int hash = 5381;

	int start, end, incr;
	StringHash_InitializeScanDirection(variation, len, start, end, incr);

	for (int i = start; i != end; i += incr) 
		hash = ((hash << 5) + hash) + str[i];

	return StringHash_ReturnValue(hash, variation);
}


//********************************************************************************
//Not really tested
//********************************************************************************
unsigned int StringHash_DEK(const char* str, int len, int variation)
{
	unsigned int hash = static_cast<unsigned int>(len);

	int start, end, incr;
	StringHash_InitializeScanDirection(variation, len, start, end, incr);

	for (int i = start; i != end; i += incr) 
		hash = ((hash << 5) ^ (hash >> 27)) ^ str[i];

	return StringHash_ReturnValue(hash, variation);
}

//********************************************************************************
//Not really tested
//********************************************************************************
unsigned int StringHash_AP(const char* str, int len, int variation)
{
	unsigned int hash = 0;

	int start, end, incr;
	StringHash_InitializeScanDirection(variation, len, start, end, incr);

	for (int i = start; i != end; i += incr) 
		hash ^= ((i & 1) == 0) ? (  (hash <<  7) ^ str[i] ^ (hash >> 3)) :
									(~((hash << 11) ^ str[i] ^ (hash >> 5)));

	return StringHash_ReturnValue(hash, variation);
}


#endif





//********************************************************************************
//3. Hash functions returning string digest:
//********************************************************************************



//********************************************************************************
//SHA1 implementation from Paul E Jones.  (Tidied up a lot of meaningless comments
//from his code and removed useless interfaces, or should I say interfaces I don't
//like!)
//
//My wrapper object (see hash.h) uses this to do the hashing.
//********************************************************************************

/*
 *	sha1.h
 *
 *	Copyright (C) 1998
 *	Paul E. Jones <paulej@arid.us>
 *	All Rights Reserved.
 *
 *****************************************************************************
 *
 *	Description:
 * 		This class implements the Secure Hashing Standard as defined
 * 		in FIPS PUB 180-1 published April 17, 1995.
 *
 *		Many of the variable names in this class, especially the single
 *		character names, were used because those were the names used
 *		in the publication.
 *
 * 		Please read the file sha1.cpp for more information.
 *
 */



//********************************************************************************
//SHA1 test data as documented in FIPS PUB 180-1.
//
//NB.  The 5 blocks shown are each little-endian 32 bit integers, so will look
//different if interpreted as a string.
//
//"abc"
//"A9993E36 4706816A BA3E2571 7850C26C 9CD0D89D"
//"363E99A96A81064771253EBA6CC250789DD8D09C" (as hex string - see above comment)
//
//"abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"
//"84983E44 1C3BD26E BAAE4AA1 F95129E5 E54670F1"
//"443E98846ED23B1CA14AAEBAE52951F9F17046E5" (as hex string - see above comment)
//
//One million 'a' characters
//"34AA973C D4C4DAA4 F61EEB2B DBAD2731 6534016F"
//"3C97AA34A4DAC4D42BEB1EF63127ADDB6F013465" (as hex string - see above comment)
//********************************************************************************

class SHA1 {

	void ProcessMessageBlock();
	void PadMessage();
	inline unsigned CircularShift(int bits, unsigned word);

	unsigned H[5];						// Message digest buffers

	unsigned Length_Low;				// Message length in bits
	unsigned Length_High;				// Message length in bits

	unsigned char Message_Block[64];	// 512-bit message blocks
	int Message_Block_Index;			// Index into message block array

	bool Computed;						// Is the digest computed?
	bool Corrupted;						// Is the message digest corruped?

public:
	SHA1() {Reset();}
	void Reset();

	void Input(const unsigned char*, unsigned int);
	void Input(const char* c, unsigned int l) {Input( (const unsigned char*) c, l);}

	bool ComputeResult(unsigned int*);
};


//********************************************************************************
/*
 *	sha1.cpp
 *
 *	Copyright (C) 1998
 *	Paul E. Jones <paulej@arid.us>
 *	All Rights Reserved.
 *
 *****************************************************************************
 *
 *	Description:
 * 		This class implements the Secure Hashing Standard as defined
 * 		in FIPS PUB 180-1 published April 17, 1995.
 *
 * 		The Secure Hashing Standard, which uses the Secure Hashing
 * 		Algorithm (SHA), produces a 160-bit message digest for a
 * 		given data stream.  In theory, it is highly improbable that
 * 		two messages will produce the same message digest.  Therefore,
 * 		this algorithm can serve as a means of providing a "fingerprint"
 * 		for a message.
 *
 *	Portability Issues:
 * 		SHA-1 is defined in terms of 32-bit "words".  This code was
 * 		written with the expectation that the processor has at least
 * 		a 32-bit machine word size.  If the machine word size is larger,
 * 		the code should still function properly.  One caveat to that
 *		is that the input functions taking characters and character arrays
 *		assume that only 8 bits of information are stored in each character.
 *
 *	Caveats:
 * 		SHA-1 is designed to work with messages less than 2^64 bits long.
 * 		Although SHA-1 allows a message digest to be generated for
 * 		messages of any number of bits less than 2^64, this implementation
 * 		only works with messages with a length that is a multiple of 8
 * 		bits.
 *
 */



//********************************************************************************
//Initialize member variables in preparation for computing a new message digest.
void SHA1::Reset() 
{
	Length_Low			= 0;
	Length_High			= 0;
	Message_Block_Index	= 0;

	H[0]		= 0x67452301;
	H[1]		= 0xEFCDAB89;
	H[2]		= 0x98BADCFE;
	H[3]		= 0x10325476;
	H[4]		= 0xC3D2E1F0;

	Computed	= false;
	Corrupted	= false;
}

//********************************************************************************
//Return the 160-bit message digest into the array provided.
bool SHA1::ComputeResult(unsigned int* message_digest_array)
{
	if (Corrupted)
		return false;

	if (!Computed) {
		PadMessage();
		Computed = true;
	}

	for(int i = 0; i < 5; i++)
		message_digest_array[i] = H[i];

	return true;
}

//********************************************************************************
//Accepts an array of octets as the next portion of the message.
void SHA1::Input(const unsigned char* message_array, unsigned int length)
{
	if (!length)
		return;

	if (Computed || Corrupted) {
		Corrupted = true;
		return;
	}

	while (length-- && !Corrupted) {

		Message_Block[Message_Block_Index++] = (*message_array & 0xFF);

		Length_Low += 8;
		Length_Low &= 0xFFFFFFFF;				// Force it to 32 bits
		if (Length_Low == 0)
		{
			Length_High++;
			Length_High &= 0xFFFFFFFF;			// Force it to 32 bits
			if (Length_High == 0)
				Corrupted = true;				// Message is too long
		}

		if (Message_Block_Index == 64)
			ProcessMessageBlock();

		message_array++;
	}
}


//********************************************************************************
//Process the next 512 bits of the message stored in the Message_Block array.
//NB.
//Many of the variable names in this function, especially the single
//character names, were used because those were the names used
//in the publication.
//********************************************************************************
void SHA1::ProcessMessageBlock()
{
	const unsigned K[] = 	{ 				// Constants defined for SHA-1
								0x5A827999,
								0x6ED9EBA1,
								0x8F1BBCDC,
								0xCA62C1D6
							};
	int 		t;							// Loop counter
	unsigned 	temp;						// Temporary word value
	unsigned	W[80];						// Word sequence
	unsigned	A, B, C, D, E;				// Word buffers

	/*
	 *	Initialize the first 16 words in the array W
	 */
	for(t = 0; t < 16; t++)
	{
		W[t] = ((unsigned) Message_Block[t * 4]) << 24;
		W[t] |= ((unsigned) Message_Block[t * 4 + 1]) << 16;
		W[t] |= ((unsigned) Message_Block[t * 4 + 2]) << 8;
		W[t] |= ((unsigned) Message_Block[t * 4 + 3]);
	}

	for(t = 16; t < 80; t++)
	{
	   W[t] = CircularShift(1,W[t-3] ^ W[t-8] ^ W[t-14] ^ W[t-16]);
	}

	A = H[0];
	B = H[1];
	C = H[2];
	D = H[3];
	E = H[4];

	for(t = 0; t < 20; t++)
	{
		temp = CircularShift(5,A) + ((B & C) | ((~B) & D)) + E + W[t] + K[0];
		temp &= 0xFFFFFFFF;
		E = D;
		D = C;
		C = CircularShift(30,B);
		B = A;
		A = temp;
	}

	for(t = 20; t < 40; t++)
	{
		temp = CircularShift(5,A) + (B ^ C ^ D) + E + W[t] + K[1];
		temp &= 0xFFFFFFFF;
		E = D;
		D = C;
		C = CircularShift(30,B);
		B = A;
		A = temp;
	}

	for(t = 40; t < 60; t++)
	{
		temp = CircularShift(5,A) +
		 	   ((B & C) | (B & D) | (C & D)) + E + W[t] + K[2];
		temp &= 0xFFFFFFFF;
		E = D;
		D = C;
		C = CircularShift(30,B);
		B = A;
		A = temp;
	}

	for(t = 60; t < 80; t++)
	{
		temp = CircularShift(5,A) + (B ^ C ^ D) + E + W[t] + K[3];
		temp &= 0xFFFFFFFF;
		E = D;
		D = C;
		C = CircularShift(30,B);
		B = A;
		A = temp;
	}

	H[0] = (H[0] + A) & 0xFFFFFFFF;
	H[1] = (H[1] + B) & 0xFFFFFFFF;
	H[2] = (H[2] + C) & 0xFFFFFFFF;
	H[3] = (H[3] + D) & 0xFFFFFFFF;
	H[4] = (H[4] + E) & 0xFFFFFFFF;

	Message_Block_Index = 0;
}

//********************************************************************************
//According to the standard, the message must be padded to an even
//512 bits.  The first padding bit must be a '1'.  The last 64 bits
//represent the length of the original message.  All bits in between
//should be 0.  This function will pad the message according to those
//rules by filling the message_block array accordingly.  It will also
//call ProcessMessageBlock() appropriately.  When it returns, it
//can be assumed that the message digest has been computed.
//********************************************************************************
void SHA1::PadMessage()
{
	/*
	 *	Check to see if the current message block is too small to hold
	 *	the initial padding bits and length.  If so, we will pad the
	 *	block, process it, and then continue padding into a second block.
	 */
	if (Message_Block_Index > 55)
	{
		Message_Block[Message_Block_Index++] = 0x80;
		while(Message_Block_Index < 64)
		{
			Message_Block[Message_Block_Index++] = 0;
		}

		ProcessMessageBlock();

		while(Message_Block_Index < 56)
		{
			Message_Block[Message_Block_Index++] = 0;
		}
	}
	else
	{
		Message_Block[Message_Block_Index++] = 0x80;
		while(Message_Block_Index < 56)
		{
			Message_Block[Message_Block_Index++] = 0;
		}

	}

	/*
	 *	Store the message length as the last 8 octets
	 */
	Message_Block[56] = (Length_High >> 24) & 0xFF;
	Message_Block[57] = (Length_High >> 16) & 0xFF;
	Message_Block[58] = (Length_High >> 8) & 0xFF;
	Message_Block[59] = (Length_High) & 0xFF;
	Message_Block[60] = (Length_Low >> 24) & 0xFF;
	Message_Block[61] = (Length_Low >> 16) & 0xFF;
	Message_Block[62] = (Length_Low >> 8) & 0xFF;
	Message_Block[63] = (Length_Low) & 0xFF;

	ProcessMessageBlock();
}


//********************************************************************************
//Perform a circular shifting operation.
unsigned SHA1::CircularShift(int bits, unsigned word)
{
	return ((word << bits) & 0xFFFFFFFF) | ((word & 0xFFFFFFFF) >> (32-bits));
}





//********************************************************************************
//Wrapper class using the above code
//********************************************************************************

const unsigned int Hash_SHA1::DIGEST_LENGTH = 20;

//*****************************
std::string Hash_SHA1::Perform(const char* data, unsigned int len)
{
	char buff[DIGEST_LENGTH];
	Perform(buff, data, len);

	return std::string(buff, DIGEST_LENGTH);
}

//*****************************
void Hash_SHA1::Perform(char* buff, const char* data, unsigned int len)
{
	SHA1 hasher;
	hasher.Input(data, len);

	if (hasher.ComputeResult(reinterpret_cast<unsigned int*>(buff)))
		return;

	throw Exception(UTIL_MISC_ALGORITHM_ERROR, "Error computing SHA1 hash");
}


} //close namespace
