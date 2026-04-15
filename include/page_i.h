/****************************************************************************************
Inverted list master records
****************************************************************************************/

#ifndef BB_PAGE_I
#define BB_PAGE_I

#include "pageslotrec.h"

namespace dpt {

//------------
//Hmm - the precompiler can't do the constant math, and the C compiler doesn't seem to
//optimize it away either.  Hence redefine all these if changing the fraction.
#define ILMR_RESERVE_FRACTION 0.5
//#define ILMR_RESERVE_BYTES (DBPAGE_SIZE * ILMR_RESERVE_FRACTION)
#define ILMR_RESERVE_BYTES 4096
//#define ILMR_MINSIZE 8
//#define ILMR_PAGESLOTS ( (DBPAGE_SIZE / (ILMR_MINSIZE + 2)) * (1 - ILMR_RESERVE_FRACTION))
#define ILMR_PAGESLOTS 409
//------------

class InvertedListMasterRecordPage : public SlotAndRecordPage {
	friend class InvertedListAPI;

	void InsertSegRIB(short ilmrslot, short pagepos, short seg, short listslot, int listpage) {
		MakeDirty();
		Splice(ilmrslot, pagepos, NULL, 8);
		MapInt16(pagepos) = seg; 
		MapInt16(pagepos+2) = listslot; 
		MapInt32(pagepos+4) = listpage;}
	
	void DeleteSegRIB(short ilmrslot, short pagepos) {
		MakeDirty();
		Splice(ilmrslot, pagepos, NULL, -8);}

	void PageGetSegRIBInfo(short offset, short& seg, short& slot, int& page) {
		seg = MapInt16(offset); 
		slot = MapInt16(offset+2); 
		page = MapInt32(offset+4);}

	//The desired segment RIB will have been located before calling this
	void PageSetSegRIBInfo(short offset, short slot, int page) {
		MakeDirty(); 
		MapInt16(offset+2) = slot; 
		MapInt32(offset+4) = page;}

	void FindILMROnPage(short slotnum, short& offset, short& reclen) {
		offset = MapRecordOffset(slotnum);
		short nextoffset = MapNextRecordOffset(slotnum);
		reclen = nextoffset - offset;}

public:
	InvertedListMasterRecordPage(BufferPageHandle& bh, bool fresh) 
		: SlotAndRecordPage('I', bh, (fresh) ? ILMR_PAGESLOTS : -1) {}
	InvertedListMasterRecordPage(BufferPageHandle& bh) : SlotAndRecordPage('I', bh) {}
};
	
} //close namespace

#endif
