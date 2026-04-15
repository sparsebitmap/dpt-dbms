
#if !defined(BB_MSG_FILE)
#define BB_MSG_FILE

#include "msgcodes.h"

namespace dpt {

//Allocation/freeing/opening/closing
const int SYSFILE_FIRST				= FILE_FIRST + 1; 
const int SYSFILE_ALLOC_ALREADY		= FILE_FIRST + 1;
const int SYSFILE_ALLOC_DD_IN_USE	= FILE_FIRST + 2;
const int SYSFILE_ALLOC_DSN_IN_USE	= FILE_FIRST + 3;
const int SYSFILE_NOT_ALLOCATED		= FILE_FIRST + 4;
const int SYSFILE_ALLOCATED			= FILE_FIRST + 5;	
const int SYSFILE_FREED				= FILE_FIRST + 6;	
const int SYSFILE_FREECMD_NOTALLOC	= FILE_FIRST + 7; //M204 doesn't report this
const int SYSFILE_FREED_FINAL		= FILE_FIRST + 8;	
const int SYSFILE_OPEN_ALREADY		= FILE_FIRST + 9;
const int SYSFILE_OPEN_FAILED		= FILE_FIRST + 10;
const int SYSFILE_CLOSE_FAILED		= FILE_FIRST + 11;
const int SYSFILE_BAD_DSN			= FILE_FIRST + 12;
const int SYSFILE_BAD_DD			= FILE_FIRST + 13;
const int SYSFILE_BAD_TYPE			= FILE_FIRST + 14;
const int SYSFILE_BAD_ALIAS			= FILE_FIRST + 15;
const int SYSFILE_LAST				= FILE_FIRST + 99; 

//Groups
const int GROUP_FIRST				= FILE_FIRST + 101;
const int GROUP_DUPE_MEMBER			= FILE_FIRST + 101;
const int GROUP_NONEXISTENT			= FILE_FIRST + 102;
const int GROUP_CREATED				= FILE_FIRST + 103;
const int GROUP_DELETED				= FILE_FIRST + 104;
const int GROUP_DELETED_FINAL		= FILE_FIRST + 105;
const int GROUP_ALREADY_EXISTS		= FILE_FIRST + 106;
const int GROUP_INVALID_NAME		= FILE_FIRST + 107;
const int GROUP_IN_USE				= FILE_FIRST + 108;
const int GROUP_INVALID_UPDTFILE	= FILE_FIRST + 109;
const int GROUP_LAST				= FILE_FIRST + 199;

//File/group contexts in general
const int CONTEXT_FIRST				= FILE_FIRST + 201;
const int CONTEXT_BAD				= FILE_FIRST + 201;
const int CONTEXT_IGNORED			= FILE_FIRST + 202;
const int CONTEXT_DISALLOWED		= FILE_FIRST + 203;
const int CONTEXT_GROUP_ONLY		= FILE_FIRST + 205;
const int CONTEXT_SINGLE_FILE_ONLY	= FILE_FIRST + 206;
const int CONTEXT_PERM_ONLY			= FILE_FIRST + 207;
const int CONTEXT_NO_DEFAULT		= FILE_FIRST + 208;
const int CONTEXT_IS_NOT_OPEN		= FILE_FIRST + 209;
const int CONTEXT_NO_CURRENT		= FILE_FIRST + 210;
const int CONTEXT_NO_UPDTFILE		= FILE_FIRST + 211;
const int CONTEXT_NONSPECIAL_ONLY	= FILE_FIRST + 212;
const int CONTEXT_NON$CURFILE_ONLY	= FILE_FIRST + 213;
const int CONTEXT_NONADHOC_ONLY		= FILE_FIRST + 214;
const int CONTEXT_MISMATCH			= FILE_FIRST + 215;
const int CONTEXT_LAST				= FILE_FIRST + 299;

}	//close namespace

#endif
