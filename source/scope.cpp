
#include "stdafx.h"


namespace dpt {

//Parameters, commands, UL statements etc. can all fall into these categories
const char* SCOPE_NO_SECURITY = "DPT does not currently emulate all security features";
const char* SCOPE_SCHEDULING = "DPT thread scheduling is done by the operating system";
//V1.2 25/7/06
//Rephrased to say parameter/command not just parameter
const char* SCOPE_MEMORY = "DPT memory management does not use this parameter/command";
const char* SCOPE_NOT_APPLICABLE = "Not applicable/appropriate in this environment";
const char* SCOPE_CONFUSE = "A different name is used on DPT to clarify the difference from ";
const char* SCOPE_UNNECESSARY = "This lesser-used feature is not currently emulated";
const char* SCOPE_ROLL_FORWARD = "DPT does not implement roll forward recovery";
const char* SCOPE_ADVANCED = "DPT does not currently emulate some advanced features";
const char* SCOPE_DATABASE = "DPT files are internallly different to Model 204 files";

//specific to parameters
const char* SCOPE_SYSTEM_ID = "Not applicable in this environment, but see SYSNAME/SYSPORT";
const char* SCOPE_OUTMRL = "Assorted parameters like this have been merged into OUTMRL";

} //close namespace

