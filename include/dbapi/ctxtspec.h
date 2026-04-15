//**************************************************************************************
//This class just performs a parsing function to determine what is intended by a context
//specification in e.g. "IN xxxxx" or "OPEN xxxxx" situations.
//The usual Model 204 style rules are applied if no explicit context type is given
//as part of the string, namely to take in order of priority:
//1. User-specific (TEMP) group
//2. System-wide (PERM) group
//3. Single file
//**************************************************************************************

#if !defined(BB_API_CTXTSPEC)
#define BB_API_CTXTSPEC

#include <string>

namespace dpt {

class ContextSpecification;

//Create with e.g. ContextSpecification("FILE MYFILE").  In many cases implicit 
//construction works and is neater - e.g. OpenContext("FILE MYFILE").
class APIContextSpecification {
public:
	ContextSpecification* target;
	APIContextSpecification(const std::string&);
	APIContextSpecification(const char*);
	APIContextSpecification(const APIContextSpecification&);
	~APIContextSpecification();
	//-----------------------------------------------------------------------------------
};

} //close namespace

#endif
