
//Calling code gets one of these back as the result of a btree search.  
//It hides the different inverted list physical representations such as bitmaps.

#if !defined(BB_INVERTED)
#define BB_INVERTED

#include "fastunload.h"
#include "buffhandle.h"
#include "limits.h"
#include "infostructs.h"
#include <vector>

namespace dpt {

class BitMappedFileRecordSet;
class SingleDatabaseFileContext;
class DatabaseServices;
class DatabaseFileTableDManager;
class DatabaseFileIndexManager;
class DatabaseFile;
class FastUnloadRequest;
struct SegmentRIB;
struct DU1FlushStats;

//*****************************************************************************
const short INDEX_INVALID = -1;
const short INDEX_UNIQUE = -2;

const short SEG_INVALID = -1;
const short SEG_UNIQUE = -2;
const short SEG_SEMI_UNIQUE = -3;
const short SEG_BITMAP = -4;

//*****************************************************************************
class InvertedListAPI {
	SingleDatabaseFileContext* context;

	//Derivable from context but for nicer code
	DatabaseServices* dbapi;
	DatabaseFileTableDManager* tdmgr;
	DatabaseFileIndexManager* ixmgr;
	DatabaseFile* file;

	//------------------------------------
	//Access to the btree leaf or whatever
	BufferPageHandle* index_valarea_buffpage;
	short index_value_page_offset;

	mutable short ilmr_page_slot;
	mutable int ilmr_page_number;
	void SetILMRInfo(int, short);

	short GetILMRControlValue() const {return ilmr_page_slot;}
	int UniqueValRecNum() const {return ilmr_page_number;}

	//------------------------------------
	//Access to the master record if there is one
	mutable BufferPageHandle ilmr_buffpage;
	mutable short ilmr_page_offset;
	mutable short num_segribs;
	void CacheMasterRecordPage(BufferPageHandle* = NULL) const;
	
	void GetSegRIBInfo(short, SegmentRIB&) const;
	void SetSegRIBInfo(short, const SegmentRIB&);
	void InsertSegRIB(short, const SegmentRIB&, bool, bool = false);
	void RemoveSegRIB(short, bool = false);

	bool Iterate(BitMappedFileRecordSet**, InvertedListAnalyze1Info*, bool, 
		const BitMappedFileRecordSet*, std::vector<short>* = NULL);
	void CreateOutputSetNowIfNotDoneYet(
		SingleDatabaseFileContext*, BitMappedFileRecordSet**, bool&);

	void Copy(const InvertedListAPI& from);

	//V2.14 Jan 09.  Shared code
	void InsertSegRIBs(BitMappedFileRecordSet*, bool, DU1FlushStats*);
	short AugmentFindRibixForSeg(short, short);

public:
	InvertedListAPI(SingleDatabaseFileContext* c, BufferPageHandle* b, short o);

	//So we can return one from the index function
	InvertedListAPI(const InvertedListAPI& from) {Copy(from);}
	InvertedListAPI& operator=(InvertedListAPI& rhs) {Copy(rhs); return *this;}

	bool IndexValueValid() const {return GetILMRControlValue() != INDEX_INVALID;}
	bool UniqueValInFile() const {return GetILMRControlValue() == INDEX_UNIQUE;}

	BitMappedFileRecordSet* AssembleRecordSet(const BitMappedFileRecordSet*);
	bool AddRecord(int);
	bool RemoveRecord(int);

	//FILE RECORDS processing (replace set)
	bool ReplaceRecordSet(BitMappedFileRecordSet*, BitMappedFileRecordSet**);
	//V2.14 Jan 09.  Separate function now for loads - it's clearer
	void AugmentRecordSet(BitMappedFileRecordSet*, struct DU1FlushStats*);

	void Analyze(InvertedListAnalyze1Info* info) {Iterate(NULL, info, false, NULL);}
	//void Delete() {Iterate(NULL, NULL, true);}

	//V3.0.  Fast unload.
	int Unload(FastUnloadOutputFile*, const BitMappedFileRecordSet*, bool, bool);
};

//*****************************************************************************
struct SegmentRIB {
	short seg_num;
	short list_pageslot;
	int list_page;

	//--------
	SegmentRIB(short s = SEG_INVALID, short n = SEG_INVALID, int p = SEG_INVALID) 
		: seg_num(s), list_pageslot(n), list_page(p) {}
	SegmentRIB(short s, unsigned short urrn) 
		: seg_num(s), list_pageslot(SEG_UNIQUE) {
			SegUniqueValRelRecNumA() = urrn;
			SegUniqueValRelRecNumB() = USHRT_MAX;}

	inline short& ControlValue() {return list_pageslot;}

	bool IsInvalid() {return ControlValue() == SEG_INVALID;}
	bool IsBitMap() {return ControlValue() == SEG_BITMAP;}
	bool UniqueValInSegment() {return ControlValue() == SEG_UNIQUE;}
	bool SemiUniqueValInSegment() {return ControlValue() == SEG_SEMI_UNIQUE;}

	//Since if we hold segment relative numbers we can squeeze 2 in
	char* TwoSegRecNums() {return reinterpret_cast<char*>(&list_page);}
	unsigned short& SegUniqueValRelRecNumA() {
		return *(reinterpret_cast<unsigned short*>(TwoSegRecNums()));}
	unsigned short& SegUniqueValRelRecNumB() {
		return *(reinterpret_cast<unsigned short*>(TwoSegRecNums() + 2));}
};

} //close namespace

#endif
