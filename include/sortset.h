/****************************************************************************************
Sets that are held as lists of record numbers rather than bitmaps.  The purpose of this
is to allow sorting of a RecordSet and then use of a cursor on it just like a FoundSet.
****************************************************************************************/

#if !defined(BB_SORTSET)
#define BB_SORTSET

#include "recset.h"
#include "sortrec.h"
#include <vector>

namespace dpt {

class SortRecordSetCursor;
class SortRecord;
class SortRecordsSpecification;

//For std::sort by pointer (which would normally just sort into address order)
struct SortRecordPtr {
	SortRecord* rec;
	SortRecordPtr(SortRecord* r) : rec(r) {}
	bool operator<(const SortRecordPtr& rhs) const {return (*rec < *(rhs.rec));}
};

//***************************************************************************************
class SortRecordSet : public RecordSet {

	friend class SortRecordSetCursor;
	std::vector<SortRecordPtr> ptrs; 
	std::vector<SortRecord*> data;
	bool pass_through_flag; //for UL-style key-only sort

	void DestroySetData();

	friend class DatabaseFileContext;
	SortRecordSet(DatabaseFileContext* c, bool p) : RecordSet(c), pass_through_flag(p) {}
	~SortRecordSet() {DestroySetData();}

	friend class BitMappedRecordSet;
	void ReserveMemory(int n) {data.reserve(n); ptrs.reserve(n);}
	void AppendRec(SortRecord* r);
	void GenerateAndAppendEachKeyRotations(SortRecord*, SortRecordsSpecification*, 
		std::vector<int>*, std::vector<std::vector<FieldValue> >*, int = 0);
	void PerformSort();

	void GetRecordNumberArray_D(int*, int);

public:
	int Count() const {return data.size();}

	bool IsSortSet() {return true;} //V2.25 helps clarify the C API 
	SortRecord* AccessRandomSortRecord(int set_index); //V3.0.  Similar function to AccessRandomRecord()

	RecordSetCursor* OpenCursor(bool gotofirst = true);
};

//***************************************************************************************
class SortRecordSetCursor : public RecordSetCursor {
	int ix;

	friend class SortRecordSet;
	SortRecordSetCursor(SortRecordSet* s) : RecordSetCursor(s), ix(-1) {}
	~SortRecordSetCursor() {}

	bool InData() {return (ix >= 0 && ix < Set()->Count());}
	void ValidateAdvancedPosition();

	SortRecordSet* SortSet() {return static_cast<SortRecordSet*>(Set());}

public:
	//See BMSet comment
	void RequestReposition(int changetype) {
		if (changetype == 11) { 
			SetLARInfo(-1, NULL); ix = -1;}}

	void GotoFirst();
	void GotoLast();
	void Advance(int n);

	ReadableRecord* AccessCurrentRecordForRead();

	RecordSetCursor* CreateClone();
	void ExportPosition(RecordSetCursor* dest);
};

} //close namespace

#endif
