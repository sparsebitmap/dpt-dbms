/****************************************************************************************
Record sets are made up of a collection of per-file sets, and within that a collection
of per-segment sets.
****************************************************************************************/

#if !defined(BB_FRECSET)
#define BB_FRECSET

#include <map>
#include "recset.h"
#include "page_v.h" //#include "page_V.h" : V2.24 case is less interchangeable on *NIX - Roger M.

namespace dpt {

namespace util {class BitMap;}
class DatabaseFile;
class Record;
class RecordLock;
class SegmentRecordSet_BitMap;
class SegmentRecordSet_SingleRecord;
class SegmentRecordSet_Multiple;
class SegmentRecordSet_Array;
class BitMapFilePage;

//**************************************************************************************
class SegmentRecordSet {
protected:
	short segnum;
	SegmentRecordSet(short s) : segnum(s) {}

public:
	short SegNum() const {return segnum;}
	virtual ~SegmentRecordSet() {}

	virtual SegmentRecordSet* MakeCopy(bool = false) const = 0;

	//These are static because the lhs may get deleted and/or recreated
	static SegmentRecordSet* BitOr		(SegmentRecordSet*, const SegmentRecordSet*);
	static SegmentRecordSet* BitAnd_S	(SegmentRecordSet*, const SegmentRecordSet*, bool);
	static SegmentRecordSet* BitAnd		(SegmentRecordSet* s1, const SegmentRecordSet* s2) {
											return BitAnd_S(s1, s2, false);}
	static SegmentRecordSet* BitAndNot	(SegmentRecordSet* s1, const SegmentRecordSet* s2) {
											return BitAnd_S(s1, s2, true);}

	virtual SegmentRecordSet_BitMap* CastToBitMap() {return NULL;}
	virtual SegmentRecordSet_SingleRecord* CastToSingle() {return NULL;}
	virtual SegmentRecordSet_Array* CastToArray() {return NULL;} //V2.14 Jan 09.
	virtual const SegmentRecordSet_BitMap* CCastToBitMap() const {return NULL;}
	virtual const SegmentRecordSet_SingleRecord* CCastToSingle() const {return NULL;}
	virtual const SegmentRecordSet_Array* CCastToArray() const {return NULL;}

	virtual unsigned short Count() const = 0;
	bool AnyIntersection(const SegmentRecordSet*) const;

	//These do not touch the input parm if no more records after it (false return code)
	virtual bool FindNextRelRec(unsigned short&) const {return false;}
	virtual bool FindPrevRelRec(unsigned short&) const {return false;}
	virtual bool ContainsRelRecNum(unsigned short s) const = 0;
};

//**************************************************************************************
//V2.14 Jan 09.  This added to speed the path through a one-step file load by building
//arrays and passing them directly into table D without going via bitmap form.
//NB.  Only used in this situation.  Array form is not efficient to AND/OR, so sets
//are bitmapized for general use.
//**************************************************************************************
class SegmentRecordSet_Array : public SegmentRecordSet {
	mutable ILArray array;

public:
	SegmentRecordSet_Array(const short, unsigned short); //create memory here
	SegmentRecordSet_Array(const short, unsigned short*, void*); //adopt other memory
	~SegmentRecordSet_Array() {}

	SegmentRecordSet_Array* CastToArray() {return this;}
	const SegmentRecordSet_Array* CCastToArray() const {return this;}

	//We only need the functions used when populating table D from a set
	unsigned short Count() const {return array.NumEntries();}
	SegmentRecordSet* MakeCopy(bool = false) const;

	const ILArray& CArray() const {return array;}
	ILArray& Array() {return array;}

	bool ContainsRelRecNum(unsigned short) const {throw "bug if you see this SRS_A";}
};

//**************************************************************************************
class SegmentRecordSet_BitMap : public SegmentRecordSet {

	util::BitMap* data;
	BitMapFilePage* filepage;
	mutable bool own_data;

public:
	SegmentRecordSet_BitMap(short s);
	SegmentRecordSet_BitMap(short, BitMapFilePage&); //handy for ebm etc
	SegmentRecordSet_BitMap(short, util::BitMap*);
	~SegmentRecordSet_BitMap();

	SegmentRecordSet_BitMap* CastToBitMap() {return this;}
	const SegmentRecordSet_BitMap* CCastToBitMap() const {return this;}

	util::BitMap* Data() {return data;}
	const util::BitMap* CData() const {return data;}

	SegmentRecordSet* MakeCopy(bool = false) const;

	void RecOr(const SegmentRecordSet_Array*); //V2.14 Jan 09.

	unsigned short Count() const;
	bool AnyIntersection(const SegmentRecordSet_BitMap*) const;

	bool ContainsRelRecNum(unsigned short) const;
	bool FindNextRelRec(unsigned short&) const;
	bool FindPrevRelRec(unsigned short&) const;
};

//**************************************************************************************
class SegmentRecordSet_SingleRecord : public SegmentRecordSet {
	unsigned short relrecnum;

public:
	SegmentRecordSet_SingleRecord(int rn) 
		: SegmentRecordSet(SegNumFromAbsRecNum(rn)) {
			relrecnum = RelRecNumFromAbsRecNum(rn, segnum);} 
	SegmentRecordSet_SingleRecord(short sn, unsigned short rr)
		: SegmentRecordSet(sn), relrecnum(rr) {}

	SegmentRecordSet_SingleRecord* CastToSingle() {return this;}
	const SegmentRecordSet_SingleRecord* CCastToSingle() const {return this;}

	SegmentRecordSet* MakeCopy(bool = false) const {
		return new SegmentRecordSet_SingleRecord(segnum, relrecnum);}

	unsigned short RelRecNum() const {return relrecnum;} 
	int AbsRecNum() const {return AbsRecNumFromRelRecNum(relrecnum, segnum);}
	unsigned short Count() const {return 1;}

	bool IsSameRelRecNum(const SegmentRecordSet_SingleRecord* s) const {
		return (relrecnum == s->relrecnum);}
	bool ContainsRelRecNum(unsigned short s) const {return (relrecnum == s);}
};







//**************************************************************************************
class FileRecordSet {

protected:
	DatabaseFile* file;
	RecordLock* lock;
	std::map<short, SegmentRecordSet*> data;

	FileRecordSet(DatabaseFile* f) : file(f), lock(NULL) {}
	void DestroySetData();

	//Generally these objects only exist as part of RecordSets, and are removed when
	//empty.  However in some internal situations we can get empty ones.
	friend class InvertedListAPI;			//during FILE RECORDS
	friend class DatabaseFileIndexManager;	//during FIND
	virtual int SingleRecordNumber() const = 0;

public:
	virtual ~FileRecordSet() {}

	std::map<short, SegmentRecordSet*>* Data() {return &data;}
	SegmentRecordSet*  GetSegmentSubSet(short) const;
	bool IsEmpty() const {return (data.size() == 0);}

	virtual void Unlock() = 0;
	virtual bool AnyIntersection(const FileRecordSet*) const;
	virtual int RLCRec() = 0;

	FindEnqueueType LockType();  //V2.25 handy for diagnostics

	void GetRecordNumberArray(int*, int getmaxrecs); //V3.0
};

} //close namespace

#endif
