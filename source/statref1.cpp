
#include "stdafx.h"

#include "statref.h"

namespace dpt {

//**************************************************************************************
void StatRefTable::DBAPIExtraConstructor()
{

//DBAPI stats with file implications too
StoreEntry("BACKOUTS", "Times TBO invoked", true);
StoreEntry("COMMITS", "Transactions committed", true);

//File stats
StoreEntry("BADD", "Fields added to table B records", true);
StoreEntry("BCHG", "Fields changed in table B records", true);
StoreEntry("BDEL", "Fields deleted from table B records", true);
StoreEntry("BXDEL", "B-tree value entries deleted", true);
StoreEntry("BXFIND", "B-tree value search operations", true);
StoreEntry("BXFREE", "B-tree node pages removed", true);
StoreEntry("BXINSE", "B-tree value entries inserted", true);
StoreEntry("BXNEXT", "B-tree nodes traversed horizontally", true);
StoreEntry("BXRFND", "B-tree re-finds caused by other thread", true);
StoreEntry("BXSPLI", "B-tree node splits", true);
StoreEntry("DIRRCD", "Records scanned during table B searches", true);
StoreEntry("FINDS", "Find statements evaluated", true);
StoreEntry("RECADD", "Records added to table B", true);
StoreEntry("RECDEL", "Records deleted from table B", true);
StoreEntry("RECDS", "Records retrieved from table B (excluding DIRRCD)", true);
StoreEntry("SORTS", "Record and value sorts performed");
StoreEntry("STRECDS", "Records sorted", true);

//Buffer stats
StoreEntry("DKPR", "Disk page requests (logical page reads)", true);
StoreEntry("DKRD", "Physical disk pages read", true);
StoreEntry("DKWR", "Physical disk pages written", true);
StoreEntry("FBWT", "Waits for buffer: full pool, all in use", true);
StoreEntry("DKSFBS", "Scans past the first page on the LRUQ", true);
StoreEntry("DKSFNU", "Buffer pages taken off the LRUQ", true);
StoreEntry("DKSKIP", "Max dirty pages skipped in 1 LRUQ scan", true);
StoreEntry("DKSKIPT", "Dirty pages skipped during LRUQ scans", true);
StoreEntry("DKSWAIT", "Dirty page flushes when using LRU page", true);
StoreEntry("DKUPTIME", "Time spent physically writing pages", true);
StoreEntry("UPDTTIME", "Time spent with uncommitted logical updates", true);

//Sequential IO
StoreEntry("SEQI", "Lines read from sequential files");
StoreEntry("SEQO", "Lines written to sequential files");

//Wait stats
StoreEntry("BLKCFRE", "Times forced another user to wait for CFR", false);
StoreEntry("WTCFR", "Times had to wait for CFR", false);
StoreEntry("WTRLK", "Times had to wait for a record lock", false);

//DPT custom index stats - probably more than ultimately necessary but hopefully might
//be useful in the early days for diagnostics.
StoreEntry("ILMRADD", "Inverted list master records created", true);
StoreEntry("ILMRDEL", "Inverted list master records deleted", true);
StoreEntry("ILMRMOVE", "ILMR moves to pages with more room", true);
StoreEntry("ILRADD", "Records added to inverted lists", true);
StoreEntry("ILRDEL", "Records removed from inverted lists", true);
StoreEntry("ILSADD", "Inverted list segment entries added", true);
StoreEntry("ILSDEL", "Inverted list segment entries deleted", true);
StoreEntry("ILSMOVE", "IL segment data moves to pages with more room", true);

//Custom stats to give more info about FRV loops
StoreEntry("STVALS", "Values sorted", false);
StoreEntry("MERGES", "Individual file value sets merged", false);
StoreEntry("MRGVALS", "Values merged while building group value sets", true);

}

} //close namespace

