
#if !defined(BB_APICONST)
#define BB_APICONST

namespace dpt {

//-------------------------------------
enum FileDisp {
	FILEDISP_OLD, 
	FILEDISP_NEW, 
	FILEDISP_COND,
	FILEDISP_MOD,
	FILEDISP_READONLY,
	FILEDISP_TEMP			//V2.06 May 2007.
};

//V2.10 - Dec 2007.  Moved following 3 enums here from private section of class DatabaseFile.
enum Fistats {
	FISTAT_UNUSED_X80		= 0x80,
	FISTAT_LOG_BROKEN		= 0x40,
	FISTAT_DEFERRED_UPDATES	= 0x20,
	FISTAT_RECOVERED		= 0x10,
	FISTAT_FULL				= 0x08,
	FISTAT_UNUSED_X04		= 0x04,
	FISTAT_PHYS_BROKEN		= 0x02,
	FISTAT_NOT_INIT			= 0x01
};
enum Fiflags {
	FIFLAGS_PAGE_BROKEN		= 0x80, //replaces M204's 80 bit to do with sorted files
	FIFLAGS_FULL_TABLEB		= 0x40, 
	FIFLAGS_FULL_TABLED		= 0x20, 
	FIFLAGS_UNUSED_FLC		= 0x10, 
	FIFLAGS_UNUSED_CLOGIC	= 0x08, 
	FIFLAGS_UNUSED_OIFULL	= 0x04, //consolidated under x20
	FIFLAGS_REORGING		= 0x02, //M204 does not use this bit
	FIFLAGS_UNUSED_POST_316	= 0x01 
};

enum Fileorgs {
	FILEORG_UNUSED_SKEWED	= 0x40,
	FILEORG_UNUSED_UNORD	= 0x20,
	FILEORG_UNUSED_HASHKEY	= 0x08,
	FILEORG_RRN				= 0x04,
	FILEORG_UNUSED_KEY_REQ	= 0x02,
	FILEORG_UNUSED_SORTED	= 0x01
};
const int FILEORG_NORMAL	= 0;
const int FILEORG_UNORD_RRN	= 0x24;

//-------------------------------------
typedef unsigned short FindOperator;

const FindOperator FD_NULLOP			= 0; //internal use only
const FindOperator FD_EQ				= 1;
const FindOperator FD_RANGE1_LOCONST		= 2;
const FindOperator FD_ORDERED_LOCONST		= 2;
const FindOperator FD_GT				= 2;
const FindOperator FD_LT				= 3;
const FindOperator FD_GE				= 4;
const FindOperator FD_LE				= 5;
const FindOperator FD_RANGE2_LOCONST		= 6;
const FindOperator FD_RANGE_GE_LE		= 6;
const FindOperator FD_RANGE				= FD_RANGE_GE_LE;
const FindOperator FD_RANGE_GT_LE		= 7;
const FindOperator FD_RANGE_GE_LT		= 8;
const FindOperator FD_RANGE_GT_LT		= 9;
const FindOperator FD_RANGE_HICONST			= 9;
const FindOperator FD_LIKE				= 10;
const FindOperator FD_UNLIKE			= 11;
const FindOperator FD_ORDERED_HICONST		= 11;
const FindOperator FD_PRESENT			= 12;
const FindOperator FD_POINT$			= 13;
const FindOperator FD_NON_FIELD				= 13;
const FindOperator FD_SET$				= 14; //BMRS pointer cast (optr not reqd by API)
const FindOperator FD_FILE$				= 15;
const FindOperator FD_SINGLEREC			= 16;
const FindOperator FD_ALLRECS			= 17; //similar to FILE$ but matches all files
const FindOperator FD_ALLVALUES			= 18;
const FindOperator FD_HI_BASIC_OPTR			= 18;

const int FD_NOT = 256;
const FindOperator FD_NOT_EQ			= FD_NOT | FD_EQ;
const FindOperator FD_NOT_GT			= FD_NOT | FD_GT;
const FindOperator FD_NOT_LT			= FD_NOT | FD_LT;
const FindOperator FD_NOT_GE			= FD_NOT | FD_GE;
const FindOperator FD_NOT_LE			= FD_NOT | FD_LE;
const FindOperator FD_NOT_LIKE			= FD_NOT | FD_LIKE;
const FindOperator FD_NOT_UNLIKE		= FD_NOT | FD_UNLIKE;
const FindOperator FD_NOT_PRESENT		= FD_NOT | FD_PRESENT;
const FindOperator FD_NOT_RANGE_GE_LE	= FD_NOT | FD_RANGE_GE_LE;
const FindOperator FD_NOT_RANGE			= FD_NOT_RANGE_GE_LE;
const FindOperator FD_NOT_RANGE_GT_LE	= FD_NOT | FD_RANGE_GT_LE;
const FindOperator FD_NOT_RANGE_GE_LT	= FD_NOT | FD_RANGE_GE_LT;
const FindOperator FD_NOT_RANGE_GT_LT	= FD_NOT | FD_RANGE_GT_LT;
const FindOperator FD_NOT_POINT$		= FD_NOT | FD_POINT$;
const FindOperator FD_NOT_SINGLEREC		= FD_NOT | FD_SINGLEREC;
const FindOperator FD_NORECS			= FD_NOT | FD_ALLRECS;

//These can be used to improve code readability by "documenting" the search type that
//will happen, but beware - you can force accidental table B searches if the requested
//condition does not have a matching index.  Of course sometimes it would not be accidental.
//These are what the UL compiler uses if the programmer gives force criteria (ALPHA etc).
const int FD_FORCE_ALPHA = 512;
const FindOperator FD_A_EQ			= FD_FORCE_ALPHA | FD_EQ;
const FindOperator FD_A_NOT_EQ		= FD_FORCE_ALPHA | FD_NOT_EQ;
const FindOperator FD_A_GT			= FD_FORCE_ALPHA | FD_GT;
const FindOperator FD_A_NOT_GT		= FD_FORCE_ALPHA | FD_NOT_GT;
const FindOperator FD_A_LT			= FD_FORCE_ALPHA | FD_LT;
const FindOperator FD_A_NOT_LT		= FD_FORCE_ALPHA | FD_NOT_LT;
const FindOperator FD_A_GE			= FD_FORCE_ALPHA | FD_GE;
const FindOperator FD_A_NOT_GE		= FD_FORCE_ALPHA | FD_NOT_GE;
const FindOperator FD_A_LE			= FD_FORCE_ALPHA | FD_LE;
const FindOperator FD_A_NOT_LE		= FD_FORCE_ALPHA | FD_NOT_LE;
const FindOperator FD_A_RANGE		= FD_FORCE_ALPHA | FD_RANGE;
const FindOperator FD_A_NOT_RANGE	= FD_FORCE_ALPHA | FD_NOT_RANGE;

//As above
const int FD_FORCE_NUM = 1024;
const FindOperator FD_N_EQ			= FD_FORCE_NUM | FD_EQ;
const FindOperator FD_N_NOT_EQ		= FD_FORCE_NUM | FD_NOT_EQ;
const FindOperator FD_N_GT			= FD_FORCE_NUM | FD_GT;
const FindOperator FD_N_NOT_GT		= FD_FORCE_NUM | FD_NOT_GT;
const FindOperator FD_N_LT			= FD_FORCE_NUM | FD_LT;
const FindOperator FD_N_NOT_LT		= FD_FORCE_NUM | FD_NOT_LT;
const FindOperator FD_N_GE			= FD_FORCE_NUM | FD_GE;
const FindOperator FD_N_NOT_GE		= FD_FORCE_NUM | FD_NOT_GE;
const FindOperator FD_N_LE			= FD_FORCE_NUM | FD_LE;
const FindOperator FD_N_NOT_LE		= FD_FORCE_NUM | FD_NOT_LE;
const FindOperator FD_N_RANGE		= FD_FORCE_NUM | FD_RANGE;
const FindOperator FD_N_NOT_RANGE	= FD_FORCE_NUM | FD_NOT_RANGE;

//Useful for diagnostics
inline FindOperator FOBasic(FindOperator op) {return (op & 0x0000001F);}
inline bool FOIsRange2(FindOperator in) {FindOperator o = FOBasic(in); return (o >= FD_RANGE2_LOCONST && o <= FD_RANGE_HICONST);}
inline bool FOIsOrdered(FindOperator in) {FindOperator o = FOBasic(in); return (o >= FD_ORDERED_LOCONST && o <= FD_ORDERED_HICONST);}
inline bool FOIsPattern(FindOperator in) {FindOperator o = FOBasic(in); return (o == FD_LIKE || o == FD_UNLIKE);}
inline bool FOIsField(FindOperator in) {FindOperator o = FOBasic(in); return (o > 0 && o < FD_NON_FIELD);}
inline bool FONonField(FindOperator in) {FindOperator o = FOBasic(in); return (o >= FD_NON_FIELD);}
extern const char* fodesc[FD_HI_BASIC_OPTR+1];
inline const char* FODesc(FindOperator in) {FindOperator o = FOBasic(in); return fodesc[o];}

//-------------------------------------
typedef unsigned int FindEnqueueType;

const FindEnqueueType FD_LOCK_NONE = 0;
const FindEnqueueType FD_LOCK_SHR = 1;
const FindEnqueueType FD_LOCK_EXCL = 2;

const FindEnqueueType FD_WITHOUT_LOCKS = FD_LOCK_NONE;
const FindEnqueueType FD_AND_RESERVE = FD_LOCK_EXCL;

//-------------------------------------
//Incremental diagnostic level.  The detail levels are not strictly adhered to really - 
//critlog is moderate detail, critcounts is full detail.  1+2+4 is also quite useful.
const unsigned int FD_RTD_SPEC			= 1; //shows the input find specification
const unsigned int FD_RTD_WORKINIT		= 2; //shows restructured spec
const unsigned int FD_RTD_NORUN			= 4; //stop after the above 2 diagnostics printed
const unsigned int FD_RTD_CRIT_LOG		= 8; //describe each criterion pre-execution
const unsigned int FD_RTD_CRIT_COUNTS	= 16; //crit set counts before/after (implies 8)

//-------------------------------------
typedef unsigned char SortType;
//These values must stay the same - they're used by the UL compiler.
const SortType SORT_CHAR = 0;
const SortType SORT_CHARACTER = SORT_CHAR;
const SortType SORT_NUM = 1;
const SortType SORT_NUMERIC = SORT_NUM;

const SortType SORT_RIGHT_ADJUSTED = 2;
const SortType SORT_DEFAULT_TYPE = 3;
const SortType SORT_NONE = 4;
const SortType SORT_NOCASE = 5;

typedef bool SortDirection;
const SortDirection SORT_ASC = true;
const SortDirection SORT_ASCENDING = SORT_ASC;
const SortDirection SORT_DESC = false;
const SortDirection SORT_DESCENDING = SORT_DESC;

const bool SORT_BY_EACH = true;
const bool SORT_COLLECT_ALL_OCCS = true;
const bool SORT_COLLECT_FIRSTOCC = false;

//-------------------------------------
typedef bool CursorDirection;
const CursorDirection CURSOR_ASC = SORT_ASC;
const CursorDirection CURSOR_ASCENDING = SORT_ASC;
const CursorDirection CURSOR_DESC = SORT_DESC;
const CursorDirection CURSOR_DESCENDING = SORT_DESC;

//OR these together as required
typedef unsigned int CursorOptions;
const CursorOptions CURSOR_DEFOPTS = 0x0000;
//Options controlling Advance() behaviour
const CursorOptions CURSOR_ADV_NO_OVERRUN = 0x0001; //Don't invalidate when one end reached
//Options controlling SetPosition() behaviour
const CursorOptions CURSOR_POS_FROM_CURRENT = 0x0000; //Start b-tree walk from current pos
const CursorOptions CURSOR_POS_FROM_FIRST = 0x1000; //...from start of cursor range
const CursorOptions CURSOR_POS_FROM_LAST = 0x2000; //...from end of it
const CursorOptions CURSOR_POSFAIL_INVALIDATE = 0x0000; //Entry not found - break cursor
const CursorOptions CURSOR_POSFAIL_REMAIN = 0x0100; //...revert to old position
const CursorOptions CURSOR_POSFAIL_NEXT = 0x0200; //... entry after desired entry used
const CursorOptions CURSOR_POSFAIL_PREV = 0x0400; //... entry before it


//-------------------------------------
const int DEFAULT_CPTO = -2;
const int DEFAULT_BUFAGE = -2;
const int ROLLBACK_OK = 0;
const int ROLLBACK_BYPASS = 1;
const int ROLLBACK_FAIL = 2;
const int ZORDER_STRICT = -1;

//-------------------------------------
enum FieldStorageFormat {
	FDEF_FLOAT,
	FDEF_STRING,
	FDEF_BLOB,        //V3.0, a variation of STRING
	FDEF_INVISIBLE
};
enum FieldStorageUpdatePosition {
	FDEF_UPDATE_IN_PLACE,
	FDEF_UPDATE_AT_END
};
enum FieldBtreeType {
	FDEF_NON_ORDERED,
	FDEF_ORDERED_CHARACTER,
	FDEF_ORDERED_NUMERIC
};

//-------------------------------------

//V2.10 - Dec 07.
typedef short DUFormat;
const DUFormat DU_FORMAT_DEFAULT = 0x0000;
const DUFormat DU_FORMAT_NOCRLF  = 0x0001;
const DUFormat DU_FORMAT_NOPAD   = 0x0002;
const DUFormat DU_FORMAT_DISCARD = 0x1000;

//V3.0 
typedef unsigned int FastUnloadOptions;
const FastUnloadOptions FUNLOAD_NULLOPTS          = 0x00000000;
const FastUnloadOptions FUNLOAD_WHAT_F            = 0x00000100;
const FastUnloadOptions FUNLOAD_WHAT_D            = 0x00000200;
const FastUnloadOptions FUNLOAD_WHAT_I            = 0x00000400;
const FastUnloadOptions FUNLOAD_REPLACE           = 0x00001000;
const FastUnloadOptions FUNLOAD_EXCLUDE_FIELDS    = 0x00004000;
const FastUnloadOptions FUNLOAD_FNAMES            = 0x00010000;
const FastUnloadOptions FUNLOAD_NOFLOAT           = 0x00020000;
const FastUnloadOptions FUNLOAD_EBCDIC            = 0x00040000;
const FastUnloadOptions FUNLOAD_IENDIAN           = 0x00080000;
const FastUnloadOptions FUNLOAD_FENDIAN           = 0x00100000;
const FastUnloadOptions FUNLOAD_CRLF              = 0x00200000;
const FastUnloadOptions FUNLOAD_PAI               = 0x20000000;
const FastUnloadOptions FUNLOAD_ENDIAN            = FUNLOAD_IENDIAN | FUNLOAD_FENDIAN;
const FastUnloadOptions FUNLOAD_DATA_REFORMAT     = FUNLOAD_FNAMES | FUNLOAD_NOFLOAT | FUNLOAD_EBCDIC |
                                                    FUNLOAD_ENDIAN | FUNLOAD_PAI;
const FastUnloadOptions FUNLOAD_ALLINFO           = FUNLOAD_WHAT_F | FUNLOAD_WHAT_D | FUNLOAD_WHAT_I;
const FastUnloadOptions FUNLOAD_ANYINFO           = FUNLOAD_ALLINFO;
const FastUnloadOptions FUNLOAD_DEFAULT           = FUNLOAD_ALLINFO;

typedef unsigned int FastLoadOptions;
const FastLoadOptions   FLOAD_DEFAULT             = 0x00000000;
const FastLoadOptions   FLOAD_NOACTION            = 0x00000001;
const FastLoadOptions   FLOAD_CLEANAFTER          = 0x00000002;
const FastLoadOptions   FLOAD_CLEANONLY           = FLOAD_NOACTION | FLOAD_CLEANAFTER;

//Defaulting to the same dir makes an unload/reload sequence work with all default options
#define FUNLOAD_DIR "#FASTIO"
#define FLOAD_DIR "#FASTIO"

} //close namespace

#endif
