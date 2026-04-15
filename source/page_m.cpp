
#include "stdafx.h"

#include "page_m.h" //#include "page_M.h" : V2.24 case is less interchangeable on *NIX - Roger M.

namespace dpt {

void InvertedIndexBitMapPage::InitializeFromList(ILArray& array)
{
	const unsigned short numentries = array.NumEntries();

	for (int x = 0; x < numentries; x++) {
		unsigned short relrec = array.GetEntry(x);
		ValidateBit(relrec);
		bitmap.Set(relrec, true);
	}

	//It is assumed that the page was fresh before calling this function
	MapSetCount() = numentries;
}

//****************************************************************************
void InvertedIndexBitMapPage::InitializeFromBitMap
(const util::BitMap* newmap, unsigned short* count_already)
{
	bitmap.CopyBitsFrom(*newmap);

	unsigned short segrecs;
	if (count_already)
		segrecs = *count_already;
	else
		segrecs = newmap->Count();

	//V2.14 Jan 09.  During File Records or Load processing the set count was not 
	//being initialized, leaving an exposure to data corruption if the bitmap
	//was modified after that (by single-record changes to the field). Luckily
	//for static data or pure FILE RECORDS processing the only effect was
	//incorrect display on the ANALYZE command.
	MapSetCount() = segrecs;
}

} //close namespace
