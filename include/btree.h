
#if !defined(BB_BTREE)
#define BB_BTREE

#include <vector>

#include "inverted.h"
#include "buffhandle.h"
#include "fieldval.h"
#include "infostructs.h"

namespace dpt {

struct PhysicalFieldInfo;
class SingleDatabaseFileContext;
class DatabaseServices;
class DatabaseFileTableDManager;
class DatabaseFile;
class FieldValue;
class BitMappedFileRecordSet;
class BTreePage;
class BTreeExtract;
struct DU1FlushStats;

//See tech docs for analysis of why this is enough in all realistic cases.
const int MAX_TREE_DEPTH = 4; 

//************************************************************************************
class BTreeAPI {
	friend class BTreeExtract;

	DatabaseServices* dbapi;
	DatabaseFileTableDManager* tdmgr;
	DatabaseFile* file;

	FieldValue last_value_located;
	bool last_locate_successful;

	//----------------------------------------
	bool AtLeaf() {return leaf_level != -1;}

	bool LocateValueEntry_Part1(bool = false);
	void LocateValueEntry_Part2(const FieldValue*, bool = false);

	bool InsertBranchPointer(short, const FieldValue&, const int);
	void MakeNewRootDuringSplit(BTreePage&);
	void CreateNewSoloRoot();
	void RemoveBranchPointer(short);

	//Diagnostics
	short Depth() {if (!AtLeaf()) return 0; return leaf_level+1;}
	int CurrentLeafPageNum() {if (!AtLeaf()) return -1; return buffpagenum[leaf_level];}
	double CurrentLeafFree();
	int BranchAnalysis(double&, bool);
	int BranchAnalysis_1Node(int, double&, bool);

	static void Analyze3_1Level
		(SingleDatabaseFileContext*, BB_OPDEVICE*, bool, bool, int, std::vector<int>*);

	void DeleteAllNodes_1Level(std::vector<int>*);

protected:
	PhysicalFieldInfo* pfi;
	SingleDatabaseFileContext* context;

	BufferPageHandle buffpage[MAX_TREE_DEPTH];
	int buffpagenum[MAX_TREE_DEPTH];
	short leaf_level;
	short leaf_value_offset;
	short leaf_ilmr_offset;

	void InitializeCache();

public:
	BTreeAPI() : pfi(NULL), context(NULL) {}
	virtual ~BTreeAPI() {}

	virtual void Initialize(SingleDatabaseFileContext*, PhysicalFieldInfo*);
	BTreeAPI(SingleDatabaseFileContext* c, PhysicalFieldInfo* p) {Initialize(c, p);}

	//V2.14 Jan 09.  See derived class.
	virtual bool LocateValueEntry(const FieldValue&, bool = false, bool = false);
	bool LocateLowestValueEntryPreBrowse();
	bool LocateHighestValueEntryPreBrowse();

	bool LastLocateSuccessful() {return last_locate_successful;}
	void GetLastValueLocated(FieldValue& v) {v = last_value_located;}
	FieldValue* GetLastValueLocatedPtr() {return &last_value_located;}

	bool WalkToNextValueEntry();
	bool WalkToPreviousValueEntry();
	bool DVCReposition(int, CursorDirection);
	int GetLeafTStamp();

	void InsertValueEntry(const FieldValue&, bool = false);
	void RemoveValueEntry();
	void DeleteAllNodes(); //V2.19

	InvertedListAPI InvertedList();

	bool FieldIsOrdNum();

	void Analyze1(BTreeAnalyzeInfo*, InvertedListAnalyze1Info*, 
		SingleDatabaseFileContext*, PhysicalFieldInfo*, bool);
	static void Analyze3(SingleDatabaseFileContext*, BB_OPDEVICE*, 
		PhysicalFieldInfo*, bool, bool);

	//V2.06 Jul 07.  For use by DVC.
	BTreeAPI* CreateClone();
};

//************************************************************************************
//V2.14 Jan 09.  For faster build if done in strict order and we have CFR_INDEX in EXCL.
class BTreeAPI_Load : public BTreeAPI {
	FieldValue prev_value;
	enum {NOT_INIT, IN_ORDER, UNORDERED} status;
	DU1FlushStats* stats;

public:
	BTreeAPI_Load(DU1FlushStats* s = NULL) : BTreeAPI(), stats(s) {}
	void Initialize(SingleDatabaseFileContext*, PhysicalFieldInfo*);

	struct DU1FlushStats* DUFlushStats() {return stats;}

	bool LocateValueEntry(const FieldValue&, bool = false, bool = false);

	void BuildFromExtract(BTreeExtract&);
};

//************************************************************************************
//V2.19 June 09.  Used when changing btree collating order
class BTreeExtract {
	bool numeric;
public:

	struct Entry {
		FieldValue val;
		int idata;
		short sdata;

		Entry() {} 
		Entry(const Entry& i) : val(i.val), idata(i.idata), sdata(i.sdata) {} 
		Entry(const FieldValue& v, int i, short s) {val = v; idata = i; sdata = s;} 

		bool operator<(const Entry& rhs) const {return val.Compare(rhs.val) < 0;}
	};
	std::vector<Entry> data;

	BTreeExtract(SingleDatabaseFileContext*, PhysicalFieldInfo*);
	void ConvertAndSort(bool);
	bool IsNumeric() {return numeric;}
};







} //close namespace

#endif
