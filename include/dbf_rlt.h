/*******************************************************************************************
A sub-class largely just to keep this stuff from clogging up the DatabaseFile code.
This class deals with the record locking table for a file.
*******************************************************************************************/
#if !defined(BB_DBF_RLT)
#define BB_DBF_RLT

#include "lockable.h"
#include <set>
#include <map>

namespace dpt {

class DatabaseServices;
class DatabaseFile;
class FileRecordSet;
class FileRecordSet_RecordUpdate;
class FileRecordSet_PAI;
class RecordLock;

//*****************************************************************************************
class DatabaseFileRLTManager {
	DatabaseFile* file;

	std::set<RecordLock*> shr_locks;
	Sharable shr_table_lock;
	std::set<RecordLock*> excl_locks;
	Sharable excl_table_lock;
	std::map<int, RecordLock*> record_update_locks;
	Sharable recupdate_table_lock;

	Sharable waiter_table_lock;
	struct WaiterInfo {
		DatabaseServices* dbapi;
		volatile bool release_flag;
		WaiterInfo(DatabaseServices* d) : dbapi(d), release_flag(false) {}
	};
	std::multimap<RecordLock*, WaiterInfo*> waiter_table;
	WaiterInfo* InsertWaiterInfo(DatabaseServices*, RecordLock*);
	void DeleteWaiterInfo(DatabaseServices*, RecordLock*);
	bool EnqRetryWait(DatabaseServices*, RecordLock*, int&);
	void NotifyWaitersOfLockRelease(RecordLock*);

	void SerialSearchShrLockTable(FileRecordSet*, DatabaseServices*, bool);
	void SerialSearchExclLockTable(FileRecordSet*, DatabaseServices*, bool);

	//Two types of scan for performance
	void SerialSearchRecordUpdateLockTable(FileRecordSet*, DatabaseServices*);
	bool KeyedSearchRecordUpdateLockTable(FileRecordSet_RecordUpdate*, DatabaseServices*);

	void ThrowRLC(RecordLock*, DatabaseServices*, bool);

	RecordLock* ApplyShrSetLock_S(FileRecordSet*, DatabaseServices*, bool);

public:
	DatabaseFileRLTManager(DatabaseFile* f) : file(f) {}

	RecordLock* ApplyShrFoundSetLock(FileRecordSet* s, DatabaseServices* d);
	RecordLock* ApplyExclFoundSetLock(FileRecordSet*, DatabaseServices*);
	RecordLock* ApplyRecordUpdateLock(FileRecordSet_RecordUpdate*, DatabaseServices*);
	RecordLock* ApplyPAIRecordLock(FileRecordSet_PAI* s, DatabaseServices* d);

	void ReleaseNormalLock(RecordLock*);
	void ReleaseRecordUpdateLock(int, RecordLock*);

	void TBSReplaceLockSet(FileRecordSet*, FileRecordSet*);
	Sharable* TBSExposeShrTableLock() {return &shr_table_lock;}
};

//*****************************************************************************************
class RecordLock {
	time_t start_time;
	DatabaseServices* holder;
	FileRecordSet* frecset;
	bool is_excl;

	friend class DatabaseFileRLTManager;
	RecordLock(DatabaseServices*, bool, FileRecordSet*);

	DatabaseServices* ClashTest(FileRecordSet*, DatabaseServices*);

public:
	bool IsExcl() {return is_excl;}
};

} //close namespace

#endif
