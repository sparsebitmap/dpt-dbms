//**************************************************************************************
//Function loading up reference information for parameters used by levels above
//CoreServices.  See parmref.cpp.
//**************************************************************************************

#include "stdafx.h"

#include "parmref.h"

namespace dpt {

void ParmRefTable::DBAPIExtraConstructor()
{

//*******************
//User parameters
//*******************

//M204 emulated
//=============
StoreEntry("ENQRETRY", "4", 255, 0, "Record locking auto retries", user);
StoreEntry("MBSCAN", "-1", INT_MAX, -1, "Max records to scan in a find", user);
StoreEntry("MDKRD", "1000000", INT_MAX, -1, "Max physical disk reads per request", user);
StoreEntry("MDKWR", "1000000", INT_MAX, -1, "Max physical disk writes per request", user);
StoreEntry("UPDTID", "", 0, 0, "Current transaction ID", user, never_resettable);

//Baby204 specific
//================

//*******************
//System parameters
//*******************

//M204 emulated
//=============
StoreEntry("CPTO", "5", INT_MAX, -1, "Checkpoint time-out interval in seconds", system);
StoreEntry("CPTIME", "5", INT_MAX, -1, "Automatic checkpoint interval in minutes", system);
StoreEntry("DUFILES", "0", 0, 0, "Number of current deferred update files", system, never_resettable);
StoreEntry("DTSLCHKP", "", INT_MAX, 0, "Date/time of last checkpoint", system, never_resettable);
StoreEntry("DTSLRCVY", "", INT_MAX, 0, "Date/time of last recovery", system, never_resettable);
StoreEntry("DTSLDKWR", "", INT_MAX, 0, "Date/time of last DKWR", system, never_resettable);
StoreEntry("DTSLUPDT", "", INT_MAX, 0, "Date/time of last update", system, never_resettable);
StoreEntry("MAXBUF", "10000", 262144, 32, "Max # of disk buffer pages", system, inifile_only);
StoreEntry("PAGESZ", "", 0, 0, "Database file page size", system, never_resettable);
//NB. Dynamic reset of 2 bit (TBO) if ever implemented would require all files' CFR_UPDATING.
StoreEntry("RCVOPT", "3", 255, 0, "Recovery options", system, inifile_only);

//Baby204 specific
//================
StoreEntry("BTREEPAD", "0", 8100, 0, "Training/debug btree node pre-padding", system, inifile_only);
StoreEntry("BUFAGE", "600", INT_MAX, 0, "Buffer memory retention period in seconds", system);
StoreEntry("FMODLDPT", "3", 3, 0, "File model options (like M204 FILEMODL)", system);
StoreEntry("MAXRECNO", "16777215", INT_MAX-1, 0, "Maximum allowed internal record number", system);

//Assorted load control.  Started in V2.17
StoreEntry("LOADCTL", "0", 4, 0, "Load/unload: Miscellaneous control flags", system);
StoreEntry("LOADDIAG", "0", 4, 0, "Load/unload: Diagnostics level", system);
StoreEntry("LOADFVPC", "0", INT_MAX, 0, "Load: Forced FV pairs per chunk", system);
StoreEntry("LOADMEMP", "75", 95, 5, "Load: Real memory % usage target", system);
StoreEntry("LOADMMFH", "256", INT_MAX, 2, "Load: Merge max file handles", system);
StoreEntry("LOADTHRD", "1", INT_MAX, 1, "Load/unload: Parallel processing threads", system);





//*******************
//File parameters
//*******************

//M204 emulated
//=============

//"tables"
StoreEntry("BSIZE", "5", INT_MAX, 1, "Pages in table B", tables, never_resettable);
//NB. The maximums of these two mean the first record can always be started on a page
StoreEntry("BRECPPG", "256", 1024, 1, "Records per table B page", tables, never_resettable);
StoreEntry("BRESERVE", "17", 4096, 0, "Table B page expansion bytes", tables, not_inifile);
StoreEntry("BREUSE", "20", 100, 0, "Table B page reusability", tables, not_inifile);
StoreEntry("DSIZE", "15", INT_MAX, 3, "Pages in table D", tables, never_resettable);
StoreEntry("DRESERVE", "15", 100, 1, "Table D list page expansion percent", tables, not_inifile);
StoreEntry("DPGSRES", "0", 32767, 0, "Table D pages reserved for TBO", tables, not_inifile);

//"fparms"
StoreEntry("FICREATE", "0", 0, 0, "File create generation", fparms, never_resettable);
StoreEntry("FIFLAGS", "File status flags", num, fparms);
StoreEntry("FILEORG", "0", 0x24, 0, "File organization flags", fparms, never_resettable);
StoreEntry("FISTAT", "0", 0, 0, "File status flags", fparms, not_inifile);
StoreEntry("FIWHEN", "Date/time when FISTAT was reset", num, fparms);
StoreEntry("FIWHO", "User who reset FISTAT", alpha, fparms);
StoreEntry("SEQOPT", "0", 128, 0, "Speculative disk read extra pages", fparms, not_inifile);
StoreEntry("OSNAME", "OS file name (abbreviation)", alpha, fparms);

//usage stats, also appearing as "tables"
StoreEntry("ATRPG", "Field attribute pages", num, tables);
StoreEntry("BHIGHPG", "Table B high water mark", num, tables);
StoreEntry("BREUSED", "Records added reusing record numbers", num, tables);
StoreEntry("BQLEN", "Table B reuse queue length", num, tables);
StoreEntry("EXTNADD", "Extension records added", num, tables);
StoreEntry("EXTNDEL", "Extension records deleted", num, tables);
StoreEntry("MSTRADD", "Primary record extents added", num, tables);
StoreEntry("MSTRDEL", "Primary record extents deleted", num, tables);
StoreEntry("DACTIVE", "Active inverted list page", num, tables);
StoreEntry("DHIGHPG", "Table D high water mark", num, tables);
StoreEntry("DPGSUSED", "Table D pages in use", num, tables);

//Baby204 specific
//================
StoreEntry("ATRFLD", "Fields defined", num, tables);
StoreEntry("ILACTIVE", "Active inverted list master record page", num, tables);

StoreEntry("DPGS_1", "Table D pages type 1 = field attributes, aka 'table A'", num, tables);
StoreEntry("DPGS_2", "Table D pages type 2 = existence bitmap index", num, tables);
StoreEntry("DPGS_3", "Table D pages type 3 = existence bitmaps", num, tables);
StoreEntry("DPGS_4", "Table D pages type 4 = inverted list master records", num, tables);
StoreEntry("DPGS_5", "Table D pages type 5 = inverted lists", num, tables);
StoreEntry("DPGS_6", "Table D pages type 6 = compressed inverted lists", num, tables);
StoreEntry("DPGS_7", "Table D pages type 7 = b-tree nodes", num, tables);
StoreEntry("DPGS_8", "Table D pages type 8 = BLOBs, aka 'table E'", num, tables); //V3.0
StoreEntry("DPGS_X", "Table D pages type X = null/reusable", num, tables);

}

} //close namespace

