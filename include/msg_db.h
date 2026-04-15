
#if !defined(BB_MSG_DB)
#define BB_MSG_DB

#include "msgcodes.h"

namespace dpt {

const int USER_STARTED_LEVEL2		= DB_FIRST + 1;
const int USER_FINISHED_LEVEL2		= DB_FIRST + 2;

//Context opening and closing
const int DB_OPENED					= DB_FIRST + 11;
const int DB_NONEXISTENT			= DB_FIRST + 12;
const int DB_NOT_RE_OPENED			= DB_FIRST + 13;
const int DB_OPEN_FAILED			= DB_FIRST + 14;
const int DB_CLOSED					= DB_FIRST + 15;
const int DB_IS_NOT_OPEN			= DB_FIRST + 16;
const int DB_CLOSE_FAILED			= DB_FIRST + 17;
const int DB_CLOSED_DEFAULT			= DB_FIRST + 18;
const int DB_IN_USE					= DB_FIRST + 19;
const int DB_BAD_DD					= DB_FIRST + 20;
const int DB_BAD_DSN				= DB_FIRST + 21; //distinct from SYSFILE_BAD_OSNAME
const int DB_BAD_CREATE_PARM		= DB_FIRST + 22;
const int DB_FILE_CREATED			= DB_FIRST + 23;
const int DB_FILE_CREATE_FAILED		= DB_FIRST + 24;
const int DB_FILE_INITIALIZED		= DB_FIRST + 25;
const int DB_BROADCAST_MSG			= DB_FIRST + 26;
const int DB_BAD_BROADCAST_MSG		= DB_FIRST + 27;
const int DB_BAD_FILE_CONTENTS		= DB_FIRST + 28;
const int DB_OSFILE_RENAMED			= DB_FIRST + 29;
const int DB_BROADCAST_MESSAGE		= DB_FIRST + 30;
const int DB_FISTAT_OPEN_MSG		= DB_FIRST + 31;
const int DB_BAD_PARM_MISC			= DB_FIRST + 32;
const int DB_BLOBCTL_INITIALIZED	= DB_FIRST + 33; //V3.0

//File status problems for various operations
const int DB_FILE_STATUS_FULL		= DB_FIRST + 35;
const int DB_FILE_STATUS_NOTINIT	= DB_FIRST + 36;
const int DB_FILE_STATUS_BROKEN		= DB_FIRST + 37;
const int DB_FILE_STATUS_REORGING	= DB_FIRST + 38;
const int DB_FILE_STATUS_DEFERRED	= DB_FIRST + 39;

//Buffers and checkpointing 
const int BUFF_FIRST				= DB_FIRST + 50;
const int BUFF_CONTROL_BUG			= BUFF_FIRST + 0;
const int BUFF_MEMORY				= BUFF_FIRST + 1;
const int BUFF_NEEDMORE				= BUFF_FIRST + 2;
const int BUFF_RIDICULOUS_DELAY		= BUFF_FIRST + 3;
const int BUFF_TIDY_INFO			= BUFF_FIRST + 4;

const int CHKP_FIRST				= BUFF_FIRST + 10;
const int CHKP_FILE_OPEN_FAILED		= CHKP_FIRST + 0;
const int CHKP_NOT_ENABLED			= CHKP_FIRST + 1;
const int CHKP_STARTING				= CHKP_FIRST + 2;
const int CHKP_COMPLETE				= CHKP_FIRST + 3;
const int CHKP_TIMED_OUT			= CHKP_FIRST + 4;
const int CHKP_ABORTED				= CHKP_FIRST + 5;
const int CHKP_MISC_BUG				= CHKP_FIRST + 6;
const int CHKP_STARTING_NOTERM		= CHKP_FIRST + 7; //for implied checkpoints
const int CHKP_COMPLETE_NOTERM		= CHKP_FIRST + 8; //ditto
const int CHKP_ABORTED_NOTERM		= CHKP_FIRST + 9; //ditto
const int ROLLBACK_NOTREQ			= CHKP_FIRST + 10;
const int ROLLBACK_START			= CHKP_FIRST + 11;
const int ROLLBACK_CANCELLED		= CHKP_FIRST + 12;
const int ROLLBACK_ABORTED			= CHKP_FIRST + 13;
const int ROLLBACK_BYPASSED			= CHKP_FIRST + 14;
const int ROLLBACK_BADFILES			= CHKP_FIRST + 15;
const int ROLLBACK_COMPLETE			= CHKP_FIRST + 16;
const int ROLLBACK_BACKUPS			= CHKP_FIRST + 17;
const int ROLLBACK_FILEINFO			= CHKP_FIRST + 18;

//Transaction processing and TBO
const int TXN_FIRST					= CHKP_FIRST + 20;
const int TXN_UPDATE_COMPLETE		= TXN_FIRST + 0;
const int TXN_BACKOUT_COMPLETE		= TXN_FIRST + 1;
const int TXN_BACKOUT_NO_UPDATES	= TXN_FIRST + 2;
const int TXN_BACKOUT_ERROR			= TXN_FIRST + 3;
const int TXN_ABORTED				= TXN_FIRST + 4;
const int TXN_BENIGN_ATOM			= TXN_FIRST + 5;
const int TXN_EOT_FLUSH_FAILED		= TXN_FIRST + 6;
const int TXN_BACKOUT_INFO			= TXN_FIRST + 7;
const int TXN_BACKOUT_BADSTATE		= TXN_FIRST + 8;

//Information-only exception codes, thrown to API applications
const int TXNERR_AUTO_BACKOUT		= TXN_FIRST + 10; //session response: cancel request
const int TXNERR_LOGICALLY_BROKEN	= TXN_FIRST + 11; //session response: soft restart
const int TXNERR_PHYSICALLY_BROKEN	= TXN_FIRST + 12; //session response: hard restart
const int SOFT_RESTART				= TXN_FIRST + 13;
const int HARD_RESTART				= TXN_FIRST + 14;

//DBA stuff
const int DBA_FIRST					= TXN_FIRST + 20;
const int DBA_NO_SUCH_FIELD			= DBA_FIRST + 0;
const int DBA_FIELD_ALREADY_EXISTS	= DBA_FIRST + 1;
const int DBA_GRP_FIELD_MISMATCH	= DBA_FIRST + 2;
const int DBA_GRP_NO_UPDTFILE		= DBA_FIRST + 3;
const int DBA_FLDATT_INCOMPATIBLE	= DBA_FIRST + 4;
const int DBA_FLDATT_INVALID		= DBA_FIRST + 5;
const int DBA_FLDNAME_INVALID		= DBA_FIRST + 6;
const int DBA_TOO_MANY_FIELDS		= DBA_FIRST + 7;
const int DBA_DEFERRED_UPDATE_ERROR	= DBA_FIRST + 8;
const int DBA_DEFERRED_UPDATE_INFO	= DBA_FIRST + 9;
const int DBA_UNLOAD_ERROR			= DBA_FIRST + 11;
const int DBA_UNLOAD_INFO			= DBA_FIRST + 12;
const int DBA_UNLOAD_INFO_TERM		= DBA_FIRST + 13;
const int DBA_LOAD_ERROR			= DBA_FIRST + 14;
const int DBA_LOAD_INFO				= DBA_FIRST + 15;
const int DBA_LOAD_INFO_TERM		= DBA_FIRST + 16;
const int DBA_FIELDNAME_ERROR		= DBA_FIRST + 17;
const int DBA_FIELDATTS_ERROR		= DBA_FIRST + 18;
const int DBA_REORG_INFO			= DBA_FIRST + 18;

//Data manipulation
const int DML_FIRST					= DBA_FIRST + 20;
const int DML_NONEXISTENT_RECORD	= DML_FIRST + 0;
const int DML_SICK_RECORD			= DML_FIRST + 1;
const int DML_RECORD_LOCK_FAILED	= DML_FIRST + 2;
const int DML_BAD_FIND_SPEC			= DML_FIRST + 3;
const int DML_INVALID_RETRY			= DML_FIRST + 4;
const int DML_RUNTIME_INFO_BUG		= DML_FIRST + 5;
const int DML_INVALID_INVIS_FUNC	= DML_FIRST + 6;
const int DML_NON_FLOAT_ERROR		= DML_FIRST + 7;
const int DML_NON_FLOAT_INFO		= DML_FIRST + 8;
const int DML_NON_STRING_ERROR		= DML_FIRST + 9;
const int DML_STRING_TOO_LONG		= DML_FIRST + 10;
const int DML_BAD_SORT_SPEC			= DML_FIRST + 11;
const int DML_INDEX_REQUIRED		= DML_FIRST + 12;
const int DML_BAD_INDEX_TYPE		= DML_FIRST + 13;
const int DML_TABLEB_BADSPEC		= DML_FIRST + 14;
const int DML_TABLEB_MBSCAN			= DML_FIRST + 15;
const int DML_RUNTIME_BADCONTEXT	= DML_FIRST + 16;
const int DML_INVALID_DATA_VALUE	= DML_FIRST + 17;

//General
const int DBGEN_FIRST				= DML_FIRST + 20;
const int MRO_NONEXISTENT_CHILD		= DBGEN_FIRST + 0;
const int MRO_INVALID_SET_POS		= DBGEN_FIRST + 1;
const int DB_STRUCTURE_BUG			= DBGEN_FIRST + 2;
const int DB_ALGORITHM_BUG			= DBGEN_FIRST + 3;
const int DB_MRO_MGMT_BUG			= DBGEN_FIRST + 4;
const int DB_INSUFFICIENT_SPACE		= DBGEN_FIRST + 5;
const int DB_API_BAD_PARM			= DBGEN_FIRST + 6;
const int DB_API_STUB_FUNC_ONLY		= DBGEN_FIRST + 7;
const int DB_UNEXPECTED_PAGE_TYPE	= DBGEN_FIRST + 8;
const int DB_BAD_PAGE_NUMBER		= DBGEN_FIRST + 9;

}	//close namespace

#endif
