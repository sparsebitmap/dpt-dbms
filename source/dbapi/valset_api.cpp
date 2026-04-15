
#include "stdafx.h"

#include "dbapi\valset.h"
#include "dbapi\fieldval.h"
#include "valset.h"

namespace dpt {

APIValueSetCursor::APIValueSetCursor(ValueSetCursor* t) 
: APICursor(t), target(t) {}
APIValueSetCursor::APIValueSetCursor(const APIValueSetCursor& t) 
: APICursor(t), target(t.target) {}

bool APIValueSetCursor::GetCurrentValue(APIFieldValue& v)
{
	return target->GetCurrentValue(*(v.target));
}

APIFieldValue APIValueSetCursor::GetCurrentValue()
{
	APIFieldValue v;
	GetCurrentValue(v);
	return v;
}

APIValueSetCursor* APIValueSetCursor::CreateClone()
{
	return new APIValueSetCursor(target->CreateClone());
}

//*******************************************
APIValueSetCursor APIValueSet::OpenCursor()
{
	//As with record set
	bool gotofirst = true;
	return target->OpenCursor(gotofirst);
}

void APIValueSet::CloseCursor(const APIValueSetCursor& c)
{
	target->CloseCursor(c.target);
}


//*******************************************
int APIValueSet::Count()
{
	return target->Count();
}

void APIValueSet::Clear()
{
	target->Clear();
}


//*******************************************
int APIValueSet::NumGroupValueSets()
{
	return target->NumGroupValueSets();
}


//*******************************************
void APIValueSet::Sort(SortType type, SortDirection dir)
{
	target->Sort(type, dir);
}

APIValueSet APIValueSet::CreateSortedCopy(SortType type, SortDirection dir) const
{
	return APIValueSet(target->CreateSortedCopy(type, dir));
}

//*******************************************
APIFieldValue APIValueSet::AccessRandomValue(int setix)
{
	return APIFieldValue(target->AccessRandomValue(setix));
}













} //close namespace


