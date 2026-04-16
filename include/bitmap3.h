/****************************************************************************************
This is something of a crib from the STL <bitset> header as provided with MVC++, but
is significantly different.  For a start it is a concrete class and not a template, so that
you give the number of bits when constructing.  Secondly it is based on a pointer
rather than an array.  This would be detrimental to performance for normal use, since
heap storage would have to be used.  However, it has the great benefit that we can now
manipulate any area of storage as a bitmap, by passing a pointer to void with the
constructor.  This explains the name change, since the class is a map of some other area
of storage rather than (necessarily) a set or container in its own right.
Obviously this is most useful for disk buffer pages which make heavy use of bit maps in
M204.
	e.g. BitMap b(&page + PAGE_HEADER);

Updates:
--------
01/04/26  : Change the ambiguous unsigned long and size_t to uint32_t throughout so the
			32-bit chunking algorithms work on either 32 or 64 bit compiler/machine.
			Also changed all UINT_MAX/ULONG_MAX to explicitly 0xFFFFFFFF.
05/09/10  : Increased local noheap cache size to 8 words from 4 (i.e. 256 bits) for better
			efficiency up to the maximum string field size of 255 chars, since bitmaps
			are sometimes heavily used in pattern matches.
27/08/10  : Fixed bug with private heap when copying between objs with/without heapdata
31/07/09  : Allow resize without cleaning the fresh memory (usually prior to clone)
06/05/09  : Faster recounts.
03/04/09  : Container memory can be allocated on a private heap.
01/03/09  : Added forced recount function for if we mess with the contained data.
23/04/07  : Made copy constructor and operator= take const parm (g++ requirement).
10/12/05  : Tuned some looping funcs to use ptr addition instead of repeated array lookup.
			Concentrated on the common ones used by file bitmap processing, where we will
			be doing loops across 8K bitmaps very frequently.
06/12/05  : Added AND NOT operator for use by bmlist::remove
03/08/05  : Changed SetAll() to use memset().
07/07/05  : Tuned for use mainly with pattern matching to not require heap access
			when creating a container object with small numbers of bits.
			Also re-inlined the shortish functions.
15/03/05  : FindFirst and FindLast defaults now use UINT_MAX because 0 is a valid value.
15/02/05  : Added capability to search backwards for use in record loop processing.
19/01/05  : Clarified assignment function operation: renamed and split in 2.
10/10/04  : Added cached count for frequent use during record set cursor processing.
Version 3b: Added Find and SetRange for use in the buffer manager
Version 3a: Removed auto-extend
Version 3 : Re-added the ability to have the object allocate and own its own memory.
			Also made it so it will automatically extend itself if necessary.
			Forced it to be a multiple of 32 bits - much easier (version 2 still in bitmap2).
			Added All() function.
Version 2 : Generalized to handle any number of bits.  (Version 1 still in dbbitmap).
Version 1 : Cribbed from <bitmap>, but object never owns its own memory.  Stripped out some
			lesser-used functions like output streaming.  Assuming map will always be 8K.
****************************************************************************************/

#ifndef BB_BITMAP3
#define BB_BITMAP3

#define BBBM_MAX_STACK_BITS 256 
#define BBBM_MAX_STACK_WORDS 8

#include <string>
#include <cstdint>
#include "except.h"
#include "msg_util.h"

namespace dpt {
	namespace util {

		class BitMap {
			static uint32_t onebit;
			static uint32_t nobits;
			static uint32_t allbits;

			uint32_t stack_data[BBBM_MAX_STACK_WORDS]; //07/07/05
			uint32_t* data;
			uint32_t numbits;
			uint32_t wholewords;

			//10/10/04
			mutable uint32_t cached_count_value;
			mutable bool cached_count_flag;

			//When used as a container
			bool own_memory;
			void* heap;

			void RangeCheck(uint32_t offset) const {
				if (numbits > offset) return;
				throw Exception(UTIL_BITMAP_ERROR, "Bit subscript out of mapped range");
			}
			void Mod32Check(uint32_t b) const {
				if (b % 32)	throw Exception(UTIL_BITMAP_ERROR,
					"Number of bits mapped must be a multiple of 32");
			}
			void SizeMismatch() const {
				throw Exception(UTIL_BITMAP_ERROR,
					"Error in bitmap operation requiring equal-size arguments");
			}
			void SizeEqualityCheck(uint32_t b1, uint32_t b2) const { if (b1 != b2) SizeMismatch(); }

			void CopyInformationFrom(const BitMap& b2) {
				numbits = b2.numbits;
				wholewords = b2.wholewords;
				cached_count_flag = b2.cached_count_flag;
				cached_count_value = b2.cached_count_value;
			}

			void CopyDataFrom(const BitMap& b2) {
				if (numbits == 32) //common case
					*data = *(b2.data);
				else
					memcpy(data, b2.data, numbits / 8);
			}

			//April 05.
			void PrivateHeapAllocate();
			void PrivateHeapFree();

			//Aug 2010.
			void DestroyData() {
				if (own_memory && data != stack_data) {
					if (heap) PrivateHeapFree();
					else delete[] data;
				}
			}

		public:

			//****************************************************************************
			//Constructors
			//****************************************************************************

				//Use to map any area of data
			BitMap(void* mem, uint32_t b)
				: numbits(b), wholewords(b / 32),
				cached_count_flag(false), own_memory(false), heap(NULL) {
				Mod32Check(b);
				data = (uint32_t*)mem;
			}

