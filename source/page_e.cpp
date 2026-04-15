
#include "stdafx.h"

#include "page_e.h" //#include "page_E.h" : V2.24 case is less interchangeable on *NIX - Roger M.

//Utils
//API tiers
//Diagnostics
#include "except.h"
#include "msg_db.h"

namespace dpt {

//***********************************************************************************
ExistenceBitMapIndexPage::ExistenceBitMapIndexPage(BufferPageHandle& bp, bool fresh) 
: DatabaseFilePage('E', bp, fresh) 
{
	if (fresh) {
		for (int offset = DBPAGE_DATA; offset < DBPAGE_SIZE; offset += 4)
			MapInt32(offset) = -1;
	}
}

//***********************************************************************************
short ExistenceBitMapIndexPage::PageOffsetFromRelSeg(int relseg) 
{
	int offset = DBPAGE_DATA + relseg * 4;
	if (offset >= DBPAGE_SIZE || offset < DBPAGE_DATA)
		throw Exception(DB_STRUCTURE_BUG, 
			"Bug: Invalid existence bitmap page pointer");

	return offset;
}

//***********************************************************************************
int ExistenceBitMapIndexPage::GetBitmapPageNum(int relseg) 
{
	short s = PageOffsetFromRelSeg(relseg);
	return MapInt32(s);
}

//***********************************************************************************
void ExistenceBitMapIndexPage::SetBitmapPageNum(int relseg, int pagenum) 
{
	MakeDirty();
	short s = PageOffsetFromRelSeg(relseg);
	MapInt32(s) = pagenum;
}

} //close namespace
