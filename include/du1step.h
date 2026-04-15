/*******************************************************************************************
Stuff to support the V2.14 1-step load mode.  That is, the index updates get
deferred but instead of writing each f/V pair out to a sequential file it is held and 
sorted in memory.  The values are held in an ordered tree, effectively performing
a sort on them and removing the need for external sorting.  The inverted lists are held
in the same format as they will be in table D, which speeds up their eventual
storage there.
If memory fills before the file is closed, there is a choice of how to handle the sorted
"chunk" of updates for each field.  They can be written out to sequential files and later 
merged, or written directly to table D, avoiding the sequential I/O but meaning later
chunks will have the overhead of adding to the table D info.  Just two different kinds of 
merge really.
*******************************************************************************************/
#if !defined(BB_DU1STEP)
#define BB_DU1STEP

#include <map>

#include "bitmap3.h"
#include "lockable.h"
#include "fieldval.h"
#include "fieldinfo.h"
#include "page_v.h" //#include "page_V.h" : V2.24 case is less interchangeable on *NIX - Roger M.
#include "merge.h"
#include "iowrappers.h"
#include "stlextra.h"
#include "windows.h" //V2.23 for heap HANDLEs
#include "bbthread.h" //V3.0 final merge phase is multithreaded now

namespace dpt {

namespace util {class BitMap;}

class CoreServices;
class StatViewer;
class DatabaseServices;
class DatabaseFile;
class DatabaseFileIndexManager;
class BitMappedFileRecordSet;
class SegmentRecordSet;
class CriticalFileResource;

//V2.17 Moved these out of the enclosing class below.  Later on some will get 
//their own header for use in fast reorgs etc.  The name DU1 will then change.
class DU1MergeStream;
class DU1MergeStream_File;
class DU1MergeStream_Mem;
class DU1SeqOutputFile;

struct DU1FlushStats;
class LoadDiagnostics;

class DU1FieldIndex;    //one per field
class DU1InvList;       //one per field value pair
class DU1SegInvList;    //one per segment of a FVP

//V3.0
class FastLoadRequest;
class FastLoadInputFile;

//******************************************************************************************
//One of these exists per file that's in 1-step DU mode
class DeferredUpdate1StepInfo {

	//************************************************
	//Everything for all values for all fields in the file
	std::vector<DU1FieldIndex> info;

	Lockable data_lock;
	DatabaseFile* file;
	CriticalFileResource* cfr_index;

	int max_memory_pct;
	int max_memory_size;
	int memory_hwm;
	int total_physical_memory;

	int partial_flushes;
	_int64 chunk_fvpairs;
	_int64 allchunks_fvpairs;
	int callcount;
	double total_time_taken;
	int fvps_per_chunk;
	bool merge_required;
	int loadctl_flags;

	LoadDiagnostics* diags;
	std::string ddname;

	HANDLE h_arrayheap;
	HANDLE h_bitmapheap;
	static void DestroyLocalHeaps(HANDLE*, HANDLE*);
	void CreateLocalHeaps();

	~DeferredUpdate1StepInfo();

public:
	DeferredUpdate1StepInfo(DatabaseFile*, const std::string&, 
							CriticalFileResource*, int mempct, int diags_level);
	static void DestroyObject(DeferredUpdate1StepInfo*);

	const std::string DD() {return ddname;}
	LoadDiagnostics* Diags() {return diags;}

	bool IsInitialized() {return info.size() != 0;}
	void Initialize(SingleDatabaseFileContext*);
	void InitializeForSingleField(PhysicalFieldInfo*);

	bool AddEntry(SingleDatabaseFileContext*, PhysicalFieldInfo*, const FieldValue&, int);
	void AttachFastLoadTapeI(SingleDatabaseFileContext*, PhysicalFieldInfo*, FastLoadInputFile*);

	enum FlushMode {MEMFULL, CLOSING, USER, REDEFINE, REORG};
	int Flush(SingleDatabaseFileContext*, FlushMode);

	bool AnythingToFlush() {return ( (chunk_fvpairs + callcount) > 0);}

	int MaxMemPct() {return max_memory_pct;}
	int MaxMemSize() {return max_memory_size;}

	//V2.23.  These now used where the global variables were used before.
	HANDLE* AHeap() {return &h_arrayheap;}
	HANDLE* BHeap() {return &h_bitmapheap;}
};



//**************************************************************************************
//Utility classes
//**************************************************************************************
struct DU1FlushStats {
	int unique_f;
	int seglists;
	int unique_s1;
	int unique_s2;
	int arrays;
	int bmaps;
	DU1FlushStats() 
		: unique_f(0), seglists(0), unique_s1(0), unique_s2(0), arrays(0), bmaps(0) {}
	void PrepMsg(std::string&, int);
};

//**************************************************************************************
class DU1SegInvList {
	
