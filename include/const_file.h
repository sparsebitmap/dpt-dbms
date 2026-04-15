
#if !defined BB_CONST_FILE
#define BB_CONST_FILE

namespace dpt {

enum FileType {
	FILETYPE_DB			= 1, 
	FILETYPE_SEQ		= 2, 
	FILETYPE_PROC		= 4, 
	FILETYPE_SYS		= 8,
	FILETYPE_PROCDIR	= 16,
	FILETYPE_ALL		= 31           //used in the LISTALC command
};

}

#endif
