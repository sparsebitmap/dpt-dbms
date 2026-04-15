/****************************************************************************************
All the main stuff for making it so that the file algorithms can read and update pages
as if they were doing so on the file directly but actually on buffered data.  See the
tech docs for details on buffer control information and the crucial threading aspects
of this processing.

Checkpointing is done via this module too, the first time each page is updated.
****************************************************************************************/

#ifndef BB_BUFFMGMT
#define BB_BUFFMGMT

#include <map>
#include <string>

#include "lockable.h"

#include "apiconst.h"
#include "rawpage.h"

namespace dpt {

class Recovery;
class CheckpointFile;
class DatabaseFile;
class DatabaseServices;
class PagedFile;
class BufferPage;
class RawPageData;

class IODev;
namespace util { class LineOutput; }

const int BUFF_MAX_USE_COUNT = 100;

//**************************************************************************************
class BufferedFileInterface {

	DatabaseFile* dbfile;
	std::string ddname;
	static CheckpointFile* chkp;

	//The physical file
	friend class DatabaseFile;
	PagedFile* pagedfile;

	int seqopt;
	void SetSeqopt(int);

	//Lookup table of pages to where they are in memory, or at least their control blocks
	std::map<int, BufferPage*> pagetable;

	//V2.12. April 2008.
	int npages_btree;
#ifdef BBJOIN
	//* * untested * *
	int dkrd_timed_ct;
	int dkrd_timing_remain;
	double dkrd_time;
	void InitiateDKRDTiming();
	double GetAveDKRDTime() {if (dkrd_timed_ct) return dkrd_time / dkrd_timed_ct; return -1;}
#endif

	static int maxbuf;
	static int numbuf;
	static int numbuf_hwm;
	static int max_dirty_skips;
	static int min_dirty_skips;
	static int dirty_skips;

	//Memory management
	static RawPageData* membase;
	static RawPageData* memtop;
	static void MemCommitRange(BufferPage*, BufferPage*);
	static void MemDeCommitPage(BufferPage*);

	//The control blocks
	static BufferPage* pagebase;
	static BufferPage* pagetop;
	static BufferPage* allocptr;
	static void ActivateBuffer(BufferPage*);
	static void DeactivateBuffer(BufferPage*);

	static int GetIxFromBuffPtr(BufferPage*);
	static RawPageData* GetMemPtrFromIx(int);

	friend class BufferPage;
	void ReleasePage(DatabaseServices*, BufferPage*);
	BufferPage* RequestPage(DatabaseServices*, int, bool noread = false);
	static RawPageData* PageData(BufferPage* b) {return GetMemPtrFromIx(GetIxFromBuffPtr(b));}

	//Protects the whole thing - usually held only briefly.  Always locking all files'
	//lookup tables turns out to be realistically the only way to do it.
	static Lockable bufflock;

//*

	//Utility functions
	BufferPage* RequestPage2(DatabaseServices*, int, bool, bool*);
	BufferPage* RequestPage3(DatabaseServices*, int, bool, bool*);
	BufferPage* FindOrInsertPage(DatabaseServices*, int, bool&, bool&, bool&);
	BufferPage* GetFirstFreeBuffer();
	BufferPage* GetFirstReusableBuffer(DatabaseServices*, bool&);
	//V2.10 - Dec 2007.  FlushPage() now static.
	static void FlushPage(DatabaseServices*, BufferPage*);
	void PhysicalPageWrite(DatabaseServices*, int, RawPageData*);

	static void Initialize(int, bool);
	static void Closedown(DatabaseServices*);

	BufferedFileInterface(DatabaseFile*, const std::string&, 
		const std::string&, FileDisp);
	~BufferedFileInterface();
	void FreeFilePages();

	void DirectCreateFile(DatabaseServices*, RawPageData*, int);

	//Typically used at commit.  Guarantees all dirty pages written (unless it throws)
	static int FlushAllDirtyPages(DatabaseServices*, BufferedFileInterface*);
	//Flush a single page in attempt to reduce LRUQ faults later on.  No guarantee.
	static bool PreEmptiveFlushOneDirtyPage(DatabaseServices*);

	static void CheckpointProcessing(DatabaseServices*, time_t);
	static void ChkpLogPage(DatabaseServices*, BufferPage*);

	static int Tidy(time_t);

	//Checkpointing does direct file reads and writes
	friend class Recovery;
	void ValidateFileSize();
	PagedFile* GetPagedFile() {return pagedfile;}
	void PhysicalPageRead(DatabaseServices*, int, RawPageData*, bool* = NULL);

public:
#ifdef _BBHOST
	static void Dump1(IODev*, int, int); //=BUFFDUMP
	static void Dump2(IODev*); //MONITOR DISKBUFF
#else
	static void Dump1(util::LineOutput*, int, int); //=BUFFDUMP
	static void Dump2(util::LineOutput*); //MONITOR DISKBUFF
#endif

