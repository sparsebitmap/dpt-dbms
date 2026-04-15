//****************************************************************************************
//A kind of redumentary variant object, created when retrieving values from records
//and also to help reduce the number of API functions for things like finding and changing 
//records.  The user can specify either a number or a string.
//See API docs for more discussion/background about this class.
//****************************************************************************************

#if !defined(BB_API_FIELDVAL)
#define BB_API_FIELDVAL

#include <string>
#include "floatnum.h"

namespace dpt {

class FieldValue;

class APIFieldValue {
public:
	FieldValue* target;
	~APIFieldValue();
	//-----------------------------------------------------------------------------------

	//Default constructor will be string type with zero length
	APIFieldValue();

	APIFieldValue(const char*);
	APIFieldValue(const std::string&);
	APIFieldValue(const int&);
	APIFieldValue(const double&);
	APIFieldValue(const APIRoundedDouble&);
	APIFieldValue(const APIFieldValue&);
	APIFieldValue(const FieldValue&);

	void operator=(const char*);
	void operator=(const std::string&);
	void operator=(const int&);
	void operator=(const double&);
	void operator=(const APIRoundedDouble&);
	void operator=(const APIFieldValue&);
	//V2.10 - Dec 07.  Alternative names for the above (easier SWIG/Python interfaces).
	void Assign(const char*);
	void Assign(const std::string&);
	void Assign(const int&);
	void Assign(const double&);
	void Assign(const APIRoundedDouble&);
	void Assign(const APIFieldValue&);

	void Swap(APIFieldValue&);

	//Extractors working for any type
	APIRoundedDouble ExtractRoundedDouble(bool throw_non_num = false) const;
	double ExtractNumber() const {return ExtractRoundedDouble().Data();}
	std::string ExtractString() const;

	//Extractors for strings only
	//const unsigned char StrLen() const; //V3.0
	const int StrLen() const;
	//V3.0.  See comments in base fieldval.h about this.
	const char* CStr() const;

	//Comparisons rely on the two types being the same
	bool operator==(const APIFieldValue&) const;
	int Compare(const APIFieldValue&) const;

	bool CurrentlyNumeric() const;
	void ConvertToString();
	void ConvertToNumeric(bool throw_non_num = false);

	//Strings only
	int CompareRightJustifiedString(const APIFieldValue&) const;
};


} //close namespace

#endif