	//Trying to be as compact as possible at this level.
	union Ptr {                      //Same structure packing as in table D:
		util::BitMap* bmap;          //Bitmap form if lots of recs
		unsigned short* array;       //Array form if not so many
		struct {                     //Single record numbers if 1 or 2
			unsigned short rn1;
			unsigned short rn2;
		} recnums;
		_int32 allbits;              //generic
	} data;
	short segnum;
	mutable short rectype;

	static const short NONE;
	static const short UNIQUE1;
	static const short UNIQUE2;
	static const short ARRAY;
	static const short BITMAP;
	
public:
	DU1SegInvList() : segnum(-1), rectype(NONE) {data.allbits = 0x00000000;}
	void Initialize(short s, unsigned short r) {
		segnum = s; rectype = UNIQUE1; data.recnums.rn1 = r;}

	void operator=(const DU1SegInvList& l) {segnum = l.segnum; rectype = l.rectype; 
								data.allbits = l.data.allbits; l.rectype = NONE;}
	DU1SegInvList(const DU1SegInvList& l) {*this = l;}

	void Clear(DeferredUpdate1StepInfo*);

	short SegNum() {return segnum;}

	void AddRecord(unsigned short, DeferredUpdate1StepInfo*);

	void FlushData(BitMappedFileRecordSet*, DU1SeqOutputFile*, 
					DU1FlushStats*, DeferredUpdate1StepInfo*);
};

//**************************************************************************************
class DU1InvList {

	//Decided to go to the trouble of avoiding std::<vector> here to save space
	//per object, and enable special packing for unique values.
	union {
		DU1SegInvList* seginfos;
		int unique_recnum;
	} info;
	unsigned short allocsegs;
	unsigned short numsegs;

	void DestroySegInfo(DeferredUpdate1StepInfo* du1) {if (!allocsegs) return;
		for (int x = 0; x < numsegs; x++) info.seginfos[x].Clear(du1);
		delete[] info.seginfos;}

	DU1SegInvList& SegList(int x) {return info.seginfos[x];}
	DU1SegInvList& InfoBack() {return info.seginfos[numsegs-1];}

	~DU1InvList() {}

public:
	DU1InvList() : allocsegs(0), numsegs(0) {info.unique_recnum = -1;}
	static void DestroyObject(DU1InvList* o, DeferredUpdate1StepInfo* du1) {
		o->DestroySegInfo(du1), delete o;}

	void AddRecord(int recnum, DeferredUpdate1StepInfo*);

	unsigned short NumSegs() {return numsegs;}
	int URN() {return info.unique_recnum;}

	void FlushData(BitMappedFileRecordSet*, DU1SeqOutputFile*, 
					DU1FlushStats*, DeferredUpdate1StepInfo*);
};

//**************************************************************************************
//Much the same as the MergeStream class.  Should .. er .. merge the two classes really.
class DU1SeqOutputFile : public util::BBStdioFile {
	int vallen;
	unsigned char value_buffer[262];
	RoundedDouble& NumVal() {return *(reinterpret_cast<RoundedDouble*>(value_buffer));}
	unsigned short& NumSegs() {return *(reinterpret_cast<unsigned short*>(value_buffer+vallen));}
	int& URN() {return *(reinterpret_cast<int*>(value_buffer+vallen+2));}

	char seg_header_buffer[4];
	short& SegNum() {return *(reinterpret_cast<short*>(seg_header_buffer));}
	unsigned short& SegNRecs() {return*(reinterpret_cast<unsigned short*>(seg_header_buffer+2));}

public:
	void SetFieldValue(const RoundedDouble& d) {NumVal() = d; vallen = 8;}
	void SetFieldValue(const std::string&);
	void SetFieldValue(const FieldValue*);
	void SetNumSegs(unsigned short ns) {NumSegs() = ns;}

	//Value write is delayed so for unique vals we can do it all in one stdio write call
	void WriteURN(int urn) {URN() = urn; WriteValueHeader(true);}
	void WriteValueHeader(bool urn) {Write(value_buffer, vallen + ((urn) ? 6 : 2) );}

	void WriteSegInvList(short, unsigned short, void*);
};

//**************************************************************************************
class DU1FieldIndex {

