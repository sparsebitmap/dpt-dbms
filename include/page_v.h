/****************************************************************************************
Inverted list pages (list format - see also page type M which is bitmap format).
The list format is more compact than a whole 8K bitmap for just a few records.  See
tech docs for a discussion of the point at which bitmap is preferred.

The lists are expanded into bitmaps on retrieval during a find, since bitmaps are more
easily combined.  In some load-time situations the lists are retrieved as they are. 
****************************************************************************************/

#ifndef BB_PAGE_V
#define BB_PAGE_V

#include "pageslotrec.h"

#define DLIST_MAXRECS 1000
#define DLIST_PAGESLOTS 400

namespace dpt {

//***********************************************************************************
//V2.14 Jan 09.  This is not a totally rigorous container class, but it's efficient 
//and does the job of replacing <vector> below.
//***********************************************************************************
class ILArray {

	unsigned short* mem;
	bool own_mem;

	//V2.15.  See comments in du1step.cpp.
	void* heap;

	unsigned short* PAlloc() {return mem;}
	unsigned short* PEntries() {return mem+1;}
	unsigned short* PData() {return mem+2;}
	const unsigned short* CPAlloc() const {return mem;}
	const unsigned short* CPEntries() const {return mem+1;}
	const unsigned short* CPData() const {return mem+2;}

	unsigned short* AllocateArray(unsigned short&);
	void FreeArray();

public:
	//Empty container
	ILArray() : mem(NULL), own_mem(true), heap(NULL) {} 
	//Overlay array of shorts
	ILArray(unsigned short* m, void* h) : mem(m), own_mem(false), heap(h) {} 
	//Adopt container
	ILArray(unsigned short* m, void* h, bool om) : mem(m), own_mem(om), heap(h) {}

	~ILArray() {if (own_mem) DestroyData();}
	void DestroyData() {if (mem) FreeArray();}

	const unsigned short AllocSize() const {return *(CPAlloc());}
	const unsigned short NumEntries() const {return *(CPEntries());}
	unsigned short* Data() {return PData();}
	const unsigned short* CData() const {return CPData();}
	unsigned short* Mem() {return mem;}

	void* Heap() {return heap;}

	//See comment in cpp about the default parm here.
	unsigned short* InitializeAndReserve(unsigned short entries = 6);

	const unsigned short GetEntry(unsigned short e) const {return *(CPData() + e);}
	unsigned short* AppendEntry(unsigned short);

	void CopyEntriesFrom(void*, short);
	unsigned short* AdoptMemFrom() {own_mem = false; return mem;}
	void SetCount(unsigned short s) {*(PEntries()) = s;}
};

//*****************************************************************************
class InvertedIndexListPage : public SlotAndRecordPage {

public:
	InvertedIndexListPage(BufferPageHandle& bh, bool fresh) 
		: SlotAndRecordPage('V', bh, (fresh) ? DLIST_PAGESLOTS : -1) {}
	InvertedIndexListPage(BufferPageHandle& bh) : SlotAndRecordPage('V', bh) {}

	short SlotAddRecordToList(short, unsigned short);
	bool SlotRemoveRecordFromList(short, unsigned short, bool&);
	void SlotClearList(short);

	//V2.14 Jan 09.  Now using array not vector, for more control of space and speed.
//	void SlotRetrieveList(short, std::vector<unsigned short>*); 
//	void SlotPopulateNewList(short, std::vector<unsigned short>*);
	void SlotRetrieveList(short, ILArray&); 
	void SlotPopulateNewList(short, ILArray&);
};
	
} //close namespace

#endif
