/****************************************************************************************
Functionality shared by any kind of primary index value entry page.  Currently this
only means btree leaves.  The main idea is to allow inverted lists to be moved and
ensure that the address held in the value entry gets udpated.
****************************************************************************************/

#ifndef BB_PAGEIXVAL
#define BB_PAGEIXVAL

#include "pagebase.h"

namespace dpt {

class AnyIndexValueEntryPage : public DatabaseFilePage {

	_int32& MapILMRPageNumber(short offset) {return MapInt32(offset);}
	short& MapILMRPageSlot(short offset) {return MapInt16(offset + 4);}

	friend class InvertedListAPI;
	AnyIndexValueEntryPage(BufferPageHandle& bh) : DatabaseFilePage('*', bh) {}

	int GetILMRPageNumber(short offset) {return MapILMRPageNumber(offset);}
	short GetILMRPageSlot(short offset) {return MapILMRPageSlot(offset);}

	//Double uses for the two fields to allow more efficient unique value handling
//	int GetIndexUniqueRecNum(short offset) {return MapILMRPageNumber(offset);}
//	short GetILMRControlValue(short offset) {return GetILMRPageSlot(offset);}

	void SetInvertedListInfo(short offset, int master_page, short master_num) {
		MakeDirty(); 
		MapILMRPageNumber(offset) = master_page;
		MapILMRPageSlot(offset) = master_num;}

};
	
} //close namespace

#endif
