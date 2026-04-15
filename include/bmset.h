/****************************************************************************************
Record sets as used in finds and lists (not sorted sets).  In other words those we
maintain as bitmaps.
****************************************************************************************/

#if !defined(BB_BMSET)
#define BB_BMSET

#include <map>
#include "frecset.h"

namespace dpt {

namespace util {class BitMap;}
class DatabaseFile;
class DatabaseServices;
class Record;
class ReadableRecord;
class BitMappedRecordSetCursor;
class RecordList;
class SortRecordsSpecification;
class SortRecordSet;
class DU1SeqOutputFile;

//**************************************************************************************
class BitMappedFileRecordSet : public FileRecordSet {

	SingleDatabaseFileContext* home_context;
	SegmentRecordSet* append_hi_segset;

public:
	BitMappedFileRecordSet(SingleDatabaseFileContext*);
	virtual ~BitMappedFileRecordSet() {Unlock(); DestroySetData();}

	BitMappedFileRecordSet* MakeCopy
		(BitMappedFileRecordSet* = NULL, short = 0, short = SHRT_MAX) const;

	void BitOr(const BitMappedFileRecordSet*, bool = false);
	//For use in single record list place/remove
	void BitOr(const ReadableRecord*);
	void BitOr(int);
	void FastAppend(int);

	//These are static because the lhs may get deleted or recreated
	static BitMappedFileRecordSet* BitAnd_S
					(BitMappedFileRecordSet*, const BitMappedFileRecordSet*, bool, bool = false);
	static BitMappedFileRecordSet* BitAnd_S
					(BitMappedFileRecordSet*, const ReadableRecord*, bool);
	static BitMappedFileRecordSet* BitAnd
					(BitMappedFileRecordSet* s1, const BitMappedFileRecordSet* s2) {
		return BitAnd_S(s1, s2, false);}
	static BitMappedFileRecordSet* BitAndNot
					(BitMappedFileRecordSet* s1, const BitMappedFileRecordSet* s2) {
		return BitAnd_S(s1, s2, true);}

	//-------------
	//Some special functions used internally mainly during finds
	static BitMappedFileRecordSet* SegmentAnd
					(BitMappedFileRecordSet* s1, const BitMappedFileRecordSet* s2) {
		return BitAnd_S(s1, s2, false, true);}
	static BitMappedFileRecordSet* Point$MaskOff(BitMappedFileRecordSet*, int, bool);
	void ClearButNoDelete();
	void TBSHandOverLock(BitMappedFileRecordSet*);
	void TBSDropSegmentLock(short);

	int RLCRec();
	int Count() const;
	int NumSegs() const {return data.size();}
	SingleDatabaseFileContext* HomeFileContext() const {return home_context;}

	void LockShr(DatabaseServices*);
	void LockExcl(DatabaseServices*);
	void Unlock();

	void AppendSegmentSet(SegmentRecordSet*);
	int SingleRecordNumber() const;
	bool ContainsAbsRecNum(int) const;
	bool ContainsSegment(short segnum) const {return (data.find(segnum) != data.end());}
	int HiRecNum() const;

	void Unload(DU1SeqOutputFile*);
};	

//**************************************************************************************
class BitMappedRecordSet : public RecordSet {

	void DestroySetData();

	void BitAnd_S(const BitMappedRecordSet*, bool);
	void BitAnd_S(const ReadableRecord*, bool);

	void ContextCheck(const BitMappedRecordSet*) const;
	int ContextCheck(SingleDatabaseFileContext*, const char*) const;

//	void NotifyOfDirtyDelete(BitMappedRecordSet*);

	friend class FindOperation;
	void TBSDropSegmentLock(short, RecordSetCursor*);

	void GetRecordNumberArray_D(int*, int);

protected:
	friend class RecordList;
	friend class BitMappedRecordSetCursor;
	friend class FastUnloadRequest;

	std::map<int, BitMappedFileRecordSet*> data;

	BitMappedRecordSet(DatabaseFileContext* c) : RecordSet(c) {}
	virtual ~BitMappedRecordSet() {DestroySetData();}

	void BitOr(const BitMappedRecordSet*);
	void BitOr(const ReadableRecord*);

	//These are *not* static because empty set objects at the user level are fine,
	//so the original object remains whatever records get taken out of it here.
	void BitAnd(const BitMappedRecordSet* s2) {BitAnd_S(s2, false);}
	void BitAndNot(const BitMappedRecordSet* s2) {BitAnd_S(s2, true);}
	void BitAnd(const ReadableRecord* r) {BitAnd_S(r, false);}
	void BitAndNot(const ReadableRecord* r) {BitAnd_S(r, true);}

	void ClearMember_B(SingleDatabaseFileContext*);

public:

	int Count() const;
	BitMappedFileRecordSet* GetFileSubSet(int) const;
	RecordSetCursor* OpenCursor(bool gotofirst = true);

	SortRecordSet* Sort(SortRecordsSpecification&);

	FindEnqueueType LockType();  //V2.25 handy for diagnostics
};

//***************************************************************************************
class BitMappedRecordSetCursor : public RecordSetCursor {

	std::map<int, BitMappedFileRecordSet*>::iterator fileset_bookmark;
	int fileset_ix;
	std::map<short, SegmentRecordSet*>::iterator segset_bookmark;
	short segset_ix;
	unsigned short relrecnum_ix;

	void GetInitialIxValues(bool);
	void GetInitialRecNumIx(bool);

	friend class BitMappedRecordSet;
	BitMappedRecordSetCursor(BitMappedRecordSet* s);
	~BitMappedRecordSetCursor() {}

	void SetAtEnd() {relrecnum_ix = USHRT_MAX; SetLARInfo(-1, NULL);}

	bool RegisterValidSetPosition(bool);
	void ClearSetPositionInfo() {SetLARInfo(-1, NULL);}

	void RequestReposition(int changetype) {
		if (changetype == 11)		//set cleared or released
			ClearSetPositionInfo();
		needs_reposition = true;}	//other changes such as REMOVE/PLACE

	bool RecoverPositionWithAdvance();
	bool Advance_1(bool);
	void AdvanceMain(int);

	BitMappedRecordSet* BMSet() {return static_cast<BitMappedRecordSet*>(Set());}
	void TBSReposition(std::map<short, SegmentRecordSet*>*);

public:
	void GotoFirst();
	void GotoLast();
	void Advance(int);

	ReadableRecord* AccessCurrentRecordForRead() {
		return (ReadableRecord*) AccessCurrentRealRecord();}

	RecordSetCursor* CreateClone();
	void ExportPosition(RecordSetCursor* dest);
};

} //close namespace

#endif



