/*******************************************************************************************
A sub-class largely just to keep this stuff from clogging up the DatabaseFile code.
This class handles all the work that gets done under the protection of the DIRECT CFR.
That is, manipulation of table B pages.
*******************************************************************************************/
#if !defined(BB_DBF_DATA)
#define BB_DBF_DATA

#include "atom.h"
#include "atomback.h"
#include "dbfile.h"
#include "bbfloat.h"
#include "fieldval.h"
#include "infostructs.h"
#include "bbthread.h"

namespace dpt {

class DatabaseServices;
class SingleDatabaseFileContext;
class BitMappedRecordSet;
class BitMappedFileRecordSet;
class Record;
struct PhysicalFieldInfo;
class RecordDataAccessor;
class FastUnloadRequest;
class FastLoadRequest;
class FieldValue;
class RecordCopy;
typedef short FieldID;
class FastLoadRecordBuffer;

//**************************************************************************************
class DatabaseFileDataManager {
	DatabaseFile* file;

	//Used to handle extension processing and occurrence scans.  Using vector because
	//faster than multimap in the most common case of empty.
	std::vector<std::pair<int, RecordDataAccessor*> > record_mros;
	int record_mros_hwm;
	Lockable record_mro_lock;
	void RegisterRecordMRO(RecordDataAccessor*);
	void DeregisterRecordMRO(RecordDataAccessor*);
	void FlagUpdatedRecordMROs(RecordDataAccessor*, bool);
	void FlagDeletedRecordMROs(RecordDataAccessor*);
	friend class RecordDataAccessor;

	void ProcessChunkXferBuffer(SingleDatabaseFileContext*, FastLoadRequest*, 
		DeferredUpdate1StepInfo*, FastLoadRecordBuffer&, int, bool, bool, bool, bool);
	void AddIndexEntryFromTapeDItem(SingleDatabaseFileContext*, DeferredUpdate1StepInfo*,
		const PhysicalFieldInfo*, const FieldValue&, int);

public:
	DatabaseFileDataManager(DatabaseFile* f);

	//Reading functions
	bool ReadFieldValue(const Record*, PhysicalFieldInfo*, int, FieldValue&, bool = true);
	int CountOccurrences(const Record*, PhysicalFieldInfo*);

	bool GetNextFVPair(const Record*, FieldID*, FieldValue*, FieldValue*, int&);
	void CopyAllInformation(const Record*, RecordCopy&);

	//Updating functions
	int Atom_StoreEmptyRecord(DatabaseServices*, int = -1);
	void Atom_DeleteEmptyRecord(Record*);

	int Atom_InsertField(Record*, PhysicalFieldInfo*, const FieldValue&, int, bool*, bool = false);

	int Atom_ChangeField(bool, Record*, PhysicalFieldInfo*, const FieldValue&, 
		const int, const FieldValue*, int*, FieldValue*, bool*, bool*);

	int Atom_DeleteField(bool, Record*, PhysicalFieldInfo*, 
		const int, const FieldValue*, FieldValue*, bool*);

	//DBA stuff
	std::string ShowPhysicalInformation(Record*);
	void Unload(FastUnloadRequest*);
	int Load(FastLoadRequest*, BB_OPDEVICE*);

