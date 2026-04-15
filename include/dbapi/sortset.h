//****************************************************************************************
//Sorted record sets
//****************************************************************************************

#if !defined(BB_API_SORTSET)
#define BB_API_SORTSET

#include "recset.h"

namespace dpt {

class SortRecordSet;

class APISortRecordSet : public APIRecordSet {
public:
	SortRecordSet* target;
	APISortRecordSet(SortRecordSet*);
	APISortRecordSet(const APISortRecordSet&);
	//-----------------------------------------------------------------------------------
};

} //close namespace

#endif
