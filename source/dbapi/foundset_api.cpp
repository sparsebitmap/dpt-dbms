
#include "stdafx.h"

#include "dbapi\foundset.h"
#include "foundset.h"

namespace dpt {

APIFoundSet::APIFoundSet(FoundSet* t) 
: APIBitMappedRecordSet(t), target(t) {}
APIFoundSet::APIFoundSet(const APIFoundSet& t) 
: APIBitMappedRecordSet(t), target(t.target) {}

void APIFoundSet::Unlock()
{
	target->Unlock();
}

} //close namespace