			//Use as a container owning its own data 
			BitMap(uint32_t b = 0, void* h = NULL)
				: numbits(b), wholewords(b / 32),
				cached_count_flag(false), own_memory(true), heap(h) {
				Mod32Check(b);
				if (b <= BBBM_MAX_STACK_BITS) data = stack_data;
				else if (heap) PrivateHeapAllocate();
				else data = new uint32_t[wholewords];
				ResetAll();
			}

			~BitMap() { DestroyData(); }

			//Non-destructive assignment
			BitMap& CopyBitsFrom(const BitMap& b2) {
				SizeEqualityCheck(numbits, b2.numbits);
				CopyInformationFrom(b2);
				CopyDataFrom(b2);
				return (*this);
			}

			BitMap& AdoptDataFrom(const BitMap& b2); //destructive only if b2 has its own memory

			//These two are the same for conceptual clarity reasons
			BitMap(const BitMap& b2) : own_memory(false), heap(NULL) { AdoptDataFrom(b2); }
			BitMap& operator=(const BitMap& b2) { return AdoptDataFrom(b2); }

			//****************************************************************************
			//General info functions
			//****************************************************************************
			uint32_t		GetWord(uint32_t word_offset) const { return (data[word_offset]); }
			uint32_t		NumBits()					const { return numbits; }
			uint32_t		NumWholeWords()				const { return wholewords; }
			std::string		ToStringAsBinaryNumber() const; //Most significant bit to the left
			std::string		ToStringAsBitMap() const; //Most significant bit to the right

			uint32_t Count() const;
			void InvalidateCachedCount() { cached_count_flag = false; }
			bool All() const;
			bool Any() const;
			bool None() const { return !Any(); }

			//* * NOTE* * If modifying the data, be sure to call InvalidateCachedCount()
			void* Data()						const { return data; }
			const void* CData()					const { return data; }

			//****************************************************************************
			//Element access functions.  I never use at(), so cut it out.
			//****************************************************************************
			bool Test(uint32_t offset) const {
				RangeCheck(offset);
				return (data[offset / 32] & (onebit << (31 - (offset % 32)))) != 0;
			}

			//Output parm (uint32_t&) is not touched if the return value is false
			bool FindNext(uint32_t&, uint32_t = 0, uint32_t = 0xFFFFFFFF, bool = true) const;
			bool FindPrev(uint32_t&, uint32_t = 0xFFFFFFFF, uint32_t = 0, bool = true) const;

			//*************************************************************************
			//This private class is here to allow [] notation to be used as an l-value.
			//*************************************************************************
			class Cursor {
				BitMap* parent;
				uint32_t  offset;

				friend class BitMap;
				Cursor(BitMap& p, uint32_t o) : parent(&p), offset(o) {}

			public:
				Cursor& operator=(bool b) {
					parent->Set(offset, b);
					return *this;
				}
				Cursor& operator=(const Cursor& c) {
					parent->Set(offset, c.IsSet());
					return *this;
				}
				Cursor& Flip() {
					parent->Flip(offset);
					return *this;
				}
				bool IsSet() const {
					return (parent->Test(offset));
				}
				operator bool() {
					return IsSet();
				}
			};

			//	bool operator[](uint32_t offset) const {return (Test(offset)); }
			Cursor operator[](uint32_t offset) { return (Cursor(*this, offset)); }

			//****************************************************************************
			//Comparison of bitmaps
			//****************************************************************************
			bool AnyIntersection(const BitMap& rhs) const;
			bool operator==(const BitMap& rhs) const;
			bool operator!=(const BitMap& rhs) const { return !(*this == rhs); }

			//****************************************************************************
			//Set-modifying operations.  Dropped the func which cleared a range of bits.
			//Also dropped all the copying functions like >> and |
			//****************************************************************************
			BitMap& EnsureCapacity(uint32_t n, bool clean = true);

			BitMap& Set(uint32_t offset, bool b = true) {
				RangeCheck(offset);
				if (b)
					data[offset / 32] |= onebit << (31 - (offset % 32));
				else
					data[offset / 32] &= ~(onebit << (31 - (offset % 32)));
				cached_count_flag = false; //Could preserve, but would require test
				return *this;
			}

			BitMap& SetAll(bool b = true) {
				//					uint32_t val = b ? allbits : nobits;
				//					for (uint32_t x = 0; x < wholewords; ++x)
				//						data[x] = val;
				unsigned char val = b ? 0xFF : 0x00;
				memset(data, val, numbits / 8);
				cached_count_flag = true;
				cached_count_value = (b) ? numbits : 0;
				return *this;
			}

			BitMap& SetRange(uint32_t, uint32_t, bool b = true);

			BitMap& Reset(uint32_t offset) { return Set(offset, false); }
			BitMap& ResetAll() { return SetAll(false); }

			BitMap& Flip(uint32_t);
			BitMap& FlipAll();

			//****************************************************************************
			//Modifying operations based on another BitMap - dropped the xor version.
			//****************************************************************************
			BitMap& operator&=(const BitMap& rhs);
			BitMap& operator|=(const BitMap& rhs);

			//AND NOT ... for List::Remove.  (/= is not totally intuitive but neat)
			BitMap& operator/=(const BitMap& rhs);

			//****************************************************************************
			//Shifters.
			//****************************************************************************
			BitMap& operator<<=(uint32_t);
			BitMap& operator>>=(uint32_t);
		};

	} //close namespace util
} //close namespace dpt

#endif
