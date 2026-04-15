//****************************************************************************************
//Found sets and lists, but not sorted sets
//****************************************************************************************

#if !defined(BB_API_BMSET)
#define BB_API_BMSET

#include "recset.h"
#include "sortset.h"
#include "sortspec.h"

namespace dpt {

class BitMappedRecordSet;

class APIBitMappedRecordSet : public APIRecordSet {
public:
	BitMappedRecordSet* target;
	APIBitMappedRecordSet(BitMappedRecordSet*);
	APIBitMappedRecordSet(const APIBitMappedRecordSet&);
	//-----------------------------------------------------------------------------------

	APISortRecordSet Sort(const APISortRecordsSpecification&);
};

} //close namespace

#endif
