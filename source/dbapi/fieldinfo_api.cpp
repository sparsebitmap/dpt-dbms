
#include "stdafx.h"

#include "dbapi\fieldinfo.h"
#include "fieldinfo.h"

namespace dpt {

APIFieldAttributeCursor::APIFieldAttributeCursor(FieldAttributeCursor* t) 
: APICursor(t), target(t) {}
APIFieldAttributeCursor::APIFieldAttributeCursor(const APIFieldAttributeCursor& t) 
: APICursor(t), target(t.target) {}

//*******************************************
int APIFieldAttributeCursor::NumFields()
{
	return target->NumFields();
}

std::string APIFieldAttributeCursor::Name()
{
	return *(target->Name());
}

APIFieldAttributes APIFieldAttributeCursor::Atts()
{
	return APIFieldAttributes(target->Atts());
}

short APIFieldAttributeCursor::FID()
{
	return *(target->FID());
}


} //close namespace


