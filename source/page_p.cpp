
#include "stdafx.h"

#include "page_p.h" //#include "page_P.h" : V2.24 case is less interchangeable on *NIX - Roger M.

//Utils
//API tiers
//Diagnostics

namespace dpt {

//char* debug_ptr = 0;

ExistenceBitMapPage::ExistenceBitMapPage(BufferPageHandle& bp, bool fresh) 
: BitMapFilePage('P', bp, fresh) 
{
//	if (fresh)
//		debug_ptr = MapPChar(DBPAGE_PAGETYPE);
}

} //close namespace
