/*******************************************************************************************
A sub-class largely just to keep this stuff from clogging up the DatabaseFile code.
This class handles all the field definition work - page type A.
*******************************************************************************************/
#if !defined(BB_DBF_FIELD)
#define BB_DBF_FIELD

#include <string>
#include <vector>
#include <map>

#include "fieldinfo.h"
#include "apiconst.h"
#include "infostructs.h"
#include "bbthread.h"

namespace dpt {

class BufferPageHandle;
class FieldAttributes;
class FieldAttributePage;
class DatabaseFileTableDManager;
class Record;
class DatabaseFile;
class FastUnloadRequest;
class FastLoadRequest;
class SingleDatabaseFileContext;
class DatabaseServices;
class FieldValue;
typedef short FieldID;

//*****************************************************************************************
class DatabaseFileFieldManager {
	DatabaseFile* file;

	//The attributes are cached so that in general use we need not scan table A
	std::map<std::string, PhysicalFieldInfo*> namelookup;
	std::map<FieldID, PhysicalFieldInfo*> idlookup;
	Lockable lookup_table_lock; //V2.07 - Sep 07.
	bool lookup_tables_valid;
	void RefreshFieldInfo(SingleDatabaseFileContext*);
	void DestroyFieldInfo();
	void DestroyFieldInfoNoLock();

	std::string StandardizeFieldName(const std::string&);
	int DefineField_S1(DatabaseServices*, const std::string&, BufferPageHandle&);
	PhysicalFieldInfo* DefineField_S2(DatabaseServices*,
							const std::string&, FieldAttributes&, BufferPageHandle&, int&);

	int SoftInitialize(SingleDatabaseFileContext*);
	int GetNextAttPageNum(FieldAttributePage&, bool throwit = true);

	void MakeFileChanges_TableA(SingleDatabaseFileContext*, PhysicalFieldInfo*, const FieldAttributes&, const FieldAttributes&);
	void MakeFileChanges_TableB(SingleDatabaseFileContext*, PhysicalFieldInfo*, const FieldAttributes&, const FieldAttributes&);
	void MakeFileChanges_TableD1(SingleDatabaseFileContext*, PhysicalFieldInfo*, const FieldAttributes&, const FieldAttributes&);
	void MakeFileChanges_TableD2(SingleDatabaseFileContext*, PhysicalFieldInfo*, const FieldAttributes&, const FieldAttributes&);

	std::string MakeFieldAttsDDL(const FieldAttributes&, bool);

public:
	DatabaseFileFieldManager(DatabaseFile* f) : file(f), lookup_tables_valid(false) {}
	~DatabaseFileFieldManager() {DestroyFieldInfo();}

	std::string ViewParm(SingleDatabaseFileContext*, const std::string&);

	void DefineField(SingleDatabaseFileContext*, const std::string&, 
								bool, bool, bool, bool, bool, unsigned char, bool, bool);
	void RedefineField(SingleDatabaseFileContext*, const std::string&, const FieldAttributes&);
	void DeleteField(SingleDatabaseFileContext*, const std::string&);
	void RenameField(SingleDatabaseFileContext*, const std::string&, const std::string&);

	FieldAttributes GetFieldAtts(SingleDatabaseFileContext* c, const std::string& fname) {
		return GetPhysicalFieldInfo(c, fname)->atts;}

	void TakeFieldAttsTableCopy(SingleDatabaseFileContext*, std::vector<PhysicalFieldInfo>*);
	int NumFields(SingleDatabaseFileContext* c) {RefreshFieldInfo(c); return namelookup.size();}

	FieldID HighestFieldID(SingleDatabaseFileContext*);
	void GetIndexedAttsArray(SingleDatabaseFileContext*, std::vector<PhysicalFieldInfo*>*);

	int Initialize(SingleDatabaseFileContext*, bool);

	//When the btree levels up or down
	void UpdateBTreeRootPage(DatabaseServices*, PhysicalFieldInfo*);

	PhysicalFieldInfo* GetPhysicalFieldInfo(SingleDatabaseFileContext*, const std::string&, bool thr = true);
	PhysicalFieldInfo* GetPhysicalFieldInfo(SingleDatabaseFileContext*, FieldID);

	static PhysicalFieldInfo* GetAndValidatePFI(SingleDatabaseFileContext*, const std::string&, 
		bool, bool, bool, FieldAttributes* = NULL, DatabaseFileContext* = NULL);
	static void ConvertValue(SingleDatabaseFileContext*, PhysicalFieldInfo*, 
		const FieldValue&, FieldValue*, const FieldValue**, 
		bool, bool* = NULL, bool* = NULL);

	int Load(FastLoadRequest*, BB_OPDEVICE*);
	void Unload(FastUnloadRequest*);
};




//**************************************************************************************
//V3.0
class FastUnloadFieldInfoParallelSubTask : public util::WorkerThreadSubTask {
	FastUnloadRequest* request;

	void Perform();
public:
	FastUnloadFieldInfoParallelSubTask(FastUnloadRequest* r)
		: WorkerThreadSubTask("Field definitions"), request(r) {}
};


} //close namespace

#endif
