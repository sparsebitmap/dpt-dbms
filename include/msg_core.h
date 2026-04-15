
#if !defined(BB_MSG_CORE)
#define BB_MSG_CORE

#include "msgcodes.h"

namespace dpt {

//User start up, close down etc.
const int USER_FIRST				= CORE_FIRST + 1;
const int USER_STARTED_LEVEL1		= CORE_FIRST + 1;
const int USER_FINISHED_LEVEL1		= CORE_FIRST + 2;
const int USER_TOO_MANY_NUSERS		= CORE_FIRST + 3;
const int USER_SETFORBUMP			= CORE_FIRST + 4;
const int USER_HEEDINGBUMP			= CORE_FIRST + 5;
const int USER_HEEDINGBUMP_FINAL	= CORE_FIRST + 6;
const int USER_BADNUMBER			= CORE_FIRST + 7;
const int USER_BADID				= CORE_FIRST + 8;
const int USER_QUIESCE_STARTED		= CORE_FIRST + 9;
const int USER_QUIESCE_ENDED		= CORE_FIRST + 10;
const int USER_HEEDINGQUIESCE		= CORE_FIRST + 11;
const int USER_PLEASEHEEDQUIESCE	= CORE_FIRST + 12;
const int USER_DUPETHREAD			= CORE_FIRST + 13;
const int USER_TOO_MANY_ERRORS		= CORE_FIRST + 14;
const int USER_IO_ERROR				= CORE_FIRST + 15;
const int USER_BEGINTHREAD_ERROR	= CORE_FIRST + 16;
const int USER_PRIORITY_CHANGE		= CORE_FIRST + 17;
const int USER_LAST					= CORE_FIRST + 99;

//Parameters
const int PARM_FIRST				= CORE_FIRST + 101;
const int PARM_BADNAME				= CORE_FIRST + 101;	//1122
const int PARM_NOTRESETTABLE		= CORE_FIRST + 102;	//1123, (plus maybe 1463 etc)
const int PARM_BADVALUE				= CORE_FIRST + 103;	//1123
const int PARM_USEDMAXORMIN			= CORE_FIRST + 104;	//1149
const int PARM_ATTRCONFLICT			= CORE_FIRST + 105;	//1245 (i.e. bit settings)
const int PARM_PARMCONFLICT			= CORE_FIRST + 106;	//many, e.g. 2304
const int PARM_BADTIME				= CORE_FIRST + 107;	//many, e.g. 2131
const int PARM_MISC					= CORE_FIRST + 108;	//e.g. 2101 perhaps
const int PARM_NEEDS_FILECONTEXT	= CORE_FIRST + 109;	//n/a
const int PARM_NOT_EMULATED			= CORE_FIRST + 110;	//n/a
const int PARM_LAST					= CORE_FIRST + 199;

//Statistics
const int STAT_FIRST				= CORE_FIRST + 201;
const int STAT_BADNAME				= CORE_FIRST + 201;
const int STAT_NOT_FILE				= CORE_FIRST + 202;
const int STAT_NEED_FILECONTEXT		= CORE_FIRST + 204;
const int STAT_MISC					= CORE_FIRST + 205;
const int STAT_LAST					= CORE_FIRST + 299;

//System start up, close down etc.
const int SYS_FIRST					= CORE_FIRST + 301;
const int SYS_UNKNOWN				= CORE_FIRST + 301;
const int SYS_STARTUP_ERROR			= CORE_FIRST + 302;
const int SYS_BAD_INIFILE			= CORE_FIRST + 303;
const int SYS_SETFORQUIESCE			= CORE_FIRST + 304;
const int SYS_QUIESCECOMPLETE		= CORE_FIRST + 305;
const int SYS_LAST					= CORE_FIRST + 399;

//Access control
const int ACCESS_FIRST				= CORE_FIRST + 401;
const int ACCOUNT_ALREADY_EXISTS	= CORE_FIRST + 401;
const int ACCOUNT_DOES_NOT_EXIST    = CORE_FIRST + 402;
const int ACCESS_ACCOUNT_BADFORMAT  = CORE_FIRST + 403;
const int ACCESS_PASSWORD_BADFORMAT = CORE_FIRST + 404;
const int ACCESS_CONTROL_UPDATED	= CORE_FIRST + 405;
const int ACCESS_CONTROL_BADDATA	= CORE_FIRST + 406;
const int LOGIN_FAILED              = CORE_FIRST + 407;
const int LOGIN_SUCCESS             = CORE_FIRST + 408;
const int ACCESS_INSUFFICIENT_PRIVS = CORE_FIRST + 409;

//Miscellaneous core
const int GARBAGE_ERROR				= CORE_FIRST + 901;
const int CUSTOM_FEATURE_ERROR		= CORE_FIRST + 902;
const int CUSTOM_FEATURE_INFO		= CORE_FIRST + 903;
const int USER_SET_MSG_HWM			= CORE_FIRST + 904;

}	//close namespace

#endif
