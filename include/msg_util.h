
#if !defined(BB_MSG_UTIL)
#define BB_MSG_UTIL

#include "msgcodes.h"

namespace dpt {

const int UTIL_BAD_PATTERN			= UTIL_FIRST + 1; //M204.1688-1691, 1963
const int UTIL_LINEIO_ERROR			= UTIL_FIRST + 2; //Would probably never be issued as such
const int UTIL_LINEIO_SHR			= UTIL_FIRST + 3; //ditto
const int UTIL_DATACONV_BADFORMAT	= UTIL_FIRST + 4; //ditto
const int UTIL_DATACONV_RANGE		= UTIL_FIRST + 5; //ditto
const int UTIL_DATACONV_DSF			= UTIL_FIRST + 6;
const int UTIL_LOCK_ERROR			= UTIL_FIRST + 7; //ditto
const int UTIL_PAGEDIO_ERROR		= UTIL_FIRST + 8; //ditto
const int UTIL_RESOURCE_NOT_HELD	= UTIL_FIRST + 9;
const int UTIL_RESOURCE_ANOTHERS	= UTIL_FIRST + 10;
const int UTIL_RESOURCE_EXISTENCE	= UTIL_FIRST + 11;
const int UTIL_MALLOC_FAILURE		= UTIL_FIRST + 12; //ditto
const int UTIL_BITMAP_ERROR			= UTIL_FIRST + 13; //ditto
const int UTIL_PARSE_ERROR			= UTIL_FIRST + 14;
const int UTIL_SOCKET_ERROR			= UTIL_FIRST + 15;
const int UTIL_BBTAM_ERROR			= UTIL_FIRST + 16;
const int UTIL_MISC_FILEIO_ERROR	= UTIL_FIRST + 17;
const int UTIL_STDIO_ERROR			= UTIL_FIRST + 18;
const int UTIL_PROGRESS_ERROR		= UTIL_FIRST + 19;
const int UTIL_STACK_ERROR			= UTIL_FIRST + 20;
const int UTIL_THREAD_ERROR			= UTIL_FIRST + 21;
const int UTIL_RNG_ERROR			= UTIL_FIRST + 22;
const int UTIL_MISC_ALGORITHM_ERROR	= UTIL_FIRST + 23;
const int UTIL_ARRAY_ERROR			= UTIL_FIRST + 20;

const int API_SINGLETON				= UTIL_FIRST + 201;
const int API_NOT_INITIALIZED		= UTIL_FIRST + 202;
const int API_MISC					= UTIL_FIRST + 203;

}	//close namespace

#endif
