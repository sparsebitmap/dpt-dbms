//******************************************************************************************
//This file defines some constants that are used in various places when explaining that
//certain features are not emulated, and why.
//******************************************************************************************

#if !defined BB_SCOPE
#define BB_SCOPE

namespace dpt {

//Parameters, commands, UL statements etc. can all fall into these categories
extern const char* SCOPE_NO_SECURITY;
extern const char* SCOPE_SCHEDULING;
extern const char* SCOPE_MEMORY;
extern const char* SCOPE_NOT_APPLICABLE;
extern const char* SCOPE_CONFUSE;
extern const char* SCOPE_UNNECESSARY;
extern const char* SCOPE_ROLL_FORWARD;
extern const char* SCOPE_ADVANCED;
extern const char* SCOPE_DATABASE;

//specific to parameters
extern const char* SCOPE_SYSTEM_ID;
extern const char* SCOPE_OUTMRL;

} //close namespace



#endif
