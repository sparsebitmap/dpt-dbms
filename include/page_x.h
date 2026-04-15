/****************************************************************************************
Reusable table D page
****************************************************************************************/

#ifndef BB_PAGE_X
#define BB_PAGE_X

#include "pagebase.h"

namespace dpt {

class ReusableTableDPage : public DatabaseFilePage {
public:
	ReusableTableDPage(BufferPageHandle& bh) : DatabaseFilePage('*', bh) {}

	char PageType() {return PageType_S();}

	int GetDReuseQPtr() {return MapInt32(DBPAGE_DREUSE_PTR);}
	char SetDReuseQPtr(int p) {
		MakeDirty(); 
		MapInt32(DBPAGE_DREUSE_PTR) = p;

		//There's no sense preserving the old page type, as the page will just be empty,
		//so set to X for the neatness of having all the page types match their mappers.
		//In addition it means that the ANALYZE2 code can do a tableD scan and know
		//that the I and V pages it finds are all in play.
		char pt = PageType_S();
		PageType_S() = 'X';
		PageSubType_S() = 'X';
		return pt;}
};
	
} //close namespace

#endif