	PhysicalFieldInfo* pfi;
	LoadDiagnostics* diags;

	DeferredUpdate1StepInfo* du1;

	//Control info for fields using the seqout/merge scheme
	DU1SeqOutputFile current_seq_file;
	std::vector<std::string> seq_file_names;
	std::vector<util::MergeRecordStream*> merge_streams;
	std::string merge_final_chunk_stats_msg;
	FastLoadInputFile* fastload_tapei; //V3.0

	//Use lower level objects than FieldValue as map keys for speed & size
	mutable std::map<RoundedDouble, DU1InvList*>* numinfo;
	mutable std::map<std::string, DU1InvList*>* strinfo;

	//ORD NUM fields - key is a double (well actually our checked/wrapped version)
	std::map<RoundedDouble, DU1InvList*>::iterator numiter;
	void BeginNumIter() {numiter = numinfo->begin();} 
	bool NumIterEnded() {return numiter == numinfo->end();}
	void AdvanceNumIter() {numiter++;}
	DU1InvList* ListAtNumIter() {return numiter->second;}

	//ORD CHAR fields - key is a std::string
	std::map<std::string, DU1InvList*>::iterator striter;
	void BeginStrIter() {striter = strinfo->begin();} 
	bool StrIterEnded() {return striter == strinfo->end();}
	void AdvanceStrIter() {striter++;} 
	DU1InvList* ListAtStrIter() {return striter->second;}

	void OpenNewCurrentSeqFile(int = -1);
	void PreMerge(SingleDatabaseFileContext*);
	void CloseMergeInputStreams();
	void DeleteMergeSequentialFiles(int = -1);

	void Clear();
	void Destroy();

public:
	DU1FieldIndex() : pfi(NULL), fastload_tapei(NULL), numinfo(NULL), strinfo(NULL) {}
	DU1FieldIndex(const DU1FieldIndex&);
	void Initialize(PhysicalFieldInfo*, LoadDiagnostics*, DeferredUpdate1StepInfo*);
	~DU1FieldIndex() {Destroy();}

	const std::string& FieldName() {return pfi->name;}
	DeferredUpdate1StepInfo* DU1() {return du1;}

	DU1InvList* FindOrInsertInvList(const FieldValue& val);
	void AttachFastLoadTapeI(FastLoadInputFile* t) {fastload_tapei = t;}

	std::map<RoundedDouble, DU1InvList*>* NumInfo() {return numinfo;}
	std::map<std::string, DU1InvList*>* StrInfo() {return strinfo;}

	void BeginIterator() {if (numinfo) BeginNumIter(); else BeginStrIter();}
	bool IteratorEnded() {if (numinfo) return NumIterEnded(); return StrIterEnded();}
	void AdvanceIterator() {if (numinfo) AdvanceNumIter(); else AdvanceStrIter();}

	const RoundedDouble& ValAtNumIter() {return numiter->first;}
	const std::string& ValAtStrIter() {return striter->first;}
	FieldValue ValueAtIterator() {if (numinfo) return ValAtNumIter(); return ValAtStrIter();}
	DU1InvList* ListAtIterator() {if (numinfo) return ListAtNumIter(); return ListAtStrIter();}

	int NumValues() {return (numinfo) ? numinfo->size() : (strinfo) ? strinfo->size() : 0;}
	int NumSeqFiles() {return seq_file_names.size();}
	FastLoadInputFile* TapeI() {return fastload_tapei;}
	bool IsNoMerge() {return pfi->atts.IsNoMerge();}

	std::string FlushData(SingleDatabaseFileContext*, DatabaseFileIndexManager*, bool);
	std::string Merge(SingleDatabaseFileContext*, DatabaseFileIndexManager*);
	const std::string& MergeFinalChunkMsg() {return merge_final_chunk_stats_msg;}
};

//**************************************************************************************
class DU1MergeStream : public util::MergeRecordStream {

	bool field_is_num;

	//These are cached as comparisons are much more frequent than extractions
	unsigned _int8 cachelen;
	const char* cachestr;
	const RoundedDouble* cachenum;

	bool CacheLTs(const DU1MergeStream* r) const {
		return VarLenMemCmp::LT(cachestr, cachelen, r->cachestr, r->cachelen);}
	bool CacheLTn(const DU1MergeStream* r) const {
		return (*cachenum < *(r->cachenum));}

protected:
	void SetCacheStr(const char* c, const unsigned _int8 l) {cachestr = c; cachelen = l;}
	void SetCacheNum(const RoundedDouble* d) {cachenum = d;}

public:
	DU1MergeStream(bool n) : field_is_num(n), cachestr(NULL), cachenum(NULL) {}
	virtual ~DU1MergeStream() {}

