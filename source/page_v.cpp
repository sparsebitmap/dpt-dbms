
#include "stdafx.h"

#include "windows.h" //for heap functions

#include "charconv.h"
#include "page_v.h" //#include "page_V.h" : V2.24 case is less interchangeable on *NIX - Roger M.
#include "except.h"
#include "msg_db.h"

namespace dpt {

//*************************************************************************
unsigned short* ILArray::AllocateArray(unsigned short& alloc_entries)
{
	if (heap) {
		HANDLE* h = static_cast<HANDLE*>(heap);
		
		//V3.0.  See comment in DeferredUpdate1StepInfo::CreateLocalHeaps()
		//void* ptr = HeapAlloc(*h, HEAP_NO_SERIALIZE | HEAP_GENERATE_EXCEPTIONS, alloc_entries * 2);
		void* ptr = HeapAlloc(*h, HEAP_GENERATE_EXCEPTIONS, alloc_entries * 2);

		//If it allocates a bit more, you're allowed to use it, although by most
		//accounts this HeapSize() function *never* tells you of any extra even though
		//it has allocated it.  V annoying.  However the call is fairly cheap (10% or 
		//so of the above) so I'm leaving it in case future Windows versions are better:

		//DWORD actual_alloc = HeapSize(*h, HEAP_NO_SERIALIZE, ptr); see above
		DWORD actual_alloc = HeapSize(*h, 0, ptr);
		if (actual_alloc != alloc_entries)
			alloc_entries = actual_alloc / 2;

		return static_cast<unsigned short*>(ptr);
	}
	else {
		return new unsigned short[alloc_entries];
	}
}

//**************************
void ILArray::FreeArray() 
{
	if (heap) {
		HANDLE* h = static_cast<HANDLE*>(heap);
		//HeapFree(*h, HEAP_NO_SERIALIZE, mem); see above
		HeapFree(*h, 0, mem);
	}
	else {
		delete[] mem;
	}

	mem = NULL;
}

//*************************************************************************
unsigned short* ILArray::AppendEntry(unsigned short e)
{
	unsigned short currentries = NumEntries();
	unsigned short curralloc = AllocSize();

	//Double array size if it fills up, sort of like regular <vector> does
	if (currentries == curralloc - 2) {

		//The actual data structure would take 64K entries, but should only ever need 1000.
		if (curralloc >= DLIST_MAXRECS)
			throw Exception(DB_ALGORITHM_BUG, 
				"Bug: ILArray trying to expand beyond expected max.");		
		
		unsigned short tempalloc = curralloc * 2;
		unsigned short* temp = AllocateArray(tempalloc);
		memcpy(temp, mem, curralloc * sizeof(unsigned short));

		FreeArray();
		mem = temp;
		*(PAlloc()) = tempalloc;
	}

	//Put new value after the last one
	PData()[currentries] = e;
	(*(PEntries()))++;
	return mem;
}

//****************************************************
//Re. Default value for the initial number of entries:
//On Win32 the heap granularity is 16 bytes, plus it actually allocates quite a lot more
//than that (see Win docs).  Allocations of <=8 bytes or round multiples of 8 have an 
//overhead of 16 bytes, and otherwise the overhead is 32 extra.  So request 9 and it 
//actually uses 48!  This is because of alignment issues and the heap indexing algorithm.
//Since this is a core part of the DU processing, it would be best to use a custom
//allocator of some kind to reduce the large overhead for inverted lists in the 10-50 
//sort of range (probably most).  However for now I'm leaving it but at least starting
//with an initial allocation that rounds nicely to 16 bytes (6 entries = 12 plus 4 bytes
//of control info is 16).
unsigned short* ILArray::InitializeAndReserve(unsigned short entries) 
{
	if (entries < 6)
		entries = 6;

	DestroyData();

	unsigned short slots = entries+2;
	mem = AllocateArray(slots);
	*(PAlloc()) = slots;
	*(PEntries()) = 0;

	return mem;
}

//*************************************************
void ILArray::CopyEntriesFrom(void* source, short entries) 
{
	if (!mem || AllocSize() < entries+2)
		InitializeAndReserve(entries);

	memcpy(PData(), source, entries * 2);
	*(PEntries()) = entries;
}



//*************************************************************************
//Return codes:
//	0 = record number added successfully to the end of the list
//	1 = record already on list
//	2 = record not on list but list already has enough entries for a bitmap
//	3 = not on list but page is full
//*************************************************************************
short InvertedIndexListPage::SlotAddRecordToList
(short slotnum, unsigned short relrec)
{
	short rec_pos_on_page = MapRecordOffset(slotnum);
	short next_rec_pos_on_page = MapNextRecordOffset(slotnum);

	short list_bytes = next_rec_pos_on_page - rec_pos_on_page;
	short list_entries = list_bytes / 2;

	//Search for the record number
	short pagepos;
	for (pagepos = rec_pos_on_page; pagepos < next_rec_pos_on_page; pagepos += 2) {
		if (MapUInt16(pagepos) == relrec)
			return 1;
	}

	//Lists have to be promoted to bitmaps at a certain point
	if (list_entries == DLIST_MAXRECS)
		return 2;

	//Going to add - we will need 2 bytes
	if (NumFreeBytes() < 2)
		return 3;

	//We are now at the end of the list, and the list doesn't contain relrec
	MakeDirty();
	Splice(slotnum, pagepos, &relrec, 2);
	return 0;
}

//*************************************************************************
bool InvertedIndexListPage::SlotRemoveRecordFromList
(short slotnum, unsigned short relrec, bool& empty)
{
	empty = false;

	short rec_pos_on_page = MapRecordOffset(slotnum);
	short next_rec_pos_on_page = MapNextRecordOffset(slotnum);

	short list_bytes = next_rec_pos_on_page - rec_pos_on_page;
	short list_entries = list_bytes / 2;

	//Search for the record number
	for (short pagepos = rec_pos_on_page; pagepos < next_rec_pos_on_page; pagepos += 2) {
		if (MapUInt16(pagepos) == relrec) {
			MakeDirty();
			Splice(slotnum, pagepos, NULL, -2);
			if (list_entries == 1)
				empty = true;
			return true;
		}
	}

	//Not found
	return false;
}

//*************************************************************************
void InvertedIndexListPage::SlotClearList(short slotnum)
{
	short rec_pos_on_page = MapRecordOffset(slotnum);
	short next_rec_pos_on_page = MapNextRecordOffset(slotnum);

	short list_bytes = next_rec_pos_on_page - rec_pos_on_page;

	MakeDirty();
	Splice(slotnum, rec_pos_on_page, NULL, -list_bytes);
}


//*************************************************************************
//V2.14 Jan 09.  New, neater, more efficient versions of these two functions.
//*************************************************************************
void InvertedIndexListPage::SlotRetrieveList(short slotnum, ILArray& outarray)
{
	short rec_pos_on_page = MapRecordOffset(slotnum);
	short next_rec_pos_on_page = MapNextRecordOffset(slotnum);
	short list_bytes = next_rec_pos_on_page - rec_pos_on_page;
	short list_entries = list_bytes / 2;

	outarray.CopyEntriesFrom(MapPChar(rec_pos_on_page), list_entries);
}

//************************
void InvertedIndexListPage::SlotPopulateNewList(short slotnum, ILArray& inarray)
{
	short rec_pos_on_page = MapRecordOffset(slotnum);
	short list_bytes = inarray.NumEntries() * 2;

	MakeDirty();

	//Room on page has already been checked before we get here
	Splice(slotnum, rec_pos_on_page, NULL, list_bytes);

	memcpy(MapPChar(rec_pos_on_page), inarray.Data(), list_bytes);
}

} //close namespace
