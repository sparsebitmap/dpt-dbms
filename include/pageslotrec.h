/****************************************************************************************
Common functionality used by table B record pages, inverted list master record pages
and table D list pages.  All of those are arranged in the Model 204 "table B" page style,
which is to say an area of pointers at one end of the page, which point to the actual
record blocs which are moveable chunks after that.  Record sizes can then change, at
the cost of moving all the other records on the page and rejigging slot pointers.
****************************************************************************************/

#ifndef BB_PAGESLOTREC
#define BB_PAGESLOTREC

#include "pagebase.h"

namespace dpt {

const short DBP_SAR_REUSEQ_PTR	= DBPAGE_DATA + 0;
const short DBP_SAR_NUMSLOTS	= DBPAGE_DATA + 4; //duplicates brecppg
const short DBP_SAR_FREESLOTS	= DBPAGE_DATA + 6; //saves counting when reusing slots  
const short DBP_SAR_NEXTFREE	= DBPAGE_DATA + 8; //sequential filling mode
const short DBP_SAR_FREEBASE	= DBPAGE_DATA + 10;
const short DBP_SAR_REUSEQ_FLAG	= DBPAGE_DATA + 13; //whether page is on Q

const short DBP_SAR_SLOT_EYE	= DBPAGE_DATA + 16;
const short DBP_SAR_SLOTAREA	= DBPAGE_DATA + 20;

class SlotAndRecordPage : public DatabaseFilePage {

	friend class DatabaseFileTableBManager;
	friend class DatabaseFileTableDManager;

	void Initialize(short);

	bool QueueForSlotReuse(int);
	int RemoveFromReuseQueue();
	bool IsOnReuseQueue()		{return (MapInt8(DBP_SAR_REUSEQ_FLAG) == 'Y');}
	_int32& MapReuseQPtr()		{return MapInt32(DBP_SAR_REUSEQ_PTR);}

	int GetReuseQPointer() {return MapReuseQPtr();}
	void SetReuseQPointer(int i) {MakeDirty(); MapReuseQPtr() = i;}

	void DeleteSlotData_S(short, bool emptycheck);
	void DeleteEmptySlotData(short slot) {DeleteSlotData_S(slot, true);}
	void DeleteSlotData(short slot) {DeleteSlotData_S(slot, false);}

	virtual short EmptyRecordLength() {return 0;}
	short EyecatcherPos() {return (DBP_SAR_SLOTAREA + MapNumSlots() * 2);}
	short EyecatcherLen() {return 4;}
	short InitialFreeBase() {return EyecatcherPos() + EyecatcherLen();}

protected:
	short& MapNumSlots()		{return MapInt16(DBP_SAR_NUMSLOTS);}
	short& MapFreeSlots()		{return MapInt16(DBP_SAR_FREESLOTS);}
	short& MapNextFreeSlot()	{return MapInt16(DBP_SAR_NEXTFREE);}
	short& MapFreeBase()		{return MapInt16(DBP_SAR_FREEBASE);}

	SlotAndRecordPage(char c, BufferPageHandle& h) : DatabaseFilePage(c, h, false) {}
	SlotAndRecordPage(char, BufferPageHandle&, short);
	SlotAndRecordPage(char, RawPageData*);

	short& MapRecordOffset(short, bool = false);
	short& MapNextRecordOffset(short);

	short AllocateSlot(short, short);
	void Splice(short, short, void*, short);

public:
	short NumSlots() {return MapNumSlots();}
	short NumFreeBytes() {return DBPAGE_SIZE - MapFreeBase();}
	short MaxFreeBytes() {return DBPAGE_SIZE - InitialFreeBase();}
	short NumSlotsInUse() {return MapNumSlots() - MapFreeSlots();}

	void SlotGetPageDataPointer(short, const char**, short*);
};
	
} //close namespace

#endif
