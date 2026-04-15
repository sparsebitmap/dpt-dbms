
#include "stdafx.h"

#include "dbapi\reclist.h"
#include "reclist.h"

namespace dpt {

APIRecordList::APIRecordList(RecordList* t) 
: APIBitMappedRecordSet(t), target(t) {}
APIRecordList::APIRecordList(const APIRecordList& t) 
: APIBitMappedRecordSet(t), target(t.target) {}

//*******************************************
void APIRecordList::Place(const APIBitMappedRecordSet& set)
{
	target->Place(set.target);
}

void APIRecordList::Remove(const APIBitMappedRecordSet& set)
{
	target->Remove(set.target);
}

//*******************************************
void APIRecordList::Place(const APIReadableRecord& rec)
{
	target->Place(rec.target);
}

void APIRecordList::Remove(const APIReadableRecord& rec)
{
	target->Remove(rec.target);
}

} //close namespace


