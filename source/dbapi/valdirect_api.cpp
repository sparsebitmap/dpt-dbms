
#include "stdafx.h"

#include "dbapi\valdirect.h"
#include "valdirect.h"

namespace dpt {

APIDirectValueCursor::APIDirectValueCursor(DirectValueCursor* t) 
: APICursor(t), target(t) {}
APIDirectValueCursor::APIDirectValueCursor(const APIDirectValueCursor& t) 
: APICursor(t), target(t.target) {}

//********************************
bool APIDirectValueCursor::GetCurrentValue(APIFieldValue& v)
{
	return target->GetCurrentValue(*(v.target));
}

APIFieldValue APIDirectValueCursor::GetCurrentValue()
{
	APIFieldValue v;
	GetCurrentValue(v);
	return v;
}

APIDirectValueCursor APIDirectValueCursor::CreateClone()
{
	return APIDirectValueCursor(target->CreateClone());
}


//********************************
APIFieldAttributes APIDirectValueCursor::GetFieldAtts()
{
	return APIFieldAttributes(target->GetFieldAtts());
}

//********************************
void APIDirectValueCursor::SetDirection(CursorDirection cd)
{
	target->SetDirection(cd);
}

void APIDirectValueCursor::SetRestriction_LoLimit(const APIFieldValue& v, bool i)
{
	target->SetRestriction_LoLimit(*(v.target), i);
}

void APIDirectValueCursor::SetRestriction_HiLimit(const APIFieldValue& v, bool i)
{
	target->SetRestriction_HiLimit(*(v.target), i);
}

void APIDirectValueCursor::SetRestriction_Pattern(const std::string& p, bool notlike)
{
	target->SetRestriction_Pattern(p, notlike);
}

//******************************************
bool APIDirectValueCursor::Accessible()
{
	return target->Accessible();
}

void APIDirectValueCursor::Advance(int delta)
{
	target->Advance(delta);
}

void APIDirectValueCursor::GotoFirst()
{
	target->GotoFirst();
}

void APIDirectValueCursor::GotoLast()
{
	target->GotoLast();
}

//**************************************************
void APIDirectValueCursor::AdoptPositionFrom(APIDirectValueCursor& from)
{
	target->AdoptPositionFrom(*(from.target));
}

void APIDirectValueCursor::SwapPositionWith(APIDirectValueCursor& from)
{
	target->SwapPositionWith(*(from.target));
}

bool APIDirectValueCursor::SetPosition(const APIFieldValue& v)
{
	return target->SetPosition(*(v.target));
}

} //close namespace


