
#include "stdafx.h"

#include "bitmap3.h"
#include "windows.h" //for heap functions

namespace dpt { namespace util {

unsigned long BitMap::onebit = 1;
unsigned long BitMap::nobits = 0;
unsigned long BitMap::allbits = ULONG_MAX;

//****************************************************************************************
void BitMap::PrivateHeapAllocate()
{
	HANDLE* h = static_cast<HANDLE*>(heap);
	void* ptr = HeapAlloc(*h, HEAP_GENERATE_EXCEPTIONS, numbits*8);
	data = static_cast<unsigned long*>(ptr);
}

void BitMap::PrivateHeapFree()
{
	HANDLE* h = static_cast<HANDLE*>(heap);
	HeapFree(*h, HEAP_GENERATE_EXCEPTIONS, data);
}

//****************************************************************************************
BitMap& BitMap::AdoptDataFrom(const BitMap& b2)
{
	if (own_memory && data != stack_data)
		//delete[] data; //Aug 2010.
		DestroyData();

	own_memory = b2.own_memory;
	heap = b2.heap; //Aug 2010

	//7/7/05 Slight complication - can't adopt b2's stack data, so copy it
	if (b2.own_memory && b2.data == b2.stack_data) {
		data = stack_data;
		CopyInformationFrom(b2);
		CopyDataFrom(b2);
	}
	else {
		data = b2.data;
		CopyInformationFrom(b2);
	}

	if (b2.own_memory) {
		//23/07/07 V2.05.  I'd rather not make all these mutable unnecessarily.  The 
		//assignment and copy constructor have to be used with care anyway.
		BitMap& ncb2 = const_cast<BitMap&>(b2);
		ncb2.numbits = 0;
		ncb2.data = NULL;
		ncb2.heap = NULL;
		ncb2.cached_count_flag = false;
		ncb2.own_memory = false;
	}

	return (*this);
}

//****************************************************************************************
BitMap& BitMap::EnsureCapacity(size_t newbits, bool newclean)
{
	if (!own_memory)
		throw Exception(UTIL_BITMAP_ERROR, 
			"BitMap::EnsureCapacity() is only valid when the object is used as a container");

	//Reserving down has no effect (if changing this the code below will need work)
	if (newbits <= numbits)
		return *this;

	//The main use of this function is to gradually increase the size of a bitmap,
	//doubling is a good approach.
	if (newbits < numbits * 2)
		newbits = numbits * 2;

	size_t new_words = newbits/32;
	if (newbits % 32) {
		new_words++;
		newbits = new_words*32;
	}
	
	//5/7/05: Tuned to allow small amount of stack container space
	if (newbits > BBBM_MAX_STACK_BITS) {

		//Allocate the new area
		unsigned long* newarea = new unsigned long[new_words];
		//Copy the existing data over first portion of it
		memcpy((void*)newarea, (void*)data, wholewords*4);    

		//If we had heap space before, delete it
		if (data != stack_data)
			//delete[] data; //Aug 2010.
			DestroyData();

		data = newarea;
	}

	//Clean any new memory if requested
	if (newclean) {
		unsigned long* freshbase = data+wholewords;
		int freshsize = (new_words-wholewords)*4;
		memset(freshbase, 0, freshsize);
	}

	wholewords = new_words;
	numbits = newbits;

	return *this;
}





//****************************************************************************************
//General info
//****************************************************************************************
size_t BitMap::Count() const
{
	if (cached_count_flag)
		return cached_count_value;

	size_t result = 0;

	//May 09. Couldn't resist doing this - nearly halves cost of counting a big bitmap
	//(one with plenty of bits set that is - much less effect on sparse maps).  Holding 
	//65536 precomputed counts (16 bits at a time) would be unweildy here.
//	static unsigned long bitcounts[16] = {0,1,1,2,1,2,2,3,1,2,2,3,2,3,3,4};
	static unsigned long bitcounts[256] = { 0,1,1,2,1,2,2,3,1,2,2,3,2,3,3,4, 
											1,2,2,3,2,3,3,4,2,3,3,4,3,4,4,5,
											1,2,2,3,2,3,3,4,2,3,3,4,3,4,4,5,
											2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,
											1,2,2,3,2,3,3,4,2,3,3,4,3,4,4,5,
											2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,
											2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,
											3,4,4,5,4,5,5,6,4,5,5,6,5,6,6,7,
											1,2,2,3,2,3,3,4,2,3,3,4,3,4,4,5,
											2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,
											2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,
											3,4,4,5,4,5,5,6,4,5,5,6,5,6,6,7,
											2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,
											3,4,4,5,4,5,5,6,4,5,5,6,5,6,6,7,
											3,4,4,5,4,5,5,6,4,5,5,6,5,6,6,7,
											4,5,5,6,5,6,6,7,5,6,6,7,6,7,7,8};

	unsigned long* word = data;
	unsigned long* pastend = &data[wholewords];
	for (; word < pastend; word++) {

		//Bite off chunks of bits and match to the above precomputed counts
		for (unsigned int w = *word; w != 0; w >>= 8) {
			result += bitcounts[w & 0xFF];
		}
	}

	cached_count_flag = true;
	cached_count_value = result;

	return result; 
}

//****************************************************************************************
bool BitMap::All() const 
{
	if (cached_count_flag)
		return (cached_count_value == numbits) ? true : false;

	unsigned long* word = data;
	unsigned long* pastend = &data[wholewords];

	for (; word < pastend; word++) {
		if (*word != ULONG_MAX) 
			return false;
	}
	
	return true;
}

//****************************************************************************************
bool BitMap::Any() const 
{
	if (cached_count_flag)
		return (cached_count_value > 0) ? true : false;

	unsigned long* word = data;
	unsigned long* pastend = &data[wholewords];

	for (; word < pastend; word++) {
		if (*word != 0) 
			return true;
	}

	return false;
}

//****************************************************************************************
bool BitMap::FindNext(size_t& result, size_t from, size_t to, bool bittype) const
{
	if (to == UINT_MAX)
		to = numbits - 1;

	RangeCheck(from);
	RangeCheck(to);

	if (from > to)
		return false;

	//Test any overhanging bits at the start of the range one at a time.  This is a little
	//neater than trying to use masking, and in any case per-bit testing will be required 
	//in the "middle" section later, so using it here allows the recursive call there.
	size_t first_word = from / 32;
	size_t last_bit_in_first_word = first_word * 32 + 31;

	if (last_bit_in_first_word > to)
		last_bit_in_first_word = to;

	size_t x;
	for (x = from; x <= last_bit_in_first_word; x++) {
		if (Test(x) == bittype) {
			result = x;
			return true;
		}
	}

	if (last_bit_in_first_word == to)
		return false;

	//Screen whole aligned words in the range a word at a time
	size_t last_word = to / 32;

	if (last_word - first_word > 1) {
		unsigned int nomatch = (bittype) ? nobits : allbits;

		for (size_t x = first_word+1; x < last_word; x++) {
			if (data[x] != nomatch) {
				//When we find a word with at least one of the desired bit, make a 
				//recursive call to test the bits one by one.
				size_t wordbit0 = x * 32;
				return FindNext(result, wordbit0, wordbit0 + 31, bittype); 
			}
		}
	}
	
	//Finally test overhanging bits at the end of the range one at a time	
	size_t first_bit_in_last_word = last_word * 32;

	for (x = first_bit_in_last_word; x <= to; x++) {
		if (Test(x) == bittype) {
			result = x;
			return true;
		}
	}

	//Matching bit was not found
	return false;
}

//****************************************************************************************
//The code is much clearer if we just have a separate function.  See above for comments.
//****************************************************************************************
bool BitMap::FindPrev(size_t& result, size_t from, size_t to, bool bittype) const
{
	if (from == UINT_MAX)
		from = numbits - 1;

	RangeCheck(from);
	RangeCheck(to);

	if (from < to)
		return false;

	size_t first_word = from / 32;
	size_t last_bit_in_first_word = first_word * 32;

	if (last_bit_in_first_word < to)
		last_bit_in_first_word = to;

	size_t x;
	for (x = from; x >= last_bit_in_first_word; x--) {
		if (x == UINT_MAX)
			break;

		if (Test(x) == bittype) {
			result = x;
			return true;
		}
	}

	if (last_bit_in_first_word == to)
		return false;

	size_t last_word = to / 32;

	if (first_word - last_word > 1) {
		unsigned int nomatch = (bittype) ? nobits : allbits;

		for (size_t x = first_word-1; x > last_word; x--) {
			if (data[x] != nomatch) {
				size_t wordbit0 = x * 32;
				return FindPrev(result, wordbit0 + 31, wordbit0, bittype); 
			}
		}
	}
	
	//Finally test overhanging bits at the end of the range one at a time	
	size_t first_bit_in_last_word = last_word * 32 + 31;

	for (x = first_bit_in_last_word; x >= to; x--) {
		if (x == UINT_MAX)
			break;

		if (Test(x) == bittype) {
			result = x;
			return true;
		}
	}

	//Matching bit was not found
	return false;
}

//****************************************************************************************
std::string BitMap::ToString()
{
	std::string result;
	result.reserve(numbits);

	//Pull it off in reverse order
	for (int x = numbits - 1; x >= 0; --x)
		result.append(1, (Test(x) ? '1' : '0'));

	return result; 
}






//****************************************************************************************
//Modifications
//****************************************************************************************
BitMap& BitMap::SetRange(size_t from, size_t to, bool b)
{
	RangeCheck(from);
	RangeCheck(to);

	//Since there could be confusion about behaviour if from > to, it's not allowed
	if (from > to)
		throw Exception(UTIL_BITMAP_ERROR, 
			"BitMap::SetRange(): The 'from' point must not be after the 'to' point");

	cached_count_flag = false;

	//First deal with non-wholeword overhangs 
	size_t first_word = from / 32;
	unsigned int first_word_mask = allbits >> (from % 32);

	size_t last_word = to / 32;
	unsigned int last_word_mask = allbits << (31 - (to % 32));

	if (first_word == last_word)
		first_word_mask = last_word_mask = (first_word_mask & last_word_mask);

	if (b) {
		data[first_word] |= first_word_mask;
		data[last_word] |= last_word_mask;
	}
	else {
		data[first_word] &= ~first_word_mask;
		data[last_word] &= ~last_word_mask;
	}

	//Finally do anything in between wholesale
	if (last_word - first_word > 1) {
		unsigned int val = b ? allbits : nobits;
		for (size_t x = first_word+1; x < last_word; x++)
			data[x] = val;
	}

	return *this;
}

//****************************************************************************************
BitMap& BitMap::Flip(size_t offset)
{
	RangeCheck(offset);

	data[offset / 32] ^= (onebit << (31 - (offset % 32)));

	//Easy to preserve the count here, but would entail testing the bit above too.
	//At the end of the day I don't think flipping single bits will be common.
	cached_count_flag = false;
	return *this;
}

//****************************************************************************************
BitMap& BitMap::FlipAll()
{
	unsigned long* word = data;
	unsigned long* pastend = &data[wholewords];

	for (; word < pastend; word++)
		*word = ~(*word);
	
	//See comment above.  Flipping entire bitmaps is much more common, and luckily
	//preserving the count is straightforward in this case.

	if (cached_count_flag)
		cached_count_value = numbits - cached_count_value;

	return (*this); 
}



//****************************************************************************************
//Comparisons
//****************************************************************************************
bool BitMap::AnyIntersection(const BitMap& rhs) const 
{
	SizeEqualityCheck(numbits, rhs.numbits);

	unsigned long* word = data;
	unsigned long* pastend = &data[wholewords];
	unsigned long* rword = rhs.data;

	for (; word < pastend; word++, rword++) {
		if (*word & *rword) 
			return true;
	}

	return false;
}

//****************************************************************************************
bool BitMap::operator==(const BitMap& rhs) const 
{
	SizeEqualityCheck(numbits, rhs.numbits);

	unsigned long* word = data;
	unsigned long* pastend = &data[wholewords];
	unsigned long* rword = rhs.data;

	for (; word < pastend; word++, rword++) {
		if (*word != *rword) 
			return false;
	}
	
	return true;
}





//****************************************************************************************
//Bitwise set/set modifiers
//****************************************************************************************
BitMap& BitMap::operator|=(const BitMap& rhs) 
{
	//The RHS can be shorter
	if (numbits < rhs.numbits)
		SizeMismatch();

	cached_count_flag = false;

	unsigned long* word = data;
	unsigned long* pastend = &data[rhs.wholewords]; //i.e. only do to end of rhs map
	unsigned long* rword = rhs.data;
	
	for (; word < pastend; word++, rword++)
		*word |= *rword;

	return (*this);
}

//****************************************************************************************
BitMap& BitMap::operator&=(const BitMap& rhs) 
{
	//The RHS can be shorter
	if (numbits < rhs.numbits)
		SizeMismatch();

	cached_count_flag = false;

	unsigned long* word = data;
	unsigned long* pastend = &data[rhs.wholewords]; //i.e. only do to end of rhs map
	unsigned long* rword = rhs.data;
	
	for (; word < pastend; word++, rword++)
		//See comment in /= below - even more marginal here
		*word &= *rword;
	
	return (*this);
}

//****************************************************************************************
BitMap& BitMap::operator/=(const BitMap& rhs) 
{
	//The RHS can be shorter
	if (numbits < rhs.numbits)
		SizeMismatch();

	cached_count_flag = false;

	unsigned long* word = data;
	unsigned long* pastend = &data[rhs.wholewords]; //i.e. only do to end of rhs map
	unsigned long* rword = rhs.data;

	for (; word < pastend; word++, rword++)
		//Thought about special-casing zero lhs in this loop, but on instruction counts
		//it would only pay if on average approximately 80% of words were zero. 
		*word &= ~(*rword);
	
	return (*this);
}




//****************************************************************************************
//Shifters. 
//Note that the way I've implemented things, a left shift is actually implemented as
//a C style right shift.
//****************************************************************************************
BitMap& BitMap::operator<<=(size_t distance)
{
	cached_count_flag = false;

	//Number of whole words to shift by.  
	int wordshift = distance / 32;
	if (wordshift != 0)
		for (int x = wholewords - 1; x >= 0 ; --x)
			data[x] = (x >= wordshift) ? data[x - wordshift] : 0;

	//The shift bits across word boundaries
	if ((distance %= 32) != 0) {
		for (int x = wholewords - 1; x > 0 ; --x)
			data[x] = (data[x] >> distance) | (data[x-1] << (32 - distance));

		//And the final word
		data[0] >>= distance;
	}

	return (*this); 
}

//****************************************************************************************
//Analogous comment to the above.
BitMap& BitMap::operator>>=(size_t distance)
{
	cached_count_flag = false;

	//Number of whole words to shift by.  
	size_t wordshift = distance / 32;
	if (wordshift != 0)
		for (size_t x = 0; x < wholewords ; ++x)
			data[x] = ((wholewords-x) >= wordshift) ? data[x + wordshift] : 0;

	//The shift bits across word boundaries
	if ((distance %= 32) != 0) {
		for (size_t x = 0; x < wholewords - 1; ++x)
			data[x] = (data[x] << distance) | (data[x+1] >> (32 - distance));

		//And the final word
		data[wholewords-1] <<= distance;
	}

	return (*this); 
}

}} //close namespace