	bool FieldIsNum() {return field_is_num;}

	bool LowerKeyThan(const util::MergeRecordStream& mrs) const  {if (field_is_num) 
		return CacheLTn(static_cast<const DU1MergeStream*>(&mrs)); 
		return CacheLTs(static_cast<const DU1MergeStream*>(&mrs));}

	FieldValue MakeKeyFieldValue() {if (field_is_num) 
		return *cachenum; 
		return FieldValue(cachestr, cachelen);}

	bool DifferentKeyTo(const FieldValue& fv) {if (field_is_num)
		return (*cachenum != *(fv.RDData()));
		return VarLenMemCmp::NE(cachestr, cachelen, fv.StrChars(), fv.StrLen());}

	virtual void MergeChunkSet(BitMappedFileRecordSet*) = 0;
};

//**************************************************************************************
class DU1MergeStream_File : public DU1MergeStream {

	int fhandle;
	const char* fname;
	bool ReadFile(void*, int, bool);

	//Field value plus another 6 bytes all read in the first "bite"
	unsigned _int8 valuelen;
	char valheader_buffer[261];
	short& NumSegs() {return *(reinterpret_cast<short*>(valheader_buffer + valuelen));}
	int& URN() {return *(reinterpret_cast<int*>(valheader_buffer + valuelen + 2));}

	//Segment header information
	short num_segs_read;
	char seg_header_buffer[4];
	int& SegHeaderAsInt() {return *(reinterpret_cast<int*>(seg_header_buffer));}
	short& SegNum() {return *(reinterpret_cast<short*>(seg_header_buffer));}
	unsigned short& SegNRecs() {return *(reinterpret_cast<unsigned short*>(seg_header_buffer+2));}

public:
	DU1MergeStream_File(const char*, bool);
	~DU1MergeStream_File();

	//-------------------------
	//From the merge base class
	void ReadFirstKey();
	util::MergeRecordStream* ReadNextKey();

	//-----------------------
	//From the DU1 base class
	void MergeChunkSet(BitMappedFileRecordSet*);
};

//**************************************************************************************
//Always one of these for the final chunk.  Saves writing it all out to file and reading
//it all back in again unnecessarily.
class DU1MergeStream_Mem : public DU1MergeStream 
{
	DU1FieldIndex* index;

	DU1FlushStats stats;
	std::string final_msg;

	void CacheValues();

public:
	DU1MergeStream_Mem(DU1FieldIndex*, bool);
	const std::string& FinalMsg() {return final_msg;}

	//-------------------------
	//From the merge base class
	void ReadFirstKey();
	util::MergeRecordStream* ReadNextKey();

	//-----------------------
	//From the DU1 base class
	void MergeChunkSet(BitMappedFileRecordSet*);
};

//**************************************************************************************
//V3.  Much like the file version, but less tuned because of possible need to endianize,
//de-EBCDICize etc. each individual item.  Some code copied from above though.
class DU1MergeStream_FastLoadTape : public DU1MergeStream 
{
	FastLoadInputFile* tapei;
	bool nofloat_option;
	bool crlf_option;
	bool pai_option;
	bool float_values_in_tape;

	unsigned _int8 string_key_len;
	char string_key_value[256];
	double num_key_value;
	std::string pai_line;
	bool throw_bad_numbers;

	unsigned short numsegs;

public:
	DU1MergeStream_FastLoadTape(FastLoadInputFile*, bool);
	~DU1MergeStream_FastLoadTape();

	//-------------------------
	//From the merge base class
	void ReadFirstKey() {ReadNextKey();}
	util::MergeRecordStream* ReadNextKey();

	//-----------------------
	//From the DU1 base class
	void MergeChunkSet(BitMappedFileRecordSet*);
};




//**************************************************************************************
//V3.
class DU1MergeTask : public util::WorkerThreadTask {
	ThreadSafeDouble total_mrg_time;

public:
	void AddTime(double d) {total_mrg_time.Add(d);}
	double GetTime() {return total_mrg_time.Value();}
};

//***************************
class DU1MergeParallelSubTask : public util::WorkerThreadSubTask {
	SingleDatabaseFileContext* context;
	DU1FieldIndex* fix;
	bool isreorg;

	void Perform();
public:
	DU1MergeParallelSubTask(SingleDatabaseFileContext*, DU1FieldIndex*, bool);

	void ClaimError();
};

} //close namespace

#endif
