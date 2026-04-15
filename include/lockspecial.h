
#if !defined BB_LOCKSINGLE
#define BB_LOCKSINGLE

#include "frecset.h"

namespace dpt {

class DatabaseServices;
class DatabaseFile;

//***************************************************************************************
class FileRecordSet_LockSpecialSingle : public FileRecordSet {
protected:
	int recnum;
public:
	FileRecordSet_LockSpecialSingle(DatabaseFile*, int);
	virtual ~FileRecordSet_LockSpecialSingle() {}

	int RecNum() {return recnum;}
	int RLCRec() {return recnum;}
	int SingleRecordNumber() const {return recnum;}
};

//***************************************************************************************
//For exclusive LPU of records that are updated
//***************************************************************************************
class FileRecordSet_RecordUpdate : public FileRecordSet_LockSpecialSingle {
public:
	FileRecordSet_RecordUpdate(DatabaseFile* f, int n) : FileRecordSet_LockSpecialSingle(f, n) {}
	~FileRecordSet_RecordUpdate() {Unlock(); DestroySetData();}
	bool ApplyLock(DatabaseServices*);
	void Unlock();
};

//***************************************************************************************
//For the hidden SHR lock placed on records during PAI to ensure record integrity
//***************************************************************************************
class FileRecordSet_PAI : public FileRecordSet_LockSpecialSingle {
	friend class PAIRecordLockSentry;
	FileRecordSet_PAI(DatabaseFile* f, int n) : FileRecordSet_LockSpecialSingle(f, n) {}
	~FileRecordSet_PAI() {Unlock(); DestroySetData();}

	void ApplyLock(DatabaseServices*);
public:
	void Unlock();
};

//**************************
class PAIRecordLockSentry {
	FileRecordSet_PAI set;
public:
	PAIRecordLockSentry(DatabaseServices* dbapi, DatabaseFile* f, int n) 
		: set(f, n) {set.ApplyLock(dbapi);}
	~PAIRecordLockSentry() {set.Unlock();}
};

}	//close namespace

#endif

