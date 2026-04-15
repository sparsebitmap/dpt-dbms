//*****************************************************************************************
//NOTE.
//This file is the basic implementation file for the message reference table.  It populates
//the table with messages used by database services and lower, and therefore makes up part
//of the API library (i.e. messages will be issued in "full host system" mode and by API
//programs.  See implementation file msgref2 for messages that only apply to the full host
//system.
//The reason for doing this is really just tidiness, in that there seems no point in 
//having lots of message codes defined that can never be issued.  It also saves memory
//and reduces the start-up time in API mode.
//*****************************************************************************************
#include "stdafx.h"

#include "msgref.h"

//Utils
#include "parsing.h"
#include "dataconv.h"
//Diagnostics
#include "except.h"

//Groups of message codes relating to DatabaseServices and below
#ifdef _BBDBAPI
#include "msg_db.h"
#include "msg_file.h"
#include "msg_seq.h"
#ifdef DPT_BUILDING_CAPI_DLL
#include "msg_japi.h"
#endif
#endif
#include "msg_core.h"
#include "msg_util.h"

//Slightly leaner audit trail in a full host build
#ifdef _BBHOST
#define _BBTIERAUDIT NOAUDIT
#else
#define _BBTIERAUDIT AUDIT
#endif

namespace dpt {

//define static members
bool MsgCtlRefTable::created = false;
bool MsgCtlRefTable::AUDIT = true;
bool MsgCtlRefTable::NOAUDIT = false;
bool MsgCtlRefTable::TERM = true;
bool MsgCtlRefTable::NOTERM = false;
bool MsgCtlRefTable::ERR = true;
bool MsgCtlRefTable::NOERR = false;

//******************************************************************************************
void MsgCtlRefTable::StoreEntry(int code, bool t, bool a, bool i, int r, bool tr)
{
	data[code] = MsgCtlOptions(t,  a,  i,  r, tr); 
}

//******************************************************************************************
//Populates all the default message control options
MsgCtlRefTable::MsgCtlRefTable()
{

if (created) 
	throw Exception(API_SINGLETON, "There can be only one parm RefTable object");
created = true;

//******************************************************************************************
//Core services, system start up etc.
//******************************************************************************************
#ifdef BB_MSG_CORE

//Parameters
	StoreEntry(PARM_BADNAME,			TERM,  AUDIT,  ERR,  4); 
	StoreEntry(PARM_NOTRESETTABLE,		TERM,  AUDIT,  ERR,  4); 
	StoreEntry(PARM_BADVALUE,			TERM,  AUDIT,  ERR,  4); 
	StoreEntry(PARM_USEDMAXORMIN,		TERM,  AUDIT,  NOERR, 0); 
	StoreEntry(PARM_ATTRCONFLICT,		TERM,  AUDIT,  NOERR, 0); //bad attributes turned off
	StoreEntry(PARM_PARMCONFLICT,		TERM,  AUDIT,  ERR, 4); 
	StoreEntry(PARM_BADTIME,			TERM,  AUDIT,  ERR,  4); 
	StoreEntry(PARM_NEEDS_FILECONTEXT,	TERM,  AUDIT,  ERR,  4); 
	StoreEntry(PARM_NOT_EMULATED,		TERM,  AUDIT,  NOERR,  4); 
	StoreEntry(PARM_MISC,				TERM,  AUDIT,  ERR,  4); 

//Stats
	StoreEntry(STAT_BADNAME,			TERM,  AUDIT,  ERR,  4); 
	StoreEntry(STAT_NOT_FILE,			TERM,  AUDIT,  ERR,  4); 
	StoreEntry(STAT_NEED_FILECONTEXT,	TERM,  AUDIT,  ERR,  4); 
	StoreEntry(STAT_MISC,				TERM,  AUDIT,  ERR,  4); 

//System start up, close down etc.
	StoreEntry(SYS_BAD_INIFILE,			TERM,  AUDIT,  ERR,  96); 
	StoreEntry(SYS_STARTUP_ERROR,		TERM,  AUDIT,  ERR,  96); 
	StoreEntry(SYS_SETFORQUIESCE,		TERM,  AUDIT,  NOERR, 0); //?
	StoreEntry(SYS_QUIESCECOMPLETE,		TERM,  AUDIT,  NOERR, 0); //0354

//Access control
	StoreEntry(ACCOUNT_ALREADY_EXISTS,		TERM,  AUDIT,  ERR,  16); 
	StoreEntry(ACCOUNT_DOES_NOT_EXIST,		TERM,  AUDIT,  ERR,  16); 
	StoreEntry(ACCESS_ACCOUNT_BADFORMAT,	TERM,  AUDIT,  ERR,  16); 
	StoreEntry(ACCESS_PASSWORD_BADFORMAT,	TERM,  AUDIT,  ERR,  16); 
	StoreEntry(ACCESS_CONTROL_UPDATED,	    TERM,  AUDIT,  NOERR,  0); 
	StoreEntry(ACCESS_CONTROL_BADDATA,	    TERM,  AUDIT,  ERR,  96); 
	StoreEntry(LOGIN_FAILED,        	    TERM,  AUDIT,  ERR,  16); 
	StoreEntry(LOGIN_SUCCESS,        	    TERM,  AUDIT,  NOERR,  0); 
	StoreEntry(ACCESS_INSUFFICIENT_PRIVS,   TERM,  AUDIT,  ERR,  16); 

//User start up, close down etc.
	//Core Services
	StoreEntry(USER_STARTED_LEVEL1,		NOTERM, _BBTIERAUDIT,  NOERR, 0); 
	StoreEntry(USER_FINISHED_LEVEL1,	NOTERM, _BBTIERAUDIT,  NOERR, 0); 
	StoreEntry(USER_SETFORBUMP,			TERM,  AUDIT,  NOERR, 0); //1124
	StoreEntry(USER_HEEDINGBUMP,		TERM,  AUDIT,  NOERR, 0); //1392, 1401?
	StoreEntry(USER_HEEDINGBUMP_FINAL,	TERM,  AUDIT,  ERR, 4); //1392, 1401?
	StoreEntry(USER_TOO_MANY_NUSERS,	TERM,  AUDIT,  ERR, 16); 
	StoreEntry(USER_BADNUMBER,			TERM,  AUDIT,  NOERR, 0); //0718
	StoreEntry(USER_BADID,				TERM,  AUDIT,  NOERR, 0); //0716
	StoreEntry(USER_QUIESCE_STARTED,	TERM,  AUDIT,  NOERR, 0); //
	StoreEntry(USER_QUIESCE_ENDED,		TERM,  AUDIT,  NOERR, 0); //
	StoreEntry(USER_HEEDINGQUIESCE,		TERM,  AUDIT,  NOERR, 0); //0348
	StoreEntry(USER_PLEASEHEEDQUIESCE,	TERM,  AUDIT,  NOERR, 0); //1028
	StoreEntry(USER_DUPETHREAD,			TERM,  AUDIT,  ERR, 16);
	StoreEntry(USER_TOO_MANY_ERRORS,	TERM,  AUDIT,  NOERR, 0);
	StoreEntry(USER_IO_ERROR,			NOTERM,  AUDIT,  ERR, 99);
	StoreEntry(USER_BEGINTHREAD_ERROR,	NOTERM,  AUDIT,  ERR, 99);
	StoreEntry(USER_PRIORITY_CHANGE,	TERM,  AUDIT,  NOERR, 0);

	StoreEntry(CUSTOM_FEATURE_ERROR,	TERM,  AUDIT,  ERR, 16);
	StoreEntry(CUSTOM_FEATURE_INFO,		TERM,  AUDIT,  NOERR, 0);
	StoreEntry(USER_SET_MSG_HWM,		TERM,  AUDIT,  NOERR, 0);

//Miscellaneous
	StoreEntry(MISC_CAUGHT_STL,			TERM,  AUDIT,  ERR,  99); 
	StoreEntry(MISC_CAUGHT_UNKNOWN,		TERM,  AUDIT,  ERR,  99); 
	StoreEntry(MISC_OS_ROUTINE_ERROR,	TERM,  AUDIT,  ERR,  99); 
//Theoretically impossible system bugs
	StoreEntry(BUG_MISC,				TERM,  AUDIT,  ERR,  99); 
	StoreEntry(BUG_STRING_TOO_LONG,		TERM,  AUDIT,  ERR,  99); 
	StoreEntry(BUG_FAT_INTERFACE_BAD_CALL, TERM,  AUDIT,  ERR,  99); 

//Debug messages can be turned on in a release build with MSGCTL
#ifdef _DEBUG
	StoreEntry(MISC_DEBUG_INFO,			TERM,  AUDIT,  NOERR, 0); 
	StoreEntry(MISC_DEBUG_ERROR,		TERM,  AUDIT,  ERR, MISC_DEBUG_ERROR); 
#else
	StoreEntry(MISC_DEBUG_INFO,			NOTERM,  NOAUDIT,  NOERR, 0); 
	StoreEntry(MISC_DEBUG_ERROR,		NOTERM,  NOAUDIT,  ERR, MISC_DEBUG_ERROR); 
#endif

	//Use in custom diagnostic builds for users
	StoreEntry(MISC_DEBUG_USRAUDIT_INFO,   NOTERM,  AUDIT,  NOERR, 0);
	StoreEntry(MISC_DEBUG_USRAUDIT_ERROR,  NOTERM,  AUDIT,  NOERR, MISC_DEBUG_USRAUDIT_ERROR); 
	StoreEntry(FUNC_UNDER_CONSTRUCTION,  TERM,  AUDIT,  ERR, 4); 

#endif

//DatabaseServices level
#ifdef BB_MSG_DB
	StoreEntry(USER_STARTED_LEVEL2,		NOTERM, _BBTIERAUDIT,  NOERR, 0); 
	StoreEntry(USER_FINISHED_LEVEL2,	NOTERM, _BBTIERAUDIT,  NOERR, 0); 
#endif

//******************************************************************************************
//File processing
//******************************************************************************************
#ifdef BB_MSG_FILE

//General file stuff shared across file types
	StoreEntry(SYSFILE_ALLOC_ALREADY,		TERM,  AUDIT,  ERR,  4); 
	StoreEntry(SYSFILE_ALLOC_DD_IN_USE,		TERM,  AUDIT,  ERR,  4); 
	StoreEntry(SYSFILE_ALLOC_DSN_IN_USE,	TERM,  AUDIT,  ERR,  4); 
	StoreEntry(SYSFILE_NOT_ALLOCATED,		TERM,  AUDIT,  ERR,  4); 
	StoreEntry(SYSFILE_ALLOCATED,			TERM,  AUDIT,  NOERR,  0); 
	StoreEntry(SYSFILE_FREED,				TERM,  AUDIT,  NOERR,  0); 
	//On m204 this one is noterm as well as NOERR.  Might get in line eventually.
	StoreEntry(SYSFILE_FREECMD_NOTALLOC,	TERM,  AUDIT,  NOERR,  0); 
	StoreEntry(SYSFILE_FREED_FINAL,			NOTERM,  AUDIT,  NOERR,  0); 
	StoreEntry(SYSFILE_OPEN_ALREADY,		TERM,  AUDIT,  ERR,  4); 
	StoreEntry(SYSFILE_OPEN_FAILED,			TERM,  AUDIT,  ERR,  4); 
	StoreEntry(SYSFILE_CLOSE_FAILED,		TERM,  AUDIT,  ERR,  4); 
	StoreEntry(SYSFILE_BAD_DSN,				TERM,  AUDIT,  ERR,  4); 
	StoreEntry(SYSFILE_BAD_DD,				TERM,  AUDIT,  ERR,  4); 
	StoreEntry(SYSFILE_BAD_TYPE,			TERM,  AUDIT,  ERR,  4); 
	StoreEntry(SYSFILE_BAD_ALIAS,			TERM,  AUDIT,  ERR,  4); 

//Groups
	StoreEntry(GROUP_DUPE_MEMBER,			TERM,  AUDIT,  ERR,  4); 
	StoreEntry(GROUP_NONEXISTENT,			TERM,  AUDIT,  ERR,  4); 
	StoreEntry(GROUP_CREATED,				TERM,  AUDIT,  NOERR,  0); 
	StoreEntry(GROUP_DELETED,				TERM,  AUDIT,  NOERR,  0); 
	StoreEntry(GROUP_DELETED_FINAL,			NOTERM,  AUDIT,  NOERR,  0); 
	StoreEntry(GROUP_ALREADY_EXISTS,		TERM,  AUDIT,  ERR,  4); 
	StoreEntry(GROUP_INVALID_NAME,			TERM,  AUDIT,  ERR,  4); 
	StoreEntry(GROUP_IN_USE,				TERM,  AUDIT,  ERR,  4); 
	StoreEntry(GROUP_INVALID_UPDTFILE,		TERM,  AUDIT,  ERR,  4); 

//General file contexts
	StoreEntry(CONTEXT_BAD,					TERM,  AUDIT,  ERR,  4); 
	StoreEntry(CONTEXT_IGNORED,				TERM,  AUDIT,  NOERR,  0); 
	StoreEntry(CONTEXT_DISALLOWED,			TERM,  AUDIT,  ERR,  4); 
	StoreEntry(CONTEXT_GROUP_ONLY,			TERM,  AUDIT,  ERR,  4); 
	StoreEntry(CONTEXT_SINGLE_FILE_ONLY,	TERM,  AUDIT,  ERR,  4); 
	StoreEntry(CONTEXT_PERM_ONLY,			TERM,  AUDIT,  ERR,  4); 
	StoreEntry(CONTEXT_NO_DEFAULT,			TERM,  AUDIT,  ERR,  4); 
	StoreEntry(CONTEXT_IS_NOT_OPEN,			TERM,  AUDIT,  ERR,  4); 
	StoreEntry(CONTEXT_NO_CURRENT,			TERM,  AUDIT,  ERR,  4); 
	StoreEntry(CONTEXT_NO_UPDTFILE,			TERM,  AUDIT,  ERR,  4); 
	StoreEntry(CONTEXT_NONSPECIAL_ONLY,		TERM,  AUDIT,  ERR,  4); 
	StoreEntry(CONTEXT_NON$CURFILE_ONLY,	TERM,  AUDIT,  ERR,  4); 
	StoreEntry(CONTEXT_NONADHOC_ONLY,		TERM,  AUDIT,  ERR,  4); 
	StoreEntry(CONTEXT_MISMATCH,			TERM,  AUDIT,  ERR,  4); 
#endif

//Sequential files
#ifdef BB_MSG_SEQ
	StoreEntry(SEQ_FILE_CREATED,			TERM,  AUDIT,  NOERR,  0); 
	StoreEntry(SEQ_IS_DIR,					TERM,  AUDIT,  ERR,  4); 
	StoreEntry(SEQ_NONEXISTENT,				TERM,  AUDIT,  ERR,  4); 
	StoreEntry(SEQ_ALREADY_EXISTS,			TERM,  AUDIT,  ERR,  4); 
	StoreEntry(SEQ_BAD_DD,					TERM,  AUDIT,  ERR,  4); 
	StoreEntry(SEQ_BAD_DSN,					TERM,  AUDIT,  ERR,  4); 
	StoreEntry(SEQ_NOT_OPEN,				TERM,  AUDIT,  ERR,  4); 
	StoreEntry(SEQ_ALREADY_OPEN,			TERM,  AUDIT,  ERR,  4); 
	StoreEntry(SEQ_BAD_OPEN_MODE,			TERM,  AUDIT,  ERR,  4); 
	StoreEntry(SEQ_PAST_EOF,				TERM,  AUDIT,  ERR,  4); 
	StoreEntry(SEQ_FILE_TOO_BIG,			TERM,  AUDIT,  ERR,  4); 
	StoreEntry(SEQ_TEMPDIR_ERROR,			TERM,  AUDIT,  ERR,  16); 
	StoreEntry(SEQ_FILE_DELETED,			TERM,  AUDIT,  NOERR,  0); 
#endif

//******************************************************************************************
//Database
//******************************************************************************************
#ifdef BB_MSG_DB

	StoreEntry(DB_OPENED,					TERM,  AUDIT,  NOERR,  0); 
	StoreEntry(DB_NONEXISTENT,				TERM,  AUDIT,  ERR,  4); 
	StoreEntry(DB_NOT_RE_OPENED,			TERM,  AUDIT,  NOERR,  0); 
	StoreEntry(DB_OPEN_FAILED,				TERM,  AUDIT,  ERR,  4); 
	StoreEntry(DB_CLOSED,					TERM,  AUDIT,  NOERR,  0); 
	StoreEntry(DB_IS_NOT_OPEN,				TERM,  AUDIT,  ERR,  4); 
	StoreEntry(DB_CLOSE_FAILED,				TERM,  AUDIT,  ERR,  4); 
	StoreEntry(DB_CLOSED_DEFAULT,			TERM,  AUDIT,  NOERR,  0); 
	StoreEntry(DB_IN_USE,					TERM,  AUDIT,  ERR,  4); 
	StoreEntry(DB_BAD_DD,					TERM,  AUDIT,  ERR,  4); 
	StoreEntry(DB_BAD_DSN,					TERM,  AUDIT,  ERR,  4); 
	StoreEntry(DB_BAD_CREATE_PARM,			TERM,  AUDIT,  ERR,  4); 
	StoreEntry(DB_FILE_CREATED,				TERM,  AUDIT,  NOERR,  0); 
	StoreEntry(DB_FILE_CREATE_FAILED,		TERM,  AUDIT,  ERR,  4); 
	StoreEntry(DB_FILE_INITIALIZED,			TERM,  AUDIT,  NOERR,  0); 
	StoreEntry(DB_BROADCAST_MSG,			TERM,  AUDIT,  NOERR,  0); 
	StoreEntry(DB_BAD_BROADCAST_MSG,		TERM,  AUDIT,  ERR,  4); 
	StoreEntry(DB_BAD_FILE_CONTENTS,		TERM,  AUDIT,  ERR,  16); 
	StoreEntry(DB_OSFILE_RENAMED,			TERM,  AUDIT,  NOERR,  0); 
	StoreEntry(DB_BROADCAST_MESSAGE,		TERM,  AUDIT,  NOERR,  0); 
	StoreEntry(DB_FISTAT_OPEN_MSG,			TERM,  AUDIT,  NOERR, 0); 
	StoreEntry(DB_BAD_PARM_MISC,			TERM,  AUDIT,  ERR,  4); 
	StoreEntry(DB_BLOBCTL_INITIALIZED,		TERM,  AUDIT,  NOERR,  0);  //V3.0

	StoreEntry(DB_FILE_STATUS_NOTINIT,		TERM,  AUDIT,  ERR, 16); 
	StoreEntry(DB_FILE_STATUS_FULL,			TERM,  AUDIT,  ERR, 16); 
	StoreEntry(DB_FILE_STATUS_BROKEN,		TERM,  AUDIT,  ERR, 16); 
	StoreEntry(DB_FILE_STATUS_REORGING,		TERM,  AUDIT,  ERR, 16); 
	StoreEntry(DB_FILE_STATUS_DEFERRED,		TERM,  AUDIT,  ERR, 16); 

	StoreEntry(BUFF_CONTROL_BUG,			TERM,  AUDIT,  ERR,  96); 
	StoreEntry(BUFF_MEMORY,					TERM,  AUDIT,  ERR,  16); 
	StoreEntry(BUFF_NEEDMORE,				TERM,  AUDIT,  ERR,  32); 
	StoreEntry(BUFF_RIDICULOUS_DELAY,		TERM,  AUDIT,  ERR,  48); 
	StoreEntry(BUFF_TIDY_INFO,				TERM,  AUDIT,  NOERR,  0); 

	StoreEntry(CHKP_FILE_OPEN_FAILED,		TERM,  AUDIT,  ERR,  96); 
	StoreEntry(CHKP_NOT_ENABLED,			TERM,  AUDIT,  ERR,  96); 
	StoreEntry(CHKP_STARTING,				TERM,  AUDIT,  NOERR,  0); 
	StoreEntry(CHKP_COMPLETE,				TERM,  AUDIT,  NOERR,  0); 
	StoreEntry(CHKP_TIMED_OUT,				TERM,  AUDIT,  NOERR,  0); 
	StoreEntry(CHKP_ABORTED,				TERM,  AUDIT,  ERR,  4); 
	StoreEntry(CHKP_MISC_BUG,				TERM,  AUDIT,  ERR,  99); 
	StoreEntry(CHKP_STARTING_NOTERM,		NOTERM,  AUDIT,  NOERR,  0); 
	StoreEntry(CHKP_COMPLETE_NOTERM,		NOTERM,  AUDIT,  NOERR,  0); 
	StoreEntry(CHKP_ABORTED_NOTERM,			NOTERM,  AUDIT,  NOERR,  0); 
	StoreEntry(ROLLBACK_NOTREQ,				NOTERM,  AUDIT,  NOERR, 0); 
	StoreEntry(ROLLBACK_START,				TERM,  AUDIT,  NOERR, 0); 
	StoreEntry(ROLLBACK_CANCELLED,			TERM,  AUDIT,  NOERR,  0); 
	StoreEntry(ROLLBACK_ABORTED,			TERM,  AUDIT,  ERR,  16); 
	StoreEntry(ROLLBACK_BYPASSED,			TERM,  AUDIT,  NOERR,  0); 
	StoreEntry(ROLLBACK_BADFILES,			TERM,  AUDIT,  ERR,  4); 
	StoreEntry(ROLLBACK_COMPLETE,			TERM,  AUDIT,  NOERR,  0); 
	StoreEntry(ROLLBACK_BACKUPS,			NOTERM,  AUDIT,  NOERR,  0); 
	StoreEntry(ROLLBACK_FILEINFO,			NOTERM,  AUDIT,  NOERR,  0); 

	StoreEntry(TXN_UPDATE_COMPLETE,			NOTERM,  AUDIT,  NOERR,  0); 
	StoreEntry(TXN_BACKOUT_COMPLETE,		TERM,  AUDIT,  NOERR,  0); 
	StoreEntry(TXN_BACKOUT_NO_UPDATES,		TERM,  AUDIT,  NOERR,  0); 
	StoreEntry(TXN_BACKOUT_ERROR,			TERM,  AUDIT,  ERR,  96); 
	StoreEntry(TXN_ABORTED,					TERM,  AUDIT,  ERR,  16); 
	StoreEntry(TXN_EOT_FLUSH_FAILED,		NOTERM,  AUDIT,  ERR,  4); 
	StoreEntry(TXN_BACKOUT_INFO,			TERM,  AUDIT,  NOERR, 4); 
	StoreEntry(TXN_BACKOUT_BADSTATE,		TERM,  AUDIT,  ERR, 16); 

	StoreEntry(TXNERR_AUTO_BACKOUT,			TERM,  AUDIT,  NOERR,  0); 
	StoreEntry(TXNERR_LOGICALLY_BROKEN,		TERM,  AUDIT,  ERR,  16); 
	StoreEntry(TXNERR_PHYSICALLY_BROKEN,	TERM,  AUDIT,  ERR,  16); 
	StoreEntry(SOFT_RESTART,				TERM,  AUDIT,  ERR,  16); 
	StoreEntry(HARD_RESTART,				TERM,  AUDIT,  ERR,  16); 

	StoreEntry(DBA_NO_SUCH_FIELD,			TERM,  AUDIT,  ERR,  4); 
	StoreEntry(DBA_FIELD_ALREADY_EXISTS,	TERM,  AUDIT,  ERR,  4); 
	StoreEntry(DBA_GRP_FIELD_MISMATCH,		TERM,  AUDIT,  ERR,  4); 
	StoreEntry(DBA_GRP_NO_UPDTFILE,			TERM,  AUDIT,  ERR,  4); 
	StoreEntry(DBA_FLDATT_INCOMPATIBLE,		TERM,  AUDIT,  ERR,  4); 
	StoreEntry(DBA_FLDATT_INVALID,			TERM,  AUDIT,  ERR,  4); 
	StoreEntry(DBA_FLDNAME_INVALID,			TERM,  AUDIT,  ERR,  4); 
	StoreEntry(DBA_TOO_MANY_FIELDS,			TERM,  AUDIT,  ERR,  4); 
	StoreEntry(DBA_DEFERRED_UPDATE_ERROR,	TERM,  AUDIT,  ERR,  32); 
	StoreEntry(DBA_DEFERRED_UPDATE_INFO,	TERM,  AUDIT,  NOERR,  0); 
	StoreEntry(DBA_UNLOAD_ERROR,			TERM,  AUDIT,  ERR,  16); 
	StoreEntry(DBA_UNLOAD_INFO,				TERM,  AUDIT,  NOERR,  0); 
	StoreEntry(DBA_UNLOAD_INFO_TERM,		TERM,  NOAUDIT,  NOERR,  0); 
	StoreEntry(DBA_LOAD_ERROR,				TERM,  AUDIT,  ERR,  32); 
	StoreEntry(DBA_LOAD_INFO,				TERM,  AUDIT,  NOERR,  0); 
	StoreEntry(DBA_LOAD_INFO_TERM,			TERM,  NOAUDIT,  NOERR,  0); 
	StoreEntry(DBA_FIELDNAME_ERROR,			TERM,  AUDIT,  ERR,  4); 
	StoreEntry(DBA_FIELDATTS_ERROR,			TERM,  AUDIT,  ERR,  4); 
	StoreEntry(DBA_REORG_INFO,				TERM,  AUDIT,  NOERR,  0); 

	StoreEntry(DML_NONEXISTENT_RECORD,		TERM,  AUDIT,  ERR,  4); 
	StoreEntry(DML_SICK_RECORD,				TERM,  AUDIT,  ERR,  4); 
	StoreEntry(DML_RECORD_LOCK_FAILED,		TERM,  AUDIT,  ERR,  4); 
	StoreEntry(DML_BAD_FIND_SPEC,			TERM,  AUDIT,  ERR,  64); 
	StoreEntry(DML_INVALID_INVIS_FUNC,		TERM,  AUDIT,  ERR,  4); 
	StoreEntry(DML_NON_FLOAT_ERROR,			TERM,  AUDIT,  ERR,  4); 
	StoreEntry(DML_NON_FLOAT_INFO,			TERM,  AUDIT,  NOERR, 4); 
	StoreEntry(DML_NON_STRING_ERROR,		TERM,  AUDIT,  ERR,  4); 
	StoreEntry(DML_STRING_TOO_LONG,			TERM,  AUDIT,  ERR,  4); 
	StoreEntry(DML_BAD_SORT_SPEC,			TERM,  AUDIT,  ERR,  64); 
	StoreEntry(DML_INDEX_REQUIRED,			TERM,  AUDIT,  ERR,  4); 
	StoreEntry(DML_BAD_INDEX_TYPE,			TERM,  AUDIT,  ERR,  4); 
	StoreEntry(DML_TABLEB_BADSPEC,			TERM,  AUDIT,  ERR,  4); 
	StoreEntry(DML_TABLEB_MBSCAN,			TERM,  AUDIT,  ERR,  4); 
	StoreEntry(DML_RUNTIME_BADCONTEXT,		TERM,  AUDIT,  ERR,  4); 
	StoreEntry(DML_INVALID_DATA_VALUE,		TERM,  AUDIT,  ERR,  4); 

	StoreEntry(MRO_NONEXISTENT_CHILD,		TERM,  AUDIT,  ERR,  96); 
	StoreEntry(MRO_INVALID_SET_POS,			TERM,  AUDIT,  ERR,  96); 
	StoreEntry(DB_STRUCTURE_BUG,			TERM,  AUDIT,  ERR, 96); 
	StoreEntry(DB_ALGORITHM_BUG,			TERM,  AUDIT,  ERR, 96); 
	StoreEntry(DB_MRO_MGMT_BUG,				TERM,  AUDIT,  ERR, 96); 
	StoreEntry(DB_INSUFFICIENT_SPACE,		TERM,  AUDIT,  ERR, 16); 
	StoreEntry(DB_API_BAD_PARM,				TERM,  AUDIT,  ERR, 4); 
	StoreEntry(DB_API_STUB_FUNC_ONLY,		TERM,  AUDIT,  ERR, 96); 
	StoreEntry(DB_UNEXPECTED_PAGE_TYPE,		TERM,  AUDIT,  ERR, 96); 
	StoreEntry(DB_BAD_PAGE_NUMBER,			TERM,  AUDIT,  ERR, 96); 

#endif

//******************************************************************************************
//Utils etc.
//******************************************************************************************
#ifdef BB_MSG_UTIL
	StoreEntry(UTIL_BAD_PATTERN,			TERM,  AUDIT,  ERR,  4); 
	StoreEntry(UTIL_LINEIO_ERROR,			TERM,  AUDIT,  ERR,  99); 
	StoreEntry(UTIL_LINEIO_SHR,				TERM,  AUDIT,  ERR,  99); 
	StoreEntry(UTIL_DATACONV_BADFORMAT,		TERM,  AUDIT,  ERR,  99); 
	StoreEntry(UTIL_DATACONV_RANGE,			TERM,  AUDIT,  ERR,  99); 
	StoreEntry(UTIL_DATACONV_DSF,			TERM,  AUDIT,  ERR,  99); 
	StoreEntry(UTIL_LOCK_ERROR,				TERM,  AUDIT,  ERR,  99); 
	StoreEntry(UTIL_PAGEDIO_ERROR,			TERM,  AUDIT,  ERR,  99); 
	StoreEntry(UTIL_RESOURCE_NOT_HELD,		TERM,  AUDIT,  ERR,  99); 
	StoreEntry(UTIL_RESOURCE_ANOTHERS,		TERM,  AUDIT,  ERR,  99); 
	StoreEntry(UTIL_RESOURCE_EXISTENCE,		TERM,  AUDIT,  ERR,  99); 
	StoreEntry(UTIL_MALLOC_FAILURE,			TERM,  AUDIT,  ERR,  99); 
	StoreEntry(UTIL_BITMAP_ERROR,			TERM,  AUDIT,  ERR,  99); 
	StoreEntry(UTIL_SOCKET_ERROR,			TERM,  AUDIT,  ERR,  99); 
	StoreEntry(UTIL_BBTAM_ERROR,			TERM,  AUDIT,  ERR,  99); 
	StoreEntry(UTIL_MISC_FILEIO_ERROR,		TERM,  AUDIT,  ERR,  99); 
	StoreEntry(UTIL_STDIO_ERROR,			TERM,  AUDIT,  ERR,  99); 
	StoreEntry(UTIL_PROGRESS_ERROR,			TERM,  AUDIT,  ERR,  99); 
	StoreEntry(UTIL_STACK_ERROR,			TERM,  AUDIT,  ERR,  99); 
	StoreEntry(UTIL_THREAD_ERROR,			TERM,  AUDIT,  ERR,  99); 
	StoreEntry(UTIL_RNG_ERROR,				TERM,  AUDIT,  ERR,  99); 
	StoreEntry(UTIL_MISC_ALGORITHM_ERROR,	TERM,  AUDIT,  ERR,  99); 
	StoreEntry(UTIL_ARRAY_ERROR,			TERM,  AUDIT,  ERR,  99); 

	//This is a bit of a kluge to support the shared handling of MSGCTL commands between
	//the ini file reader and the actual user-issued MSGCTL command.  The command will
	//therefore issue a parsing error code different from all other parsing errors.
	StoreEntry(UTIL_PARSE_ERROR,			TERM,  AUDIT,  ERR,  4); 

	StoreEntry(API_SINGLETON,				TERM,  AUDIT,  ERR,  99); 
	StoreEntry(API_NOT_INITIALIZED,			TERM,  AUDIT,  ERR,  99); 
	StoreEntry(API_MISC,					TERM,  AUDIT,  ERR,  99); 

#endif

//******************************************************************************************
//V2.26 API exceptions that might get thrown down into the core, or where Issue() is needed
//for some other reason.
//******************************************************************************************
#ifdef BB_MSG_JAPI
	StoreEntry(JAPI_JNI_MISC_ERROR,			TERM,  AUDIT,  ERR,  64);
	StoreEntry(JAPI_LINEIO_ERROR,			TERM,  AUDIT,  ERR,  64);
#endif

//******************************************************************************************
//Session level message
//******************************************************************************************
#ifdef _BBHOST
	FullHostExtraConstructor();
#endif
}

//**************************************************************************************
//Inquiry function
//**************************************************************************************
MsgCtlOptions MsgCtlRefTable::GetMsgCtl(const int msgnum) const
{
	std::map<int, MsgCtlOptions>::const_iterator mci = data.find(msgnum);
	if (mci == data.end()) {
		throw Exception(BUG_MISC, 
			std::string("No such message: DPT.")
			.append(util::ZeroPad(msgnum, 4)));
	}
	
	return mci->second;
}

//****************************************************************************************
//Used both when reading the ini file and when parsing a regular MSGCTL command.
//The returned object is an amalgamation of the one passed in (containing the current 
//settings) and the supplied command options.  In the case of reading the ini file the
//defaults should come from the overall reference table, but with a MSGCTL command, the 
//values used come from the user's current settings for the message.
//****************************************************************************************
MsgCtlOptions ParseMsgCtlCommand(const std::string& line, const MsgCtlOptions& current)
{
	//Read parts of the command.
	std::vector<std::string> opt;
	util::Tokenize(opt, line, " =");

	MsgCtlOptions mc(current.term, current.audit, current.error, current.rcode, false);

	for (size_t x = 0; x < opt.size(); x++) {
		if (opt[x] == "TERM") 
			mc.term = true; 
		else if (opt[x] == "NOTERM")
			mc.term = false; 
		else if (opt[x] == "AUDIT")
			mc.audit = true; 
		else if (opt[x] == "NOAUDIT")
			mc.audit = false; 
		else if (opt[x] == "CLASS") {
			if (x == opt.size()-1)
				throw Exception(UTIL_PARSE_ERROR, "Missing error class");
			else {
				x++;
				if (opt[x] == "E") 
					mc.error = true; 
				else if (opt[x] == "I") 
					mc.error = false; 
				else 
					throw Exception(UTIL_PARSE_ERROR, "Invalid error class");
			}
		}
		else if (util::OneOf(opt[x], "RETCODE/RETCODEB/RETCODEO")) {
			if (opt[x] == "RETCODE")
				mc.custom_option = true;
	
			if (x == opt.size()-1)
				throw Exception(UTIL_PARSE_ERROR, "Missing return code");
			else {
				x++;
				mc.rcode = util::StringToInt(opt[x]);
			}
		}
		else {
			throw Exception(UTIL_PARSE_ERROR, std::string
				("Unknown MSGCTL option: '").append(opt[x]).append(1, '\''));
		}
	}

	return mc;
}


} // close namespace
