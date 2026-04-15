/****************************************************************************************
Update units - i.e. database transactions.
****************************************************************************************/

#if !defined(BB_UPDATE)
#define BB_UPDATE

#include <map>
#include <set>
#include <vector>

#include "lockable.h"

namespace dpt {

class AtomicUpdate;
class AtomicBackout;
class DatabaseServices;
class DatabaseFile;
class Exception;
class FileRecordSet_RecordUpdate;

//***************************************************************************************
class UpdateUnit {

	DatabaseServices* dbapi;

	int id;
	bool real_updates;
	bool backoutable;
	_int64 updttime_base;
	static ThreadSafeLong sys_hi_id; //just for M204 appearance (e.g. viewable UPDTTID parm)

	std::map<DatabaseFile*, _int64> files; //data element is the updttime base for the file
	std::vector<AtomicBackout*> backout_log;

	std::set<unsigned int> my_constrained_recnums;
	static std::set<unsigned int> sys_constrained_recnums;
	static Lockable sys_constraints_lock;

	std::vector<FileRecordSet_RecordUpdate*> records_being_updated;
	void ReleaseRecordsBeingUpdated();

	//This stuff is used to try and increase the chances of backout success
	char* tbo_reserved_memory;
	bool backing_out;
	void AcquireTBOReservedMemory();
	void ReleaseTBOReservedMemory();

	//********************************
	friend class DatabaseServices;
	UpdateUnit(DatabaseServices* d) 
		: dbapi(d), id(-1), real_updates(false), updttime_base(0),
			tbo_reserved_memory(NULL), backing_out(false)  {AcquireTBOReservedMemory();}
	~UpdateUnit() {ReleaseTBOReservedMemory();}

	bool FileIsParticipating(DatabaseFile* f) {return (files.find(f) != files.end());}

	int ClearBackoutInfo();

public:
	bool AnyUpdates() {return (id != -1);}
	int ID() {return id;}
	bool IsBackoutable() {return backoutable;}
	bool IsCurrentlyBackingOut() {return backing_out;}

	void RegisterAtomicUpdateFile(AtomicUpdate*, DatabaseFile*);
	void LogCompensatingUpdate(AtomicUpdate*);

	//Constraints log
	void InsertConstrainedRecNum(int);
	void RemoveConstrainedRecNum(int);
	static bool FindConstrainedRecNum(int);

	bool Commit(bool if_backoutable, bool if_nonbackoutable);
	bool Backout(bool discreet = false);
	void ReleaseOrFlushFiles(bool, bool, bool);
	void AbruptEnd(); //Files will be logically inconsistent
	void EndOfMolecule(bool = false);

	void PlaceRecordUpdatingLock(DatabaseFile*, int);
	void MoleculeExceptionHandler(Exception* e, bool memory, DatabaseFile*);
};

} //close namespace

#endif
