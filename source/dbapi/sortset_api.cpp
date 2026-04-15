
#include "stdafx.h"

#include "dbapi\sortset.h"
#include "sortset.h"

namespace dpt {

APISortRecordSet::APISortRecordSet(SortRecordSet* t) 
: APIRecordSet(t), target(t) {}
APISortRecordSet::APISortRecordSet(const APISortRecordSet& t) 
: APIRecordSet(t), target(t.target) {}

} //close namespace


