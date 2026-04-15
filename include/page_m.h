/****************************************************************************************
Inverted index bitmap pages (see also page type V which are not bitmaps)
****************************************************************************************/

#ifndef BB_PAGE_M
#define BB_PAGE_M

#include "pagebitmap.h"
#include <vector>
#include "page_v.h" //#include "page_V.h" : V2.24 case is less interchangeable on *NIX - Roger M.

namespace dpt {

namespace util {class BitMap;}

class InvertedIndexBitMapPage : public BitMapFilePage {

public:
	InvertedIndexBitMapPage(BufferPageHandle& bh) : BitMapFilePage('M', bh, false) {}
	InvertedIndexBitMapPage(BufferPageHandle& bh, bool f) : BitMapFilePage('M', bh, f) {}

	//V2.14 Jan 09.  Replaced vector with array.
//	void InitializeFromList(std::vector<unsigned short>*);
	void InitializeFromList(ILArray&);

	void InitializeFromBitMap(const util::BitMap*, unsigned short* = NULL);
};
	
} //close namespace

#endif
