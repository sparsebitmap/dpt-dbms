//********************************************************************************************
//M204 User Language style handling of floating point
//********************************************************************************************

#if !defined(BB_API_FLOATNUM)
#define BB_API_FLOATNUM

#include <string>

namespace dpt {

class RoundedDouble;

class APIRoundedDouble {
public:
	RoundedDouble* target;
	~APIRoundedDouble();
	//-----------------------------------------------------------------------------------

	APIRoundedDouble();
	APIRoundedDouble(const int&);
	APIRoundedDouble(const RoundedDouble&);
	APIRoundedDouble(const double&);
	APIRoundedDouble(const std::string&);

	APIRoundedDouble& operator=(const RoundedDouble&);
	APIRoundedDouble& operator=(const int&);
	APIRoundedDouble& operator=(const double&);
	APIRoundedDouble& operator=(const std::string&);
	//V2.10 - Dec 07.  Alternative names for the above (easier SWIG/Python interfaces).
	APIRoundedDouble& Assign(const RoundedDouble&);
	APIRoundedDouble& Assign(const int&);
	APIRoundedDouble& Assign(const double&);
	APIRoundedDouble& Assign(const std::string&);

	const APIRoundedDouble operator-();

	double Data() const;
	std::string ToStringForDisplay() const;
	std::string ToStringWithFixedDP(const int) const;

	static void SetNumRangeThrowOption(bool b = true);
};


} //close namespace

#endif