	void DeleteFieldFromEveryRecord(SingleDatabaseFileContext*, PhysicalFieldInfo*);
	void ChangeFieldFormatOnEveryRecord(SingleDatabaseFileContext*, PhysicalFieldInfo*, bool);
	void VisiblizeFieldFromIndex(SingleDatabaseFileContext*, PhysicalFieldInfo*, bool);
};







//**************************************************************************************
class AtomicUpdate_StoreEmptyRecord : public AtomicUpdate {
	int recnum;

public:
	AtomicUpdate_StoreEmptyRecord(SingleDatabaseFileContext* c) 
		: AtomicUpdate(c), recnum(-1) {}
	AtomicBackout* CreateCompensatingUpdate();
	int Perform();
};

//*********TBO*********
class AtomicBackout_DeStoreEmptyRecord : public AtomicBackout {
	int recnum;

public:
	AtomicBackout_DeStoreEmptyRecord(SingleDatabaseFileContext* c)
		: AtomicBackout(c), recnum(-1) {}
	void SetRecNum(int n) {recnum = n;}
	void Perform();
};






//**************************************************************************************
class AtomicUpdate_DeleteEmptyRecord : public AtomicUpdate {
	Record* record;

public:
	AtomicUpdate_DeleteEmptyRecord(Record*);
	AtomicBackout* CreateCompensatingUpdate();
	void Perform();
};

//*********TBO*********
class AtomicBackout_DeDeleteEmptyRecord : public AtomicBackout {
	int recnum;

public:
	AtomicBackout_DeDeleteEmptyRecord(Record*); 
	void Perform();
};






//**************************************************************************************
class AtomicUpdate_InsertFieldData : public AtomicUpdate {
	Record* record;
	PhysicalFieldInfo* fieldinfo;
	const FieldValue& fieldval;
	int occ;
	bool* index_reqd;
	//V2.03
	bool file_registered;
	bool du_mode;

public:
	AtomicUpdate_InsertFieldData(Record* r, PhysicalFieldInfo*, 
		const FieldValue&, int, bool*, bool = false, bool = false);
	AtomicBackout* CreateCompensatingUpdate();
	int Perform();
};

//*********TBO*********
class AtomicBackout_DeInsertFieldData : public AtomicBackout {
	int recnum;
	PhysicalFieldInfo* fieldinfo;
	int occ_actual;

public:
	AtomicBackout_DeInsertFieldData(Record*, PhysicalFieldInfo*); 

	void SetInsertedOcc(int o) {occ_actual = o;}
	void Perform();
};







//**************************************************************************************
class AtomicUpdate_ChangeFieldData : public AtomicUpdate {
	Record* record;
	PhysicalFieldInfo* fieldinfo;
	const FieldValue& newval;

	bool by_value;
	const FieldValue* oldval;
	int occ;

	bool* index_reqd_newval;
	bool* index_reqd_oldval;

public:
	//Change by occurrence
	AtomicUpdate_ChangeFieldData(Record* r, PhysicalFieldInfo*, 
								const FieldValue&, int, bool*, bool*);
	//Change by value
	AtomicUpdate_ChangeFieldData(Record* r, PhysicalFieldInfo*, 
								const FieldValue&, const FieldValue*, bool*, bool*);

	AtomicBackout* CreateCompensatingUpdate();
	int Perform(FieldValue*);
};

//*********TBO*********
class AtomicBackout_DeChangeFieldData : public AtomicBackout {
	int recnum;
	PhysicalFieldInfo* fieldinfo;

	FieldValue oldval;
	int occ_deleted;
	int occ_added;

public:
	AtomicBackout_DeChangeFieldData(Record*, PhysicalFieldInfo*); 
//	~AtomicBackout_DeChangeFieldData();

	void SetDetails(int d, int a, FieldValue* v);
	void Perform();
};







//**************************************************************************************
class AtomicUpdate_DeleteFieldData : public AtomicUpdate {
	Record* record;
	PhysicalFieldInfo* fieldinfo;

	bool by_value;
	int occ;
	const FieldValue* oldval;

	bool* index_reqd;

public:
	//Delete by occurrence
	AtomicUpdate_DeleteFieldData(Record* r, PhysicalFieldInfo*, int, bool*);
	//Delete by value
	AtomicUpdate_DeleteFieldData(Record* r, PhysicalFieldInfo*, const FieldValue*, bool*);

	AtomicBackout* CreateCompensatingUpdate();
	int Perform(FieldValue*);
};

//*********TBO*********
class AtomicBackout_DeDeleteFieldData : public AtomicBackout {
	int recnum;
	PhysicalFieldInfo* fieldinfo;

	FieldValue oldval;
	int occ_deleted;

public:
	AtomicBackout_DeDeleteFieldData(Record*, PhysicalFieldInfo*); 
//	~AtomicBackout_DeDeleteFieldData(); 

	void SetDetails(int o, FieldValue* v);
	void Perform();
};




//**************************************************************************************
//V3.0
class FastUnloadDataParallelSubTask : public util::WorkerThreadSubTask {
	FastUnloadRequest* request;

	void Perform();
public:
	FastUnloadDataParallelSubTask(FastUnloadRequest* r)
		: WorkerThreadSubTask("Data records"), request(r) {}
};


} //close namespace

#endif
