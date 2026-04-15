
#if !defined(BB_RECSET)
#define BB_RECSET

#include <set>
#include "dbcursor.h"

namespace dpt {

class RecordSetCursor;
class SingleDatabaseFileContext;
class DatabaseFileContext;
class Record;
class ReadableRecord;
class RecordSetHandle;
class BitMappedRecordSet;

//***************************************************************************************
//FoundSet, RecordList, SortedRecordSet
//***************************************************************************************
class RecordSet : public DBCursorTarget {
	DatabaseFileContext* context;

	friend class RecordSetHandle;
	RecordSetHandle* handle;

	virtual void DestroySetData() = 0;

	virtual void GetRecordNumberArray_D(int*, int) = 0;
	Record* random_access_record;

protected:
	RecordSet(DatabaseFileContext* c) 
		: context(c), handle(NULL), affected_by_dirty_delete(false), random_access_record(NULL) {}

	bool affected_by_dirty_delete;

	friend class DatabaseFileContext;
	virtual ~RecordSet();
	virtual void NotifyOfDirtyDelete(BitMappedRecordSet*) {affected_by_dirty_delete = true;}

public:
	DatabaseFileContext* Context() const {return context;}

	virtual bool IsSortSet() {return false;} //V2.25 helps clarify the C API 

	//Releases all storage and locks as per RELEASE RECORDS in UL
	void Clear() {RequestRepositionCursors(11); DestroySetData();}

	virtual int Count() const = 0;

	virtual RecordSetCursor* OpenCursor(bool gotofirst = true) = 0;
	void CloseCursor(RecordSetCursor* c) {DeleteCursor((DBCursor*)c);}

	//In case records accessed via the set need to check the EBM
	bool AffectedByDirtyDelete() {return affected_by_dirty_delete;}

	//V3.0.  Can make scrolling GUIs easier to build (cursors are not ideal with large sets
	//which allow random-access scrolling).
	int* GetRecordNumberArray(int* dest = NULL, int getmaxrecs = -1);
	Record* AccessRandomRecord(int recnum);
};


//***************************************************************************************
class RecordSetCursor : public DBCursor {

	Record* prevrealmro;
	void ForgetPrevRealRec();

	int last_advanced_rec_num;
	SingleDatabaseFileContext* last_advanced_sfc;

	friend class DatabaseFileDataManager;
	friend class DatabaseFileIndexManager;
	Record* AccessCurrentRealRecord_B(bool);

protected:
	RecordSetCursor(RecordSet* s) : DBCursor(s), prevrealmro(NULL) {SetLARInfo(-1, NULL);}
	virtual ~RecordSetCursor() {ForgetPrevRealRec();}

	void SetLARInfo(int rn, SingleDatabaseFileContext* sfc) {
		last_advanced_rec_num = rn; last_advanced_sfc = sfc;}

public:
	RecordSet* Set() {return static_cast<RecordSet*>(Target());}

	bool Accessible() {return last_advanced_rec_num != -1;}

	//Use these just to get information in loops - quicker than creating Record* MRO
	int LastAdvancedRecNum() {return last_advanced_rec_num;}
	SingleDatabaseFileContext* LastAdvancedFileContext() {return last_advanced_sfc;}

	//Separate functions to allow a common interface for sorted and unsorted sets.
	virtual ReadableRecord* AccessCurrentRecordForRead() = 0;
	Record* AccessCurrentRecordForReadWrite() {return AccessCurrentRealRecord();}
	Record* AccessCurrentRealRecord() {return AccessCurrentRealRecord_B(false);}

	//These are used by the UL REMEMBER statement and might be generally useful
	virtual RecordSetCursor* CreateClone() = 0;
	virtual void ExportPosition(RecordSetCursor* dest) = 0;
};

} //close namespace

#endif
