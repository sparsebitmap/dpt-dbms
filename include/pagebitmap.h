/****************************************************************************************
Existence bit map pages and bitmap style inverted lists.
****************************************************************************************/

#ifndef BB_PAGEBITMAP
#define BB_PAGEBITMAP

#include "pagebase.h"
#include "bitmap3.h"

namespace dpt {

const short DBPAGE_BMAP_SETCOUNT = DBPAGE_PAGEHEADER;

class BitMapFilePage : public DatabaseFilePage {
protected:
	util::BitMap bitmap;

	BitMapFilePage(char c, BufferPageHandle& h, bool f = false) 
		: DatabaseFilePage(c, h, f), bitmap(RealData_S(), DBPAGE_SEGMENT_SIZE) {}

	void BadBit();
	void ValidateBit(unsigned short relrec) {if (relrec >= DBPAGE_SEGMENT_SIZE) BadBit();}
			
	unsigned short& MapSetCount() {return MapUInt16(DBPAGE_BMAP_SETCOUNT);}

public:
	bool FlagBit(unsigned short, bool);
	bool TestBit(unsigned short relrec) {ValidateBit(relrec); return bitmap.Test(relrec);}

	unsigned short FlagBits(const util::BitMap*, bool);

	unsigned short SetCount() {return MapSetCount();}
	
	util::BitMap& Data() {return bitmap;}
	const util::BitMap& CData() const {return bitmap;}
};
	
} //close namespace

#endif
