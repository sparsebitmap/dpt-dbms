
#include "stdafx.h"

#include "bitmap3.h"
#include "windows.h" //for heap functions

namespace dpt {
	namespace util {

		uint32_t BitMap::onebit = 1;
		uint32_t BitMap::nobits = 0;
		uint32_t BitMap::allbits = 0xFFFFFFFF;

		//****************************************************************************************
		void BitMap::PrivateHeapAllocate()
		{
			HANDLE* h = static_cast<HANDLE*>(heap);
			void* ptr = HeapAlloc(*h, HEAP_GENERATE_EXCEPTIONS, numbits * 8);
			data = static_cast<uint32_t*>(ptr);
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
		BitMap& BitMap::EnsureCapacity(uint32_t newbits, bool newclean)
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

			uint32_t new_words = newbits / 32;
			if (newbits % 32) {
				new_words++;
				newbits = new_words * 32;
			}

			//5/7/05: Tuned to allow small amount of stack container space
			if (newbits > BBBM_MAX_STACK_BITS) {

				//Allocate the new area
				uint32_t* newarea = new uint32_t[new_words];
				//Copy the existing data over first portion of it
				memcpy((void*)newarea, (void*)data, wholewords * 4);

				//If we had heap space before, delete it
				if (data != stack_data)
					//delete[] data; //Aug 2010.
					DestroyData();

				data = newarea;
			}

			//Clean any new memory if requested
			if (newclean) {
				uint32_t* freshbase = data + wholewords;
				uint32_t freshsize = (new_words - wholewords) * 4;
				memset(freshbase, 0, freshsize);
			}

			wholewords = new_words;
			numbits = newbits;

			return *this;
		}





