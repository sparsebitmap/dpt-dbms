
#include "stdafx.h"

#include "dbapi\findspec.h"
#include "dbapi\bmset.h"
#include "findspec.h"

namespace dpt {

APIFindSpecification::APIFindSpecification(const APIFindSpecification& from)
: target(new FindSpecification)
{
	//Assignment or or copy constructor.  No jiggery-pokery with reference counting
	//as there are lots of modifier and combiner operations.  We would have to get 
	//involved with freezing etc. for it all to make logical sense.
	*(target) = *(from.target);
}

APIFindSpecification::APIFindSpecification()
: target(new FindSpecification)
{}

APIFindSpecification::~APIFindSpecification()
{
	if (target)
		delete target;
}

//************************************************************
APIFindSpecification::APIFindSpecification
(const std::string& f, const FindOperator& o, const APIFieldValue& v)
: target(new FindSpecification(f, o, *(v.target)))
{}

APIFindSpecification::APIFindSpecification
(const std::string& f, const FindOperator& o, const APIFieldValue& v1, const APIFieldValue& v2)
: target(new FindSpecification(f, o, *(v1.target), *(v2.target)))
{}

APIFindSpecification::APIFindSpecification(const FindOperator& o, const int& n)
: target(new FindSpecification(o, n))
{}

APIFindSpecification::APIFindSpecification(const FindOperator& o, const std::string& s)
: target(new FindSpecification(o, s))
{}

APIFindSpecification::APIFindSpecification(APIBitMappedRecordSet& s)
: target(new FindSpecification(FD_SET$, reinterpret_cast<unsigned _int32>(s.target)))
{}

APIFindSpecification::APIFindSpecification(const std::string& n, const FindOperator& o)
: target(new FindSpecification(n, o))
{}

//V3.0
APIFindSpecification::APIFindSpecification(const char* expr)
: target(new FindSpecification(expr))
{}


//************************************************************
APIFindSpecification& APIFindSpecification::operator&=(const APIFindSpecification& s)
{
	*(target) &= *(s.target);
	return *this;
}

APIFindSpecification& APIFindSpecification::operator|=(const APIFindSpecification& s)
{
	*(target) |= *(s.target);
	return *this;
}

APIFindSpecification APIFindSpecification::operator!()
{
	APIFindSpecification temp(*this);
	temp.Negate();
	return temp;
}


//************************************************************
APIFindSpecification APIFindSpecification::Splice
	(APIFindSpecification& left, APIFindSpecification& right, bool optr_and)
{
	FindSpecification* s = FindSpecification::Splice(left.target, right.target, optr_and);

	left.target = NULL;
	right.target = NULL;

	//How to return this without invoking a whole copy
	return APIFindSpecification(s);
}

void APIFindSpecification::Negate()
{
	target->Negate();
}



//************************************************************
APIFindSpecification operator&
(const APIFindSpecification& lhs, const APIFindSpecification& rhs) 
{
	APIFindSpecification result(lhs);
	result &= rhs;
	return result;
} 

APIFindSpecification operator|
(const APIFindSpecification& lhs, const APIFindSpecification& rhs) 
{
	APIFindSpecification result(lhs);
	result |= rhs;
	return result;
} 





//***************************************************************************************
APIFindValuesSpecification::APIFindValuesSpecification(const APIFindValuesSpecification& from)
: target(new FindValuesSpecification(from.target->FieldName()))
{}

APIFindValuesSpecification::APIFindValuesSpecification(const std::string& fname)
: target(new FindValuesSpecification(fname))
{}

APIFindValuesSpecification::APIFindValuesSpecification(const char* fname)
: target(new FindValuesSpecification(fname))
{}

//One-ended range, or a pattern
APIFindValuesSpecification::APIFindValuesSpecification
(const std::string& fname, const FindOperator& o, const APIFieldValue& v1)
: target(new FindValuesSpecification(fname, o, *(v1.target)))
{}

//Two-ended range
APIFindValuesSpecification::APIFindValuesSpecification
(const std::string& fname, const FindOperator& o, const APIFieldValue& v1, const APIFieldValue& v2)
: target(new FindValuesSpecification(fname, o, *(v1.target), *(v2.target)))
{}

//Special form to allow both a range and a pattern to be given
APIFindValuesSpecification::APIFindValuesSpecification
(const std::string& fname, const FindOperator& o, 
	const std::string& lo, const std::string& hi, 
	const std::string& pattstring, bool notlike)
: target(new FindValuesSpecification(fname, o, lo, hi, pattstring, notlike))
{}



//***************************************************************************************
void APIFindSpecification::Dump(std::vector<std::string>& vs) const
{
	target->Dump(vs);
}
void APIFindSpecification::SetRunTimeDiagnosticLevel(unsigned int i)
{
	target->SetRunTimeDiagnosticLevel(i);
}

void APIFindValuesSpecification::Dump(std::vector<std::string>& vs) const
{
	target->Dump(vs);
}
std::string APIFindValuesSpecification::FieldName()
{
	return target->FieldName();
}

} //close namespace


