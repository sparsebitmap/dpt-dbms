/****************************************************************************************
EBM index page.
  
The EBM is scattered around table D, and this page (there may be up to 4 of them) holds
the page numbers of the actual bit map pages (page type P).
****************************************************************************************/

#ifndef BB_PAGE_E
#define BB_PAGE_E

#include "pagebase.h"

namespace dpt {

const short DBP_E_NUMSLOTS = DBPAGE_USABLE_SPACE / 4;

class DatabaseFileEBMManager;

//**************************************************************************************
class ExistenceBitMapIndexPage : public DatabaseFilePage {

	friend class DatabaseFileEBMManager;

	short PageOffsetFromRelSeg(int);

	ExistenceBitMapIndexPage(BufferPageHandle& bp) : DatabaseFilePage('E', bp, false) {} 
	ExistenceBitMapIndexPage(BufferPageHandle&, bool);

	int GetBitmapPageNum(int relseg);
	void SetBitmapPageNum(int, int);
};
	
} //close namespace

#endif
