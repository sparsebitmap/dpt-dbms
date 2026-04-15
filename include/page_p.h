/****************************************************************************************
Existence bit map page.

This is interpreted the same as an index bitmap page - hence shared class descent.
****************************************************************************************/

#ifndef BB_PAGE_P
#define BB_PAGE_P

#include "pagebitmap.h"

namespace dpt {

class DatabaseFileEBMManager;

class ExistenceBitMapPage : public BitMapFilePage {

	friend class DatabaseFileEBMManager;

	ExistenceBitMapPage(BufferPageHandle& bp): BitMapFilePage('P', bp, false) {}
	ExistenceBitMapPage(BufferPageHandle& bp, bool fresh);
};
	
} //close namespace

#endif
