
#include "stdafx.h"

#include "pageslotrec.h"

//Utils
//API tiers
//Diagnostics
#include "msg_db.h"
#include "except.h"

namespace dpt {

//**************************************************************************************
SlotAndRecordPage::SlotAndRecordPage(char c, BufferPageHandle& bh, short recppg) 
: DatabaseFilePage(c, bh, (recppg != -1)) 
{
	//Only do initial formatting if it's a fresh page
	if (recppg != -1)
		Initialize(recppg);
}

//**************************************************************************************
void SlotAndRecordPage::Initialize(short recppg) 
{
	MapNumSlots() = recppg;
	MapFreeSlots() = recppg;
	MapNextFreeSlot() = 0;

	//This is more readable in a dump than say using (queueptr = -2) for the same purpose
	MapInt8(DBP_SAR_REUSEQ_FLAG) = 'N';

	//See tech docs for comments on the issue of allocating all the slot pointers now.
	WriteChars(DBP_SAR_SLOT_EYE, "PTR:", 4);
	for (int x = 0; x < recppg; x++)
		MapInt16(DBP_SAR_SLOTAREA + x * 2) = -1;

	WriteChars(EyecatcherPos(), "DAT:", EyecatcherLen());

	MapInt16(DBP_SAR_FREEBASE) = InitialFreeBase();
}





//**************************************************************************************
//**************************************************************************************
short& SlotAndRecordPage::MapRecordOffset(short slotnum, bool allow_minus_one) 
{
	if (slotnum < 0 || slotnum >= MapNumSlots())
		throw Exception(DB_STRUCTURE_BUG, "Bug: invalid SAR page slot number");

	short& offset = MapInt16(DBP_SAR_SLOTAREA + slotnum * 2);

	if (offset == -1) {
		if (allow_minus_one)
			return offset;
		else if (PageType_S() == 'B')
			//See comments around where this exception is caught - the handling has changed
			throw Exception(DML_NONEXISTENT_RECORD);
		else
			throw Exception(DB_STRUCTURE_BUG, "Bug: accessing nonexistent SAR record");
	}

	//Sanity check
	if (offset < DBP_SAR_SLOTAREA + MapNumSlots() * 2 || offset >= DBPAGE_SIZE)
		throw Exception(DB_STRUCTURE_BUG, "Bug: invalid SAR page slot pointer");

	return offset;
}

//**************************************************************************************
short& SlotAndRecordPage::MapNextRecordOffset(short slotnum) 
{
	//This function is called quite a lot and in cases where the record is the last
	//record on the page it has to scan potentially quite a lot of -1s before it
	//realizes the fact.  Unfortunately there is little we can do to avoid this in
	//the most common cases where an unpredictable amount of slots are in use after
	//the current one.  Just make this loop as tight as possible.
	short* start = &MapInt16(DBP_SAR_SLOTAREA + (slotnum + 1) * 2);
	short* end = &MapInt16(DBP_SAR_SLOTAREA + MapNumSlots() * 2);

	for (short* ptr = start; ptr < end; ptr++) {
		if (*ptr != -1) {
			short& offset = *ptr;

			//Same sanity check as in the prev func
			if (offset < DBP_SAR_SLOTAREA + MapNumSlots() * 2 || offset >= DBPAGE_SIZE)
				throw Exception(DB_STRUCTURE_BUG, "Bug: invalid SAR page slot pointer");

			return offset;
		}
	}

	//It was the last valid record on the page (all later slots -1)
	return MapFreeBase();
}

//**************************************************************************************
void SlotAndRecordPage::Splice
(short slotnum, short insert_offset, void* insert_data, short insert_size) 
{
	//Rejig the slot pointers for records above this one.  In this case (compare
	//the above) the caller will have determined the insert point, so we know
	//if there's any info above it, and we can avoid scanning in many cases.
	if (insert_offset < MapFreeBase()) {
		for (short x = slotnum + 1; x < MapNumSlots(); x++) {
			short& recoffset = MapRecordOffset(x, true);

			if (recoffset != -1)
				recoffset += insert_size;
		}
	}

	//Move the populated region of the page up or down above the insert point.
	if (insert_size > 0) {

		//NB1. The calling function will already have verified there is sufficient 
		//space, because different action is taken in different places for failure.
		short bytes_to_move = MapFreeBase() - insert_offset;
		short target_offset = insert_offset + insert_size;

		if (bytes_to_move > 0)
			memmove(MapPChar(target_offset), MapPChar(insert_offset), bytes_to_move);

		//Put the new data in the gap unless the caller plans to do that later
		if (insert_data)
			memcpy(MapPChar(insert_offset), insert_data, insert_size);
	}

	//Negative size means a deletion not an insertion really
	else {
		short source_offset = insert_offset - insert_size;
		short bytes_to_move = MapFreeBase() - source_offset;

		if (bytes_to_move > 0)
			memmove(MapPChar(insert_offset), MapPChar(source_offset), bytes_to_move);

		//Zeroize the revealed region at the top (for readability in dumps)
		short numzeros = -insert_size;
		memset(MapPChar(MapFreeBase() - numzeros), 0, numzeros);
	}

	//Move the free space pointer up or down
	MapFreeBase() += insert_size;
}







//**************************************************************************************
//**************************************************************************************
bool SlotAndRecordPage::QueueForSlotReuse(int prevqfront) 
{
	if (IsOnReuseQueue())
		return false;

	MakeDirty();

	//Mark it so we don't have to scan the queue to know - see above test
	MapInt8(DBP_SAR_REUSEQ_FLAG) = 'Y';
	MapReuseQPtr() = prevqfront;

	return true;
}

//**************************************************************************************
int SlotAndRecordPage::RemoveFromReuseQueue() 
{
	int newqfront = MapReuseQPtr();

	MakeDirty();

	MapInt8(DBP_SAR_REUSEQ_FLAG) = 'N';
	MapReuseQPtr() = -1;

	return newqfront;
}







//**************************************************************************************
//The table B algorithms do not use this function because of various extra requirements
//such as optional RRN, and different handling when extending records.
//**************************************************************************************
short SlotAndRecordPage::AllocateSlot(short reserved_space, short required_space) 
{
	//All slots in use
	if (MapFreeSlots() == 0)
		return -1;

	//Not enough useful room
	if (NumFreeBytes() < reserved_space + required_space)
		return -1;

	//Use slots in order to start with.  This means we can avoid some memmove work for
	//a while when modifying the page (see e.g. Splice above).
	short slotnum = MapNextFreeSlot();

	//When reusing slots take the topmost ones first for the above reason.
	bool reuse = (slotnum == MapNumSlots());
	if (reuse) {
		for (slotnum-- ; ; slotnum--) {

			if (slotnum == -1)
				throw Exception(DB_STRUCTURE_BUG, 
					"Trying to reuse slot on a page with no free slots");

			if (MapRecordOffset(slotnum, true) == -1)
				break;
		}
	}

	short insert_point = MapNextRecordOffset(slotnum);

	MakeDirty();

	MapRecordOffset(slotnum, true) = insert_point;
	MapFreeSlots()--;

	//See above
	if (!reuse)
		MapNextFreeSlot()++;

	return slotnum;
}

//**************************************************************************************
//void SlotAndRecordPage::DeleteEmptySlot(short slotnum) //V3.0 
void SlotAndRecordPage::DeleteSlotData_S(short slotnum, bool emptycheck) 
{
	short& recoffset = MapRecordOffset(slotnum);

	short reclen = MapNextRecordOffset(slotnum) - recoffset;

	//Ensure the record is empty ("empty" table B records have length 4)
	if (emptycheck) {
		if (reclen != EmptyRecordLength())
			throw Exception(DB_STRUCTURE_BUG, "Bug: deleting non-empty record extent");
	}

	MakeDirty();

	//Remove any remains so no data space is occupied now
	if (reclen > 0)
		Splice(slotnum, recoffset, NULL, -reclen);

	recoffset = -1;
	MapFreeSlots()++;
}



//**************************************************************************************
//July 09, in prep for V3.0.  Direct access to page for fast unload of entire records at once.
//Note that we can allow this access since CFR_DIRECT is held for the entire unload.
void SlotAndRecordPage::SlotGetPageDataPointer(short slotnum, const char** pbuff, short* plen) 
{
	short page_offset = MapRecordOffset(slotnum);

	*pbuff = MapPChar(page_offset);

	*plen = MapNextRecordOffset(slotnum) - page_offset;
}


} //close namespace
