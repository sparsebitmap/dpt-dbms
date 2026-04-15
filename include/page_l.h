/****************************************************************************************
BLOBs: "Table E" in M204 terms

Unfortunately page types B and E are already taken so it was a contest between L and O.
****************************************************************************************/

#ifndef BB_PAGE_L
#define BB_PAGE_L

#include "pageslotrec.h"

namespace dpt {

//Although I'm allowing 2 bytes in the descriptor "just in case"
#define BLOB_PAGESLOTS 32
//So page+slot
#define PAGE_L_EXTPTR_LEN 6 
//This is arbitrary but similar arguments as with ILMR pages
#define BLOB_SMALLEST_EXTENT 4096


class BLOBPage : public SlotAndRecordPage {

	friend class DatabaseFileTableDManager;
	friend class DatabaseFileDataManager;
	friend class RecordDataAccessor;

	int& MapExtensionPtrPage(short slot) {
		return *(reinterpret_cast<int*>(MapPChar(MapRecordOffset(slot))));}
	short& MapExtensionPtrSlot(short slot) {
		return *(reinterpret_cast<short*>(MapPChar(MapRecordOffset(slot) + 4)));}

	BLOBPage(BufferPageHandle& bh, bool fresh) 
		: SlotAndRecordPage('L', bh, (fresh) ? BLOB_PAGESLOTS : -1) {}
	BLOBPage(BufferPageHandle& bh) : SlotAndRecordPage('L', bh) {}

	short AllocateSlotAndStoreBLOB(const char**, int&);

	void SlotSetExtensionPointers(short slot, int extpage, short extslot) {
		MapExtensionPtrPage(slot) = extpage; MapExtensionPtrSlot(slot) = extslot;}
	void SlotGetExtensionPointers(short slot, int& extpage, short& extslot) {
		extpage = MapExtensionPtrPage(slot); extslot = MapExtensionPtrSlot(slot);}

	void GetBLOBExtentData(int&, short&, const char**, short*); //return ptr to page data buffer+len
	void GetBLOBExtentData(int&, short&, std::string&);         //also append to string if convenient
};

} //close namespace

#endif
