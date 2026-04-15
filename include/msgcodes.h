//*********************************************************************
// 
// Ranges of message codes used throughout DPT.
//
// See also more specific details in msg_core, msg_db, msg_ul etc.
//
//*********************************************************************

#if !defined(BB_MSGCODES)
#define BB_MSGCODES

namespace dpt {

//*********************************************************************
const int CORE_FIRST = 0;  //actually unused, and as of V2.25 means OK in CAPI.
const int CORE_LAST  = 999;

const int UTIL_FIRST = 1000;
const int UTIL_LAST  = 1499;

const int FILE_FIRST = 1500;
const int FILE_LAST  = 1999;

const int PROC_FIRST = 2000;
const int PROC_LAST  = 2499;

const int SEQ_FIRST  = 2500;
const int SEQ_LAST   = 2999;

const int DB_FIRST   = 3000;
const int DB_LAST    = 3999;

const int SESS_FIRST = 4000;
const int SESS_LAST  = 4499;

const int CMD_FIRST  = 4500;
const int CMD_LAST   = 4999;

const int UL_FIRST   = 5000;
const int UL_LAST    = 5999;

//V2.24 
const int APIS_FIRST = 6000;
//6001-6020 = C API codes
//6051-6070 = Java API codes
const int APIS_LAST  = 6100;

//   ^    ^
//   |    |
//Unused range
//   |    |
//   v    v 

//For user use
const int CUSTOM_FIRST  = 8000;
const int CUSTOM_LAST   = 8999;

//*********************************************************************
const int MISC_FIRST                = 9000;

//Resulting from OS problems
const int MISC_CAUGHT_STL			= MISC_FIRST + 1;
const int MISC_CAUGHT_UNKNOWN		= MISC_FIRST + 2;
const int MISC_OS_ROUTINE_ERROR		= MISC_FIRST + 3;
const int MISC_VIRTUAL_MEMORY		= MISC_FIRST + 4;

//Internal errors - these in theory never happen
const int BUG_MISC					= MISC_FIRST + 101;
const int BUG_STRING_TOO_LONG		= MISC_FIRST + 102;
const int BUG_FAT_INTERFACE_BAD_CALL= MISC_FIRST + 103;

//Not sure if these could happen or not
const int MISC_SINGLETON			= MISC_FIRST + 201;

//Debugging messages
const int MISC_DEBUG_INFO           = MISC_FIRST + 401;
const int MISC_DEBUG_ERROR          = MISC_FIRST + 402;
const int MISC_DEBUG_USRAUDIT_INFO  = MISC_FIRST + 403;
const int MISC_DEBUG_USRAUDIT_ERROR = MISC_FIRST + 404;
const int FUNC_UNDER_CONSTRUCTION   = MISC_FIRST + 404;

const int MISC_LAST  = 9999;

//*********************************************************************
const int CLIENT_FIRST  = 30000;
const int CLIENT_LAST	= 39999;

}	//close namespace

#endif
