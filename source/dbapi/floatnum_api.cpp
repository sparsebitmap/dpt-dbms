
#include "stdafx.h"

#include "dbapi\floatnum.h"
#include "bbfloat.h"

namespace dpt {

APIRoundedDouble::~APIRoundedDouble()
{
	if (target)
		delete target;
}

//************************************************
APIRoundedDouble::APIRoundedDouble() : target(new RoundedDouble) {}
APIRoundedDouble::APIRoundedDouble(const int& v) : target(new RoundedDouble(v)) {}
APIRoundedDouble::APIRoundedDouble(const RoundedDouble& v) : target(new RoundedDouble(v)) {}
APIRoundedDouble::APIRoundedDouble(const double& v) : target(new RoundedDouble(v)) {}
APIRoundedDouble::APIRoundedDouble(const std::string& v) : target(new RoundedDouble(v)) {}

//************************************************
APIRoundedDouble& APIRoundedDouble::operator=(const RoundedDouble& rhs) 
{*target = rhs; return *this;}
APIRoundedDouble& APIRoundedDouble::operator=(const int& rhs)
{*target = rhs; return *this;}
APIRoundedDouble& APIRoundedDouble::operator=(const double& rhs)
{*target = rhs; return *this;}
APIRoundedDouble& APIRoundedDouble::operator=(const std::string& rhs)
{*target = rhs; return *this;}

//V2.10 - Dec 07.  Alternative names for the above (easier SWIG/Python interfaces).
//V2.15 Correctly use rounding constructors, not just range-checking.
APIRoundedDouble& APIRoundedDouble::Assign(const RoundedDouble& rhs) 
//{target->Assign(rhs); return *this;}
{return operator=(rhs);}
APIRoundedDouble& APIRoundedDouble::Assign(const int& rhs)
//{target->Assign(rhs); return *this;}
{return operator=(rhs);}
APIRoundedDouble& APIRoundedDouble::Assign(const double& rhs)
//{target->Assign(rhs); return *this;}
{return operator=(rhs);}
APIRoundedDouble& APIRoundedDouble::Assign(const std::string& rhs)
//{target->Assign(rhs); return *this;}
{return operator=(rhs);}

//************************************************
const APIRoundedDouble APIRoundedDouble::operator-()
{
	return APIRoundedDouble(target->operator-());
}

//************************************************
double APIRoundedDouble::Data() const
{
	return target->Data();
}

std::string APIRoundedDouble::ToStringForDisplay() const
{
	return target->ToStringForDisplay();
}

std::string APIRoundedDouble::ToStringWithFixedDP(const int dp) const
{
	return target->ToStringWithFixedDP(dp);
}

//************************************************
void APIRoundedDouble::SetNumRangeThrowOption(bool b)
{
	RangeCheckedDouble::SetNumRangeThrowOption(b);
}


} //close namespace