	static int NumCommittedPages(); //MONITOR MEMORY

	int Npages() {return pagetable.size();}
	int NpagesBtree() {return npages_btree;}
};

//**************************************************************************************
class BufferPage {

	//Referback info required during page re-use and buffer pool tidy-up processing
	BufferedFileInterface* buffapi;
	int filepage;

	//Reuse queue
	BufferPage* lruq_earlier;
	BufferPage* lruq_later;
	static BufferPage* lruq_earliest;
	static BufferPage* lruq_latest;
	union {
		long int use_count;		//if it's small
		long last_used_time;	//if it's large
	};

	//Page status flags.  See tech docs for some more comments.
	enum {
		BUFF_MEM_COMMITTED	= 0x01, 
		BUFF_PAGE_ACTIVE	= 0x02, 
		BUFF_HAS_DATA		= 0x04,
		BUFF_DIRTY			= 0x08,
		BUFF_CHKP_LOGGED	= 0x10,
		BUFF_BEING_WRITTEN	= 0x20,
		BUFF_BEING_PURGED	= 0x40  //redundant??
	};
	char flags;

	bool IsMemCommitted() {return (flags & BUFF_MEM_COMMITTED) ? true : false;}
	bool IsActive() {return (flags & BUFF_PAGE_ACTIVE) ? true : false;}
	bool IsPopulated() {return (flags & BUFF_HAS_DATA) ? true : false;}
	bool IsDirty() {return (flags & BUFF_DIRTY) ? true : false;}
	bool IsChkpLogged() {return (flags & BUFF_CHKP_LOGGED) ? true : false;}
	bool IsBeingWritten() {return (flags & BUFF_BEING_WRITTEN) ? true : false;}
	bool IsBeingPurged() {return (flags & BUFF_BEING_PURGED) ? true : false;}

	void MarkCommitted() {flags |= BUFF_MEM_COMMITTED;}
	void MarkActive() {flags |= BUFF_PAGE_ACTIVE;}
	void MarkPopulated() {flags |= BUFF_HAS_DATA;}
	void MarkDirty() {flags |= BUFF_DIRTY;}
	void MarkChkpLogged() {flags |= BUFF_CHKP_LOGGED;}
	void MarkBeingWritten() {flags |= BUFF_BEING_WRITTEN;}
	void MarkBeingPurged() {flags |= BUFF_BEING_PURGED;}

	void MarkNotCommitted() {flags &= ~BUFF_MEM_COMMITTED;}
	void MarkNotActive() {flags &= ~BUFF_PAGE_ACTIVE;}
	void MarkNotPopulated() {flags &= ~BUFF_HAS_DATA;}
	void MarkNotDirty() {flags &= ~BUFF_DIRTY;}
	void MarkNotChkpLogged() {flags &= ~BUFF_CHKP_LOGGED;}
	void MarkNotBeingWritten() {flags &= ~BUFF_BEING_WRITTEN;}
	void MarkNotBeingPurged() {flags &= ~BUFF_BEING_PURGED;}

	void Initialize() {buffapi = NULL; filepage = -1; flags = 0;
						lruq_earlier = NULL; lruq_later = NULL; use_count = 0;}

	//Construction and destruction
	friend class BufferedFileInterface;
	BufferPage() {Initialize();}
	std::string Dump();

	void RemoveFromLRUQueue();
	void DecrementUseCount();

	//Two different uses of this field
	bool ExcessiveUseCount() {return (use_count > BUFF_MAX_USE_COUNT);}
	bool IsOnReuseQueue() {return ExcessiveUseCount();}

	//Always owned by a trusted class to ensure buffers are always released correctly
	friend class BufferPageHandle;
	friend class DatabaseFile;
	static BufferPage* Request
		(DatabaseServices* dbapi, BufferedFileInterface* f, int p, bool noread = false) {
								return f->RequestPage(dbapi, p, noread);}
	static void Release(DatabaseServices* dbapi, BufferPage* bp) {
								bp->buffapi->ReleasePage(dbapi, bp);}

	friend class DatabaseFilePage;
	void MakeDirty(DatabaseServices*);
	RawPageData* PageData() {return BufferedFileInterface::PageData(this);}

	//V2.12. April 2008. For summary stats by page type
	void IncPageTypeStats();
	void DecPageTypeStats();
	void NoteFreshFormattedPage(char c);
};


//******************************************************************************************
//Since both the buffer data blocks (8k) and the control objects are allocated at start up
//time we can be sure that the Nth of one goes with the Nth of the other, without having
//to keep them in the same object.
//******************************************************************************************
inline RawPageData* BufferedFileInterface::GetMemPtrFromIx(int ix)
{
	return membase + ix;
}

//******************************************************************************************
inline int BufferedFileInterface::GetIxFromBuffPtr(BufferPage* bp)
{
	return bp - pagebase;
}


} //close namespace

#endif
