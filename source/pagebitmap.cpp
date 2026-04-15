
#include "stdafx.h"

#include "pagebitmap.h"

//Utils
//API tiers
//Diagnostics
#include "except.h"
#include "msg_db.h"

namespace dpt {

//***********************************************************************************
void BitMapFilePage::BadBit() 
{
	throw Exception(DB_STRUCTURE_BUG, "Bug: Invalid segment-relative record number");
}

//***********************************************************************************
bool BitMapFilePage::FlagBit(unsigned short relrec, bool flag) 
{
	ValidateBit(relrec);

	//The caller often needs to know if the bit already had the requested, and in any
	//case we can use it here to decide whether to dirty the page or not.
	bool current = bitmap.Test(relrec);

	if (current != flag) {
		MakeDirty();
		bitmap.Set(relrec, flag);

		//We maintain a running count on the page too, since when turning off bits it
		//is useful to know when the count goes down to one or zero.  In both cases we
		//can do later optimizations based on this.  For example maps going down to one bit
		//need not be expanded into full bitmaps for counting etc.  Index bitmaps are 
		//removed when they go down to one bit (an immediate pointer is stored).
		if (flag)
			MapSetCount()++;
		else
			MapSetCount()--;
	}

	return current;
}

//***********************************************************************************
unsigned short BitMapFilePage::FlagBits(const util::BitMap* bm2, bool flag) 
{
	MakeDirty();

	unsigned short precount = MapSetCount();

	if (flag)
		bitmap |= *bm2;
	else
		bitmap /= *bm2;

	MapSetCount() = bitmap.Count();

	unsigned short postcount = MapSetCount();

	if (flag)
		return postcount - precount;
	else
		return precount - postcount;
}

} //close namespace
