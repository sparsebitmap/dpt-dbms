//**************************************************************************************
//This class just performs a parsing function to determine what is intended by a context
//specification in e.g. "IN xxxxx" or "OPEN xxxxx" situations.
//The usual Model 204 style rules are applied if no explicit context type is given
//as part of the string, namely to take in order of priority:
//1. User-specific (TEMP) group
//2. System-wide (PERM) group
//3. Single file
//**************************************************************************************

#if !defined(BB_CTXTSPEC)
#define BB_CTXTSPEC

#include <string>

namespace dpt {

class ULWorkingStorageReference;

class ContextSpecification {
public:
	enum {
		cspec_single_file, 
		cspec_temp_group, 
		cspec_perm_group, 
		cspec_dynamic_any, 
		cspec_dynamic_group
#ifdef _BBHOST
,		cspec_$CURFILE,
		cspec_$UPDATE,
		cspec_adhoc_group
#endif
	} level;

	std::string short_name;
	std::string basic_spec_string;
	ULWorkingStorageReference* membervar;

	void ContextSpecification_S(const std::string& s, int* cursor);

	//Create one of these with e.g. ContextSpecification("FILE MYFILE").  In many
	//cases implicit construction e.g. OpenContext("FILE MYFILE") is neater.
	ContextSpecification(const char* s, int* cursor = NULL) {
		ContextSpecification_S(s, cursor);}
	ContextSpecification(const std::string& s, int* cursor = NULL) {
		ContextSpecification_S(s, cursor);}

	~ContextSpecification();

	//Mar 07.  For use by the api.
	int refcount;

	//Special contexts in UL such as $UPDATE and GROUP MEMBER.
#ifdef _BBHOST
	ContextSpecification(const std::string& s, int* cursor, bool);

	bool IsULSpecial() {return 
		level == cspec_$CURFILE || 
		level == cspec_$UPDATE || 
		level == cspec_adhoc_group || 
		membervar != NULL;}

	//Used in the User Language OPEN statement
	void SetRunTimeFileName(const std::string&);
#endif
};

} //close namespace

#endif
