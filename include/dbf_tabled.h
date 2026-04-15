/*******************************************************************************************
A sub-class largely just to keep this stuff from clogging up the DatabaseFile code.
This class manages table D - the main part of that being to act as an interface for
allocating and returning pages to the "heap".  Called from dbf_index, dbf_field etc.
*******************************************************************************************/
#if !defined(BB_DBF_TABLED)
#define BB_DBF_TABLED

#include "lockable.h"
#include "buffhandle.h"

namespace dpt {

class DatabaseFile;
class DatabaseServices;
class SingleDatabaseFileContext;
class InvertedIndexListPage;
class InvertedListMasterRecordPage;

//*****************************************************************************************
class DatabaseFileTableDManager {
	DatabaseFile* file;

	int cached_dpgsres;
	int cached_dreserve;

	Lockable heap_lock;

	friend class DatabaseFileEBMManager;
	friend class DatabaseFileFieldManager;
	friend class BTreeAPI;
	friend class InvertedListAPI;

	int AllocatePage(DatabaseServices*, char, bool);
	void ReturnPage_S(DatabaseServices*, int, BufferPageHandle*);
	void ReturnPage(DatabaseServices* d, int i) {ReturnPage_S(d, i, NULL);}
	void ReturnPage(BufferPageHandle& h, int i) {ReturnPage_S(h.DBAPI(), i, &h);}

	BufferPageHandle AllocateRecNumList(DatabaseServices*, short, int&, short&, bool);
	void DeleteRecNumList(BufferPageHandle&, int, short);

	BufferPageHandle AllocateILMR(DatabaseServices*, short, int&, short&, bool);
	void DeleteILMR(BufferPageHandle&, int, short);

	//V3.0
	friend class RecordDataAccessor;
	BufferPageHandle StoreBLOBExtent(DatabaseServices*, const char**, int&, int&, short&, bool);
	void DeleteBLOB(DatabaseServices*, int, short);

public:
	DatabaseFileTableDManager(DatabaseFile* f) : file(f) {}
	void CacheParms();

	BufferPageHandle GetTableDPage(DatabaseServices*, int, bool = false);

	std::string ViewParm(SingleDatabaseFileContext*, const std::string&);
	void ResetParm(SingleDatabaseFileContext*, const std::string&, int);
};


} //close namespace

#endif
