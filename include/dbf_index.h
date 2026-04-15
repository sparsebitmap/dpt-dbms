/*******************************************************************************************
A sub-class largely just to keep this stuff from clogging up the DatabaseFile code.
This class handles all the btree and inverted list work.
*******************************************************************************************/
#if !defined(BB_DBF_INDEX)
#define BB_DBF_INDEX

#include <string>
#include <vector>
#include "atom.h"
#include "atomback.h"
#include "apiconst.h"
#include "fieldval.h"
#include "infostructs.h"
#include "bbthread.h"

namespace dpt {

class DatabaseFile;
class DatabaseServices;
struct PhysicalFieldInfo;
class FieldValue;
class Record;
class BitMappedRecordSet;
class BitMappedFileRecordSet;
class SingleDatabaseFileContext;
class ValueSet;
class FoundSet;
class FindSpecification;
class DirectValueCursor;
class BTreeAPI_Load;
class FindValuesSpecification;
class CFRSentry;
class FastUnloadRequest;
class FastLoadRequest;
class FastLoadInputFile;
class FindWorkInfo;
class FindWorkNode_Leaf;
namespace util {
	class LineOutput;
}
typedef short FieldID;

//*****************************************************************************************
class DatabaseFileIndexManager {
	DatabaseFile* file;

	Lockable parallel_load_lock; //V3.0. See comments in cpp file.

	void LoadEyeball(FastLoadRequest*, BB_OPDEVICE*);

public:
	DatabaseFileIndexManager(DatabaseFile* f) : file(f) {}
	std::string ViewParm(DatabaseServices*, const std::string&);
	void ResetParm(SingleDatabaseFileContext*, const std::string&, int);
	void CacheParms();

	//Reading functions
	void FindRecords(int, SingleDatabaseFileContext*, FoundSet*, 
		const FindSpecification&, const FindEnqueueType&, const BitMappedFileRecordSet*);

	unsigned int FindOrCountValues
		(SingleDatabaseFileContext*, ValueSet*, const FindValuesSpecification&);

	//Updating functions
	bool Atom_AddValRec
		(PhysicalFieldInfo*, const FieldValue&, SingleDatabaseFileContext*, int, BTreeAPI_Load* = NULL);
	bool Atom_RemoveValRec
		(PhysicalFieldInfo*, const FieldValue&, SingleDatabaseFileContext*, int);

	//FILE RECORDS processing (replace set)
	bool Atom_ReplaceValRecSet(PhysicalFieldInfo*, const FieldValue&, 
		SingleDatabaseFileContext*, BitMappedFileRecordSet*, BitMappedFileRecordSet**);
	//V2.14 Jan 09.  Separate function now for deferred updates - it's clearer
	void Atom_AugmentValRecSet(PhysicalFieldInfo*, const FieldValue&, 
		SingleDatabaseFileContext*, BitMappedFileRecordSet*, BTreeAPI_Load*);

	//See comments in dbctxt
	void Analyze1(BTreeAnalyzeInfo*, InvertedListAnalyze1Info*, 
		SingleDatabaseFileContext*, const std::string&, bool);
	void Analyze2(InvertedListAnalyze2Info*, SingleDatabaseFileContext*);
	void Analyze3(SingleDatabaseFileContext*, BB_OPDEVICE*, const std::string&, bool, bool);

	//DBA stuff
	void Unload(FastUnloadRequest*, PhysicalFieldInfo*);
	int Load(FastLoadRequest*, BB_OPDEVICE*);

	void DeleteFieldIndex(SingleDatabaseFileContext*, PhysicalFieldInfo*);
	void CreateIndexFromData(SingleDatabaseFileContext*, PhysicalFieldInfo*, bool);
	void ChangeIndexType(SingleDatabaseFileContext*, PhysicalFieldInfo*);
};








//**************************************************************************************
class AtomicUpdate_AddIndexValRec : public AtomicUpdate {
	Record* record;
	PhysicalFieldInfo* fieldinfo;
	const FieldValue& fieldval;
	bool file_registered;

public:
	AtomicUpdate_AddIndexValRec(Record* r, PhysicalFieldInfo*, const FieldValue&, bool = false);
	AtomicBackout* CreateCompensatingUpdate();
	bool Perform();
};

//*********TBO*********
class AtomicBackout_DeAddIndexValRec : public AtomicBackout {
	int recnum;
	PhysicalFieldInfo* fieldinfo;
	FieldValue fieldval;

public:
	AtomicBackout_DeAddIndexValRec(Record* r, PhysicalFieldInfo*, const FieldValue&); 
	void Perform();
};






//**************************************************************************************
class AtomicUpdate_RemoveIndexValRec : public AtomicUpdate {
	Record* record;
	PhysicalFieldInfo* fieldinfo;
	const FieldValue& fieldval;

public:
	AtomicUpdate_RemoveIndexValRec(Record* r, PhysicalFieldInfo*, const FieldValue&);
	AtomicBackout* CreateCompensatingUpdate();
	bool Perform();
};

//*********TBO*********
class AtomicBackout_DeRemoveIndexValRec : public AtomicBackout {
	int recnum;
	PhysicalFieldInfo* fieldinfo;
	FieldValue fieldval;

public:
	AtomicBackout_DeRemoveIndexValRec(Record* r, PhysicalFieldInfo*, const FieldValue&); 
	void Perform();
};






//**************************************************************************************
class AtomicUpdate_FileRecords : public AtomicUpdate {
	BitMappedFileRecordSet* newset;
	PhysicalFieldInfo* fieldinfo;
	const FieldValue& fieldval;

public:
	AtomicUpdate_FileRecords(SingleDatabaseFileContext*, 
		BitMappedFileRecordSet*, PhysicalFieldInfo*, const FieldValue&);
	AtomicBackout* CreateCompensatingUpdate();
	void Perform();
};

//*********TBO*********
class AtomicBackout_UnFileRecords : public AtomicBackout {
	BitMappedFileRecordSet* oldset;
	PhysicalFieldInfo* fieldinfo;
	FieldValue fieldval;

public:
	AtomicBackout_UnFileRecords(SingleDatabaseFileContext*, 
		PhysicalFieldInfo*, const FieldValue&);
	~AtomicBackout_UnFileRecords();

	void NoteOldSet(BitMappedFileRecordSet* s) {oldset = s;}
	void Perform();
};



//**************************************************************************************
//V3.0
class FastUnloadIndexParallelSubTask : public util::WorkerThreadSubTask {
	FastUnloadRequest* request;
	PhysicalFieldInfo* pfi;

	void Perform();
public:
	FastUnloadIndexParallelSubTask(FastUnloadRequest*, PhysicalFieldInfo*); 
};

} //close namespace

#endif
