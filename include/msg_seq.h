
#if !defined(BB_MSG_SEQ)
#define BB_MSG_SEQ

#include "msgcodes.h"

namespace dpt {

//Sequential file handling
const int SEQ_FILE_CREATED			= SEQ_FIRST + 1;
const int SEQ_IS_DIR				= SEQ_FIRST + 2;
const int SEQ_NONEXISTENT			= SEQ_FIRST + 3;
const int SEQ_ALREADY_EXISTS		= SEQ_FIRST + 4;
const int SEQ_BAD_DD				= SEQ_FIRST + 5;
const int SEQ_BAD_DSN				= SEQ_FIRST + 6;
const int SEQ_NOT_OPEN				= SEQ_FIRST + 7;
const int SEQ_ALREADY_OPEN			= SEQ_FIRST + 8;
const int SEQ_BAD_OPEN_MODE			= SEQ_FIRST + 9;
const int SEQ_PAST_EOF				= SEQ_FIRST + 10;
const int SEQ_FILE_TOO_BIG			= SEQ_FIRST + 11;
const int SEQ_TEMPDIR_ERROR			= SEQ_FIRST + 12;
const int SEQ_FILE_DELETED			= SEQ_FIRST + 13;

}	//close namespace

#endif