		//****************************************************************************************
		//General info
		//****************************************************************************************
		uint32_t BitMap::Count() const
		{
			if (cached_count_flag)
				return cached_count_value;

			uint32_t result = 0;

			//May 09. Couldn't resist doing this - nearly halves cost of counting a big bitmap
			//(one with plenty of bits set that is - much less effect on sparse maps).  Holding 
			//65536 precomputed counts (16 bits at a time) would be unweildy here.
		//	static unsigned long bitcounts[16] = {0,1,1,2,1,2,2,3,1,2,2,3,2,3,3,4};
			static uint32_t bitcounts[256] = { 0,1,1,2,1,2,2,3,1,2,2,3,2,3,3,4,
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
													4,5,5,6,5,6,6,7,5,6,6,7,6,7,7,8 };

			uint32_t* word = data;
			uint32_t* pastend = &data[wholewords];
			for (; word < pastend; word++) {

				//Bite off chunks of bits and match to the above precomputed counts
				for (uint32_t w = *word; w != 0; w >>= 8) {
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

			uint32_t* word = data;
			uint32_t* pastend = &data[wholewords];

			for (; word < pastend; word++) {
				if (*word != allbits)
					return false;
			}

			return true;
		}

		//****************************************************************************************
		bool BitMap::Any() const
		{
			if (cached_count_flag)
				return (cached_count_value > 0) ? true : false;

			uint32_t* word = data;
			uint32_t* pastend = &data[wholewords];

			for (; word < pastend; word++) {
				if (*word != 0)
					return true;
			}

			return false;
		}

		//****************************************************************************************
		bool BitMap::FindNext(uint32_t& result, uint32_t from, uint32_t to, bool bittype) const
		{
			if (to == allbits)
				to = numbits - 1;

			RangeCheck(from);
			RangeCheck(to);

			if (from > to)
				return false;

			//Test any overhanging bits at the start of the range one at a time.  This is a little
			//neater than trying to use masking, and in any case per-bit testing will be required 
			//in the "middle" section later, so using it here allows the recursive call there.
			uint32_t first_word = from / 32;
			uint32_t last_bit_in_first_word = first_word * 32 + 31;

			if (last_bit_in_first_word > to)
				last_bit_in_first_word = to;

			uint32_t x;
			for (x = from; x <= last_bit_in_first_word; x++) {
				if (Test(x) == bittype) {
					result = x;
					return true;
				}
			}

			if (last_bit_in_first_word == to)
				return false;

			//Screen whole aligned words in the range a word at a time
			uint32_t last_word = to / 32;

			if (last_word - first_word > 1) {
				uint32_t nomatch = (bittype) ? nobits : allbits;

				for (uint32_t x = first_word + 1; x < last_word; x++) {
					if (data[x] != nomatch) {
						//When we find a word with at least one of the desired bit, make a 
						//recursive call to test the bits one by one.
						uint32_t wordbit0 = x * 32;
						return FindNext(result, wordbit0, wordbit0 + 31, bittype);
					}
				}
			}

			//Finally test overhanging bits at the end of the range one at a time	
			uint32_t first_bit_in_last_word = last_word * 32;

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
		bool BitMap::FindPrev(uint32_t& result, uint32_t from, uint32_t to, bool bittype) const
		{
			if (from == allbits)
				from = numbits - 1;

			RangeCheck(from);
			RangeCheck(to);

			if (from < to)
				return false;

			uint32_t first_word = from / 32;
			uint32_t last_bit_in_first_word = first_word * 32;

			if (last_bit_in_first_word < to)
				last_bit_in_first_word = to;

			uint32_t x;
			for (x = from; x >= last_bit_in_first_word; x--) {
				if (x == allbits)
					break;

				if (Test(x) == bittype) {
					result = x;
					return true;
				}
			}

			if (last_bit_in_first_word == to)
				return false;

			uint32_t last_word = to / 32;

			if (first_word - last_word > 1) {
				uint32_t nomatch = (bittype) ? nobits : allbits;

				for (uint32_t x = first_word - 1; x > last_word; x--) {
					if (data[x] != nomatch) {
						uint32_t wordbit0 = x * 32;
						return FindPrev(result, wordbit0 + 31, wordbit0, bittype);
					}
				}
			}

			//Finally test overhanging bits at the end of the range one at a time	
			uint32_t first_bit_in_last_word = last_word * 32 + 31;

			for (x = first_bit_in_last_word; x >= to; x--) {
				if (x == allbits)
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
		std::string BitMap::ToStringAsBinaryNumber() const
		{
			std::string result;
			result.reserve(numbits);

			uint32_t x = numbits - 1;
			do {
				result.append(1, (Test(x) ? '1' : '0'));
			} while (x-- > 0);

			return result;
		}

		std::string BitMap::ToStringAsBitMap() const
		{
			std::string result;
			result.reserve(numbits);

			for (uint32_t x = 0; x < numbits; x++)
				result.append(1, (Test(x) ? '1' : '0'));

			return result;
		}




		//****************************************************************************************
		//Modifications
		//****************************************************************************************
		BitMap& BitMap::SetRange(uint32_t from, uint32_t to, bool b)
		{
			RangeCheck(from);
			RangeCheck(to);

			//Since there could be confusion about behaviour if from > to, it's not allowed
			if (from > to)
				throw Exception(UTIL_BITMAP_ERROR,
					"BitMap::SetRange(): The 'from' point must not be after the 'to' point");

			cached_count_flag = false;

			//First deal with non-wholeword overhangs 
			uint32_t first_word = from / 32;
			uint32_t first_word_mask = allbits >> (from % 32);

			uint32_t last_word = to / 32;
			uint32_t last_word_mask = allbits << (31 - (to % 32));

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
				uint32_t val = b ? allbits : nobits;
				for (uint32_t x = first_word + 1; x < last_word; x++)
					data[x] = val;
			}

			return *this;
		}

		//****************************************************************************************
		BitMap& BitMap::Flip(uint32_t offset)
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
			uint32_t* word = data;
			uint32_t* pastend = &data[wholewords];

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

			uint32_t* word = data;
			uint32_t* pastend = &data[wholewords];
			uint32_t* rword = rhs.data;

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

			uint32_t* word = data;
			uint32_t* pastend = &data[wholewords];
			uint32_t* rword = rhs.data;

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

			uint32_t* word = data;
			uint32_t* pastend = &data[rhs.wholewords]; //i.e. only do to end of rhs map
			uint32_t* rword = rhs.data;

			for (; word < pastend; word++, rword++) {
				*word |= *rword;
			}

			return (*this);
		}

		//****************************************************************************************
		BitMap& BitMap::operator&=(const BitMap& rhs)
		{
			//The RHS can be shorter
			if (numbits < rhs.numbits)
				SizeMismatch();

			cached_count_flag = false;

			uint32_t* word = data;
			uint32_t* pastend = &data[rhs.wholewords]; //i.e. only do to end of rhs map
			uint32_t* rword = rhs.data;

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

			uint32_t* word = data;
			uint32_t* pastend = &data[rhs.wholewords]; //i.e. only do to end of rhs map
			uint32_t* rword = rhs.data;

			for (; word < pastend; word++, rword++)
				//Thought about special-casing zero lhs in this loop, but on instruction counts
				//it would only pay if on average approximately 80% of words were zero. 
				*word &= ~(*rword);

			return (*this);
		}




		//****************************************************************************************
		//Shifters. 
		//Note on data representation here, which might seem a little bit counterintuitive when it
		//comes to implementing the << and >> operators.
		//- "Bit 0" is visualized as the "lowest" bit at the left with "higher" bits to the right
		//- Word 0 in the backing int array is considered the leftmost word
		//The meaning of these two operators here is:
		//- << "move bits higher"
		//- >> "move bits lower"
		//This is kind of logically consistent to what C++ >> and << operators mean in the context
		//of binary integer digit positions, but in the context of the bitmap it means we have
		//to do kind of the reverse:
		//- To move bits higher (our <<) means moving them to the right (C++ >>) and vice versa.
		//****************************************************************************************
		BitMap& BitMap::operator<<=(uint32_t distance)
		{
			cached_count_flag = false;

			//Number of whole words to move "up" by
			uint32_t wordshift = distance / 32;
			if (wordshift != 0) {

				//Process all words, starting at the highest position
				uint32_t x = wholewords - 1;
				do {
					data[x] = (x >= wordshift)
						? data[x - wordshift]     //Lift the word from the lower position up to here
						: 0;                      //Zeros come in to the lowest positions
				} while (x-- > 0);
			}

			//Then a second pass doing any remaining part-word shift across word boundaries
			distance %= 32;
			if (distance != 0) {

				//Process all words, starting at the highest position
				uint32_t x = wholewords - 1;
				do {
					uint32_t thisWord = data[x];
					uint32_t leftSideOfThisWord = data[x] >> distance;
					if (x == 0)
						data[x] = leftSideOfThisWord;
					else {
						uint32_t wordToLeft = data[x - 1];
						uint32_t rightSideOfWordToLeft = wordToLeft << (32 - distance);
						data[x] = rightSideOfWordToLeft | leftSideOfThisWord;
					}
				} while (x-- > 0);
			}

			return (*this);
		}

		//****************************************************************************************
		//See remarks above re <<=
		BitMap& BitMap::operator>>=(uint32_t distance)
		{
			cached_count_flag = false;

			//Number of whole words to move "down" by
			uint32_t wordshift = distance / 32;
			if (wordshift != 0) {

				//Process all words from the lowest position upwards
				for (uint32_t x = 0; x < wholewords; ++x) {
					data[x] = ((wholewords - x) > wordshift)
						? data[x + wordshift]                   //Move word down from the higher position to here
						: 0;                                    //Zeros come into the highest positions
				}
			}

			//Then a second pass doing any remaining part-word shift across word boundaries
			distance %= 32;
			if (distance != 0) {

				//Process all words from the lowest position upwards
				for (uint32_t x = 0; x < wholewords; ++x) {
					data[x] =
						(data[x] << distance) |                     //Our right side
						((x < wholewords - 1)
							? (data[x + 1] >> (32 - distance))      //Plus the left side of the word to the right 
							: 0);                                   //(Or some zeros shifting into the rightmost word)
				}
			}

			return (*this);
		}

	} //close namespace util
} //close namespace dpt
