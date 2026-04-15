/********************************************************************************************
M204-style handling of floating point
********************************************************************************************/

#if !defined(BB_FLOAT)
#define BB_FLOAT

#include <string>

#ifdef _BBHOST
#include "string2.h" //V2.26.  Only using string2 in User Language for the time being.
#endif

namespace dpt {

enum {
	BB_UNROUNDED = -1,
	BB_15DSF	 = -2
};

class RunTimeState;

//******************************************************************************************
//One of these is just a double which is guaranteed to have an exponent less than
//+/- 75.  It is not necessarily rounded to 15 dsf - see following derived class.
//******************************************************************************************
class RangeCheckedDouble {
	static bool thrownumrange; //V2.04 Mar 07.  For API.
	static bool allowhex; //V3.03. Sick of UL not allowing this.

	const char* ToStringWithFixedDP(const int, char*) const;

protected:
	double data;
	//* * * V important - add no more data members - must be 8 bytes * * *

	//This quietly sets the value to zero if there are range problems (by default)
	void Set(const double&);

	//This throws exceptions if there are NaN or range violations
	void Set(const std::string&, bool = false);
#ifdef _BBHOST
	void Set(const std::string2&, bool = false);
#endif

	//There can't ever be any problems with these
	void Set(const int& d) {data = d;}
	void Set(const RangeCheckedDouble& d) {data = d.data;}

public:
	RangeCheckedDouble() : data(0.0) {}
	RangeCheckedDouble(const RangeCheckedDouble& d) {Set(d);}
	RangeCheckedDouble(const int& d) {Set(d);}
	RangeCheckedDouble(const double& d) {Set(d);}
	RangeCheckedDouble(const std::string& d) {Set(d);}

	static const double MAXIMUM_POSITIVE_VALUE;
	static const double MINIMUM_POSITIVE_VALUE;
	static const double MAXIMUM_NEGATIVE_VALUE;
	static const double MINIMUM_NEGATIVE_VALUE;

	RangeCheckedDouble& operator=(const int& d) {Set(d); return *this;}
	RangeCheckedDouble& operator=(const double& d) {Set(d); return *this;}
	RangeCheckedDouble& operator=(const std::string& s) {Set(s, true); return *this;}
	RangeCheckedDouble& operator=(const RangeCheckedDouble& d) {Set(d); return *this;}
	//V2.10 - Dec 07.  Alternative names for the above (easier SWIG/Python interfaces).
	RangeCheckedDouble& Assign(const int& d) {Set(d); return *this;}
	RangeCheckedDouble& Assign(const double& d) {Set(d); return *this;}
	RangeCheckedDouble& Assign(const std::string& s) {Set(s, true); return *this;}
	RangeCheckedDouble& Assign(const RangeCheckedDouble& d) {Set(d); return *this;}

	double Data() const {return data;}
	std::string ToStringForDisplay() const;
	int ToStringBufferForDisplay(char*) const;
	std::string ToStringWithFixedDP(const int dp) const {
		//The maximum length would be with exponents of 75.
		char buff[128]; return ToStringWithFixedDP(dp, buff);}

	bool operator==(const RangeCheckedDouble& d) const {return (data == d.data);}
	bool operator!=(const RangeCheckedDouble& d) const {return (data != d.data);}
	bool operator<(const RangeCheckedDouble& d) const {return (data < d.data);}
	bool operator<=(const RangeCheckedDouble& d) const {return (data <= d.data);}
	bool operator>(const RangeCheckedDouble& d) const {return (data > d.data);}
	bool operator>=(const RangeCheckedDouble& d) const {return (data >= d.data);}
//	const RangeCheckedDouble operator-() {return -data;}
	void Negate() {data = -data;} //less confusing

	static bool TooLargeNumber(const double& d) {return ((d >= 1.0E76) || (d <= -1.0E76));}
	static bool TooSmallNumber(const double& d) {return ((d < 1.0E-75) && (d > -1.0E-75));}

	//Testing against normal doubles is what we are really trying to avoid with 
	//these classes, but comparision with zero comes in handy.  Also setting to zero,
	//which can never be a problem.
	bool IsZero() const {return data == 0.0;}
	void SetToZero() {data = 0.0;}

	//V2.04
	static void SetNumRangeThrowOption(bool b) {thrownumrange = b;}
	//V3.03
	static void SetAllowHexOption(bool b) {allowhex = b;}

#ifdef _BBHOST
	RangeCheckedDouble(const std::string2& d) {Set(d);}
	RangeCheckedDouble& Assign(const std::string2& s) {Set(s, true); return *this;}
	RangeCheckedDouble& operator=(const std::string2& s) {Set(s, true); return *this;}
	std::string2 ToString2ForDisplay() const;
	std::string2 ToString2WithFixedDP(const int dp) const {
		char buff[128]; return ToStringWithFixedDP(dp, buff);}
#endif
};

//******************************************************************************************
//One of these is a double that is guaranteed to have only 15 dsf.
//******************************************************************************************
class RoundedDouble : public RangeCheckedDouble {
	
	//* * * V important - add no more data members - must be 8 bytes * * *

	//This rounds quietly
	void Round();

	//This throws an exception if there are too many dsf (amongst other reasons)
	void Check15DSF(const std::string&, bool);
#ifdef _BBHOST
	void Check15DSF(const std::string2&, bool);
#endif

	//THE UL RTVM works with unrounded values since they are sometimes needed.  But it
	//keeps track of which values are already rounded, and calls this to save re-rounding.
	friend class RunTimeState;
	RoundedDouble(const RangeCheckedDouble& d, int dp) : RangeCheckedDouble(d) {
														if (dp == BB_UNROUNDED) Round();}
public:
	RoundedDouble() : RangeCheckedDouble() {}
	RoundedDouble(const int& i) : RangeCheckedDouble(i) {}
	RoundedDouble(const RoundedDouble& rd) : RangeCheckedDouble(rd) {}
	RoundedDouble(const RangeCheckedDouble& rcd) : RangeCheckedDouble(rcd) {Round();}
	RoundedDouble(const double& d) : RangeCheckedDouble(d) {Round();}
	RoundedDouble(const std::string& s) : RangeCheckedDouble(s) {Check15DSF(s, false);}

	RoundedDouble& operator=(const RoundedDouble& d) {RangeCheckedDouble::Set(d); return *this;}
	RoundedDouble& operator=(const RangeCheckedDouble& d) {RangeCheckedDouble::Set(d); Round(); return *this;}
	RoundedDouble& operator=(const int& d) {RangeCheckedDouble::Set(d); return *this;}
	RoundedDouble& operator=(const double& d) {RangeCheckedDouble::Set(d); Round(); return *this;}
	RoundedDouble& operator=(const std::string& s) {RangeCheckedDouble::Set(s, true); Check15DSF(s, true); return *this;}

	const RoundedDouble operator-() {return -Data();}

#ifdef _BBHOST
	RoundedDouble(const std::string2& s) : RangeCheckedDouble(s) {Check15DSF(s, false);}
	RoundedDouble& operator=(const std::string2& s) {RangeCheckedDouble::Set(s, true); Check15DSF(s, true); return *this;}
#endif
};

} //close namespace

#endif
