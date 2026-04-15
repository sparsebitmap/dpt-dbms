
#include "stdafx.h"

#include "dbapi\bmset.h"
#include "bmset.h"

namespace dpt {

APIBitMappedRecordSet::APIBitMappedRecordSet(BitMappedRecordSet* t) 
: APIRecordSet(t), target(t) {}
APIBitMappedRecordSet::APIBitMappedRecordSet(const APIBitMappedRecordSet& t) 
: APIRecordSet(t), target(t.target) {}

//********************************************
APISortRecordSet APIBitMappedRecordSet::Sort(const APISortRecordsSpecification& spec)
{
	return APISortRecordSet(target->Sort(*(spec.target)));
}

} //close namespace


