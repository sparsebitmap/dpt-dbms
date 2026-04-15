
#include "stdafx.h"

#include "dbapi\fieldval.h"
#include "fieldval.h"
#include "except.h"
#include "msg_db.h"

namespace dpt {

//In the API I think it's nicer if the correct types are ensured more rigorously
//than with an assert like in the internal API.
void APIThrowFVBadType()
{
	throw Exception(DML_NON_STRING_ERROR, 
		"Unexpected or mismatched data type(s) for the APIFieldValue function called"); 
}

APIFieldValue::~APIFieldValue()
{
	if (target)
		delete target;
}

APIFieldValue::APIFieldValue() : target(new FieldValue) {}

//*********************************************
APIFieldValue::APIFieldValue(const char* v) : target(new FieldValue(v)) {}
APIFieldValue::APIFieldValue(const std::string& v) : target(new FieldValue(v)) {}
APIFieldValue::APIFieldValue(const int& v) : target(new FieldValue(v)) {}
APIFieldValue::APIFieldValue(const double& v) : target(new FieldValue(v)) {}
APIFieldValue::APIFieldValue(const APIRoundedDouble& v) : target(new FieldValue(*(v.target))) {}
APIFieldValue::APIFieldValue(const APIFieldValue& from) 
: target(new FieldValue(*(from.target))) {}
APIFieldValue::APIFieldValue(const FieldValue& from) 
: target(new FieldValue(from)) {}

//*********************************************
void APIFieldValue::operator=(const char* rhs) {*target = rhs;}
void APIFieldValue::operator=(const std::string& rhs) {*target = rhs;}
void APIFieldValue::operator=(const int& rhs) {*target = rhs;}
void APIFieldValue::operator=(const double& rhs) {*target = rhs;}
void APIFieldValue::operator=(const APIRoundedDouble& rhs) {*target = *(rhs.target);}
void APIFieldValue::operator=(const APIFieldValue& rhs) {*target = *(rhs.target);}

//V2.10 - Dec 07.  Alternative names for the above (easier SWIG/Python interfaces).
void APIFieldValue::Assign(const char* rhs) {target->Assign(rhs);}
void APIFieldValue::Assign(const std::string& rhs) {target->Assign(rhs);}
void APIFieldValue::Assign(const int& rhs) {target->Assign(rhs);}
void APIFieldValue::Assign(const double& rhs) {target->Assign(rhs);}
void APIFieldValue::Assign(const APIRoundedDouble& rhs) {target->Assign(*(rhs.target));}
void APIFieldValue::Assign(const APIFieldValue& rhs) {target->Assign(*(rhs.target));}

//*********************************************
void APIFieldValue::Swap(APIFieldValue& rhs)
{
	target->Swap(*(rhs.target));
}


//*********************************************
APIRoundedDouble APIFieldValue::ExtractRoundedDouble(bool throw_non_num) const 
{
	return APIRoundedDouble(target->ExtractRoundedDouble(throw_non_num));
}

std::string APIFieldValue::ExtractString() const
{
	return target->ExtractString();
}


//*********************************************
//const unsigned char APIFieldValue::StrLen() const //V3.0
const int APIFieldValue::StrLen() const
{
	if (target->CurrentlyNumeric())
		APIThrowFVBadType();
	return target->StrLen();
}

const char* APIFieldValue::CStr() const
{
	if (target->CurrentlyNumeric())
		APIThrowFVBadType();
	return target->CStr();
}


//*********************************************
bool APIFieldValue::CurrentlyNumeric() const
{
	return target->CurrentlyNumeric();
}

void APIFieldValue::ConvertToString()
{
	target->ConvertToString();
}

void APIFieldValue::ConvertToNumeric(bool throw_non_num)
{
	target->ConvertToNumeric(throw_non_num);
}


bool APIFieldValue::operator==(const APIFieldValue& rhs) const
{
	if (target->CurrentlyNumeric() != rhs.target->CurrentlyNumeric())
		APIThrowFVBadType();
	return target->operator==(*(rhs.target));
}

int APIFieldValue::Compare(const APIFieldValue& rhs) const
{
	if (target->CurrentlyNumeric() != rhs.target->CurrentlyNumeric())
		APIThrowFVBadType();
	return target->Compare(*(rhs.target));
}


//*********************************************
int APIFieldValue::CompareRightJustifiedString(const APIFieldValue& rhs) const
{
	if (target->CurrentlyNumeric())
		APIThrowFVBadType();
	return target->CompareRightJustifiedString(*(rhs.target));
}

} //close namespace


