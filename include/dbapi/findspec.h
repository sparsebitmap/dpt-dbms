
#if !defined(BB_API_FINDSPEC)
#define BB_API_FINDSPEC

#include <string>
#include <vector>
#include "apiconst.h"
#include "fieldval.h"

namespace dpt {

class FindSpecification;
class APIBitMappedRecordSet;

class APIFindSpecification {
	APIFindSpecification(FindSpecification* t) : target(t) {}
public:
	FindSpecification* target;
	APIFindSpecification(const APIFindSpecification&);
	virtual ~APIFindSpecification();
	//-----------------------------------------------------------------------------------

	//All records
	APIFindSpecification();

	//Single operand - e.g. F = V
	APIFindSpecification(const std::string&, const FindOperator&, const APIFieldValue&);

	//Two operands - e.g. F between V1 and V2
	APIFindSpecification(const std::string&, const FindOperator&, 
							const APIFieldValue&, const APIFieldValue&);

	//Point$ or single record number
	APIFindSpecification(const FindOperator&, const int&);

	//FILE$
	APIFindSpecification(const FindOperator&, const std::string&);

	//Find$ or List$
	APIFindSpecification(APIBitMappedRecordSet&);

	//Field presence (or a value find with no criteria)
	APIFindSpecification(const std::string&, const FindOperator&);

	//V3.0: Arbitrarily complex query expressed as a string
	APIFindSpecification(const char*);

	//-----------------------------------------------------------------------------------
	//Combiners: Convenient and readable, but involving more temporaries
	APIFindSpecification& operator&=(const APIFindSpecification&);
	APIFindSpecification& operator|=(const APIFindSpecification&);
	APIFindSpecification operator!();
	void Negate();

	//Alternative which is less elegant but does less heap work.  The two inputs variables
	//become invalid afterwards.  The UL compiler uses this.
	static APIFindSpecification Splice
		(APIFindSpecification& left, APIFindSpecification& right, bool optr_and);

	//Diagnostics
	void Dump(std::vector<std::string>&) const;
	void SetRunTimeDiagnosticLevel(unsigned int);
};

//Binary combiners
APIFindSpecification operator&(const APIFindSpecification&, const APIFindSpecification&); 
APIFindSpecification operator|(const APIFindSpecification&, const APIFindSpecification&); 


//**************************************************************************************
//A special variation for FindValues and CountValues
//**************************************************************************************
class FindValuesSpecification;

class APIFindValuesSpecification {
public:
	FindValuesSpecification* target;
	APIFindValuesSpecification(const APIFindValuesSpecification&);
	~APIFindValuesSpecification() {}
	//-----------------------------------------------------------------------------------

	//All values of field
	APIFindValuesSpecification(const char* fname);
	APIFindValuesSpecification(const std::string& fname);

	//One-ended range, or a pattern
	APIFindValuesSpecification(const std::string& fname, const FindOperator& o, 
								const APIFieldValue& v1);

	//Two-ended range
	APIFindValuesSpecification(const std::string& fname, const FindOperator& o, 
								const APIFieldValue& v1, const APIFieldValue& v2);

	//Special form to allow both a range and a pattern to be given
	APIFindValuesSpecification(const std::string& fname, const FindOperator& o, 
								const std::string& lo, const std::string& hi, 
								const std::string& pattstring, bool notlike = false);

	std::string FieldName();
	void Dump(std::vector<std::string>&) const;
};


} //close namespace

#endif
