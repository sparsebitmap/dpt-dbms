
#include "stdafx.h"

#include "buffmgmt.h"

#include <vector>
#include <set>
#include <algorithm>

//Utils
#include "dataconv.h"
#include "winutil.h"
#include "paged_io.h"
//API tiers
#include "dbfile.h"
#include "checkpt.h"
#include "page_f.h" //#include "page_F.h" : V2.24 case is less interchangeable on *NIX - Roger M.
#include "pagebase.h"
#include "dbserv.h"
#include "core.h"
#include "msgroute.h"
//Diagnostics
#include "msg_db.h"
#include "except.h"
#include "assert.h"

//For Dump
#ifdef _BBHOST
#include "iodev.h"
#else
#include "lineio.h"
#endif

namespace dpt {

Lockable BufferedFileInterface::bufflock;
CheckpointFile* BufferedFileInterface::chkp = NULL;

int BufferedFileInterface::maxbuf = 0;
int BufferedFileInterface::numbuf = 0;
int BufferedFileInterface::numbuf_hwm = 0;
int BufferedFileInterface::max_dirty_skips = 0;
int BufferedFileInterface::min_dirty_skips = 0;
int BufferedFileInterface::dirty_skips = 0;

RawPageData* BufferedFileInterface::membase = NULL;
RawPageData* BufferedFileInterface::memtop = NULL;

BufferPage* BufferedFileInterface::pagebase = NULL;
BufferPage* BufferedFileInterface::pagetop = NULL;
BufferPage* BufferedFileInterface::allocptr = NULL;

BufferPage* BufferPage::lruq_earliest = NULL;
BufferPage* BufferPage::lruq_latest = NULL;




//***************************************************************************************
//Initialization and closedown
//***************************************************************************************
void BufferedFileInterface::Initialize(int n, bool chkp_enabled)
{
	maxbuf = n;

	//For dynamic dirty scan window adjusting during fluctuating workloads
	min_dirty_skips = 4;
	max_dirty_skips = maxbuf / 4; //maxbuf of 32 would give 8
	dirty_skips = max_dirty_skips / 2;

	//Memory is the most likely to fail, so do it first
	if (membase)
		throw Exception(BUFF_MEMORY, "Buffer pool memory has already been initialized");

	try {
		//Reserve the maximum that we will ever take
		membase = static_cast<RawPageData*>(
			VirtualAlloc(NULL, maxbuf * DBPAGE_SIZE, MEM_RESERVE, PAGE_READWRITE));

		if (membase == NULL)
			throw Exception(BUFF_MEMORY, std::string
				("Error reserving buffer pool memory, OS message was: ")
				.append(win::GetLastErrorMessage()));

		pagebase = new BufferPage[maxbuf];
		allocptr = pagebase;
		pagetop = pagebase + maxbuf;

		if (chkp_enabled)
			chkp = new CheckpointFile();
	}
	catch (...) {
		Closedown(NULL);
		throw;
	}
}

//******************************************************************************************
void BufferedFileInterface::Closedown(DatabaseServices* dbapi)
{
	if (chkp)
		delete chkp;

	if (pagebase) {
		//If there are startup errors this won't be present
		if (dbapi) {
			dbapi->Core()->AuditLine(std::string
				("NUMBUF high-water-mark was ")
				.append(util::IntToString(numbuf_hwm)), "SYS");

			//Flush any remaining updated pages for any file, since the EOT flushes 
			//may have failed for some reason, or API users may just have not committed.
			FlushAllDirtyPages(dbapi, NULL);
		}

		delete pagebase;
	}

	if (membase) {
		VirtualFree(membase, 0, MEM_RELEASE);
		membase = NULL; //V2.16
	}
}










//***************************************************************************************
//Construction/destruction
//***************************************************************************************
BufferedFileInterface::BufferedFileInterface(DatabaseFile* p, const std::string& parm_dd, 
				const std::string& parm_dsn, FileDisp disp) 
: dbfile(p), ddname(parm_dd), pagedfile(NULL), npages_btree(0)
#ifdef BBJOIN
	, dkrd_timing_remain(0), dkrd_timed_ct(0), dkrd_time(0)
#endif
{
	LockingSentry ls(&bufflock);

	PagedFileDisp pfd;
	if (disp == FILEDISP_COND)
		pfd = PFD_COND;
	else if (disp == FILEDISP_NEW)
		pfd = PFD_NEW;
	else
		//Anything else we treat it as OLD for database files.
		pfd = PFD_OLD;

	pagedfile = new PagedFile(parm_dsn, pfd);
	seqopt = 0;
}

//***************************************************************************************
BufferedFileInterface::~BufferedFileInterface()
{
	LockingSentry ls(&bufflock);

	//Close the physical file
	if (pagedfile)
		delete pagedfile;

	FreeFilePages();
}

//***************************************************************************************
//Called by the above, anything like CREATE that does direct IO not buffered, and 
//anything like INITIALIZE that invalidates a lot of pages without actually changing them
//***************************************************************************************
void BufferedFileInterface::FreeFilePages()
{
	//Free all memory occupied by this file's pages (see comments in DeactivateBuffer
	//for why we don't just clear the pagetable).  The file is assumed to be exclusively
	//held by the caller so no locks explicitly taken in here.
	std::map<int, BufferPage*>::iterator i;
	for (i = pagetable.begin(); i != pagetable.end(); i++) {
		BufferPage* bp = i->second;

		//This would indicate handles are not being used properly
		if (bp->use_count > 0 && !bp->IsOnReuseQueue())
			throw Exception(BUFF_CONTROL_BUG, 
				"Bug: File pages still in use when freeing file");

		bp->RemoveFromLRUQueue();
		DeactivateBuffer(bp);
	}

	pagetable.clear();
	npages_btree = 0;
}

//******************************************************************************************
void BufferedFileInterface::SetSeqopt(int s)
{
	seqopt = 0;
	pagedfile->SetSeqopt(s);
	seqopt = s;
}











//******************************************************************************************
//The main interface functions for page acquiring, updating and releasing 
//******************************************************************************************
BufferPage* BufferedFileInterface::RequestPage
(DatabaseServices* dbapi, int pagenum, bool noread)
{
	dbfile->IncStatDKPR(dbapi);

	bool dkrdflag = false;
	BufferPage* bp = RequestPage2(dbapi, pagenum, noread, &dkrdflag);

	//See tech notes about SEQOPT, and about whether or not this chunk of code is good.
	if (dkrdflag) {
		for (int x = 1; x <= seqopt; x++) {
			try {
				//With any luck this will now require no physical IO.  If say page 10 was
				//requested, SEQOPT was 6 and page 12 was already in buffer, this is no
				//big deal - we have just done an extra map lookup for it, and hopefully
				//the IO benefits against pages 11, 13, 14 and 15 will make up for it.
				BufferPage* temp = RequestPage2(dbapi, pagenum + x, noread, NULL);

				//The extra buffers are simply put at the back of the LRUQ
				ReleasePage(dbapi, temp);
			}
			catch (Exception&) {
				//Assume EOF reached before SEQOPT pages read in - no problem
				break;
			}
		}
	}
	
	return bp;
}

//******************************************************************************************
BufferPage* BufferedFileInterface::RequestPage2
(DatabaseServices* dbapi, int pagenum, bool noread, bool* dkrdflag)
{
	//This is the only place in database services that does this
	dbapi->Core()->Tick("while requesting database file page");

	BufferPage* bp = RequestPage3(dbapi, pagenum, noread, dkrdflag);

	//Two cases here where we retry.  One is if there are no buffers free and 
	//we can't get one (see later).  This would indicate that the current workload
	//really can't happen without more buffers.  A less serious case is where another 
	//thread is right in the middle of flushing the page we want before it overwrites it.
	if (!bp) {

		//I think this is fairly close to the meaning of the M204 FBWT stat  
		dbfile->IncStatFBWT(dbapi);

		//WT=98 is a custom state for Baby204
		WTSentry ws(dbapi->Core(), 98);

		for (int x = 0;; x++) {
			bp = RequestPage3(dbapi, pagenum, noread, dkrdflag);
			if (bp)
				break;

			//This exception is necessary to avoid deadlock when all buffers are
			//full and all threads are waiting for pages to free up.  With any luck 
			//the caller will do some releasing in response to this.
			if (x == 500)
				throw Exception(BUFF_NEEDMORE, 
					"More disk buffer pages (MAXBUF) are required for this workload");

			Sleep(10); //10ms x 500 = 5 secs - totally approximate of course
		}
	}

	return bp;
}

//******************************************************************************************
BufferPage* BufferedFileInterface::RequestPage3
(DatabaseServices* dbapi, int pagenum, bool noread, bool* dkrdflag)
{
	bool need_read = false;
	bool need_flush = false;
	bool data_ready = false;
	BufferPage* bp = FindOrInsertPage(dbapi, pagenum, need_read, need_flush, data_ready);

	if (!bp)
		return NULL;

	//-----------------------------------------------------------------------
	//There are two main cases here
	//1. We just indexed the page, so we have to do the disk read into the buffer
	if (need_read) {
	
		try {

			//Plus we may have reused a dirty page - we have to write that out first
			if (need_flush) {
				dbfile->IncStatDKSWAIT(dbapi);

				FlushPage(dbapi, bp);

				LockingSentry ls(&bufflock);
				bp->MarkNotDirty();
				bp->MarkNotBeingWritten();

				//Remove index entry for the old page - it can now be read in again if wanted
				bp->buffapi->pagetable.erase(bp->filepage);
				bp->MarkNotBeingPurged();
			}

			//Physically read in the page (unless caller just wanted a blank page)
			//NB. this is done outside the buffer lock
			if (!noread)
				PhysicalPageRead(dbapi, pagenum, bp->PageData(), dkrdflag);

			//Other threads waiting for the same page can have it now
			LockingSentry ls(&bufflock);
			bp->MarkPopulated();
			bp->MarkNotChkpLogged();

			bp->buffapi = this;
			bp->filepage = pagenum;

			if (!noread)
				bp->IncPageTypeStats();

			return bp;
		}
		catch (...) {
			LockingSentry ls(&bufflock);

			//Give up on indexing this page
			pagetable.erase(pagenum);

			//Errors in the first part of the code above, namely writing out a dirty LRU 
			//page, revert it to how it was.
			if (bp->IsBeingPurged()) {
				bp->MarkNotBeingPurged();
				bp->MarkNotBeingWritten();
				bp->MarkPopulated();

				//Put it back on the reuse queue
				bp->DecrementUseCount();
			}

			//Errors in the second part, namely the fresh disk page read, discard the 
			//buffer page - it might be half full of data or anything
			else {
				DeactivateBuffer(bp);
			}

			throw;
		}
	}

	//-----------------------------------------------------------------------
	//2. The page was already indexed
	try {

		//In the simplest case we can just use it, but another thread might still be 
		//reading in the data, in which case wait for their disk read to finish.
		if (!data_ready) {

			//In a real sense we're waiting for physical IO now, so WT=1
			WTSentry s(dbapi->Core(), 1);

			for (int x = 0;;x++) {

				//Not bumpable, but give up if things appear to have gone wrong
				if (x == 1000)
					throw Exception(BUFF_RIDICULOUS_DELAY,
						"Bug: Disk read on another thread is taking ridiculously long");
				else
					Sleep(1); //real sleep to allow lower priority threads to run.

				LockingSentry ls(&bufflock);
				if (bp->IsPopulated()) {

					//Success: The other thread's disk read went OK.  We can use the buffer.
					if (bp->buffapi == this && bp->filepage == pagenum)
						break;
	
					//Fail #1: A completely different file page is now using the buffer.
					//This is what happens if a LRU page write fails (see above func), 
					//but could also happen if the disk read failed, then a third thread
					//nipped in and used the now-free buffer before we got our next time
					//slice.  Slightly unlikely.
					return NULL;
				}

				//Fail #2: The buffer has been deactivated.  This is what happens when
				//a disk read fails.
				if (!bp->IsActive())
					return NULL;
			}
		}

		return bp;
	}
	catch (...) {
		LockingSentry ls(&bufflock);

		//Give up what hold we had on the buffer
		bp->DecrementUseCount();

		throw;
	}
}

//******************************************************************************************
//Note that this function takes the lock but doesn't do any IO
//******************************************************************************************
BufferPage* BufferedFileInterface::FindOrInsertPage
(DatabaseServices* dbapi, int pagenum, 
 bool& need_read, bool& need_flush, bool& data_ready)
{
	WTSentry s(dbapi->Core(), 8);
	LockingSentry ls(&bufflock);

	//Attempt to insert entry straight off to save 2 finds if it's not there
	std::pair<std::map<int, BufferPage*>::iterator, bool> p;
	p = pagetable.insert(std::make_pair<int, BufferPage*>(pagenum, NULL));

	//-----------------------------------------------------------------------
	//Again two main cases, as per above
	//1. The table entry just got inserted, so get a buffer page to read the data into
	if (p.second) {

		try {
			//Option 1 - pages with no data in them at all
			BufferPage* bp = GetFirstFreeBuffer();

			//Option 2 - a page from the reuse queue
			if (!bp)
				bp = GetFirstReusableBuffer(dbapi, need_flush);

			//Option 3 - None available - caller has to try again a bit later (see above)
			if (!bp) {
				pagetable.erase(p.first->first);
				return NULL;
			}

			//OK got one - link it to the pagetable
			p.first->second = bp;

			//Next the caller will do the physical read into it (NB outside lock)
			need_read = true;
			bp->MarkNotPopulated();
			bp->use_count = 1;
			return bp;
		}
		catch (...) {
			pagetable.erase(p.first->first);
			throw;
		}
	}

	//-----------------------------------------------------------------------
	//2. The entry was already present in the table
	BufferPage* bp = p.first->second;

	//Another thread may be removing it from the buffer system right now - we'll start again
	if (bp->IsBeingPurged()) {
//		dbfile->IncStatDKSWAIT(dbapi);	//I think not - will do FBWT
		return NULL;
	}

	//Another thread may already be reading in the data - we'll wait for that
	data_ready = bp->IsPopulated();

	//The page may not have been used for a while, and was on the reuse queue
	if (bp->IsOnReuseQueue()) {
		bp->RemoveFromLRUQueue();
		bp->use_count = 1;
	}
	else {
		bp->use_count++;
		if (bp->ExcessiveUseCount()) {
			bp->use_count--;

			//V2.23.  Roger M was triggering this in an API context.  Better message now.
//			throw Exception(BUFF_CONTROL_BUG, 
//				"Too many users of the same buffer page!?");
			std::string msg = "Too many handles to the same buffer page (";
			msg.append(ddname).append("/#").append(util::IntToString(pagenum));
			msg.append("). Missing clean-up in calling code?");

			throw Exception(BUFF_CONTROL_BUG, msg);
		}
	}

	return bp;
}

//******************************************************************************************
BufferPage* BufferedFileInterface::GetFirstFreeBuffer()
{
	//After a while all buffers will have something in them - none free
	if (numbuf == maxbuf)
		return NULL;

	//There should be one somewhere, although we may have to scan for one.  NB. This
	//is a sequential search, but in the phase where the buffers are initially filling 
	//up the next page will always be free.  After that we hope a decent percentage 
	//will be free, plus the earliest tidied-up pages should also come back moderately 
	//contiguously too.
	for (int scanned = 0;;scanned++) {
		if (scanned > maxbuf)
			throw Exception(BUG_MISC, "Bug: A buffer should be free but isn't");

		if (!allocptr->IsActive())
			break;

		allocptr++; 
		if (allocptr == pagetop) 
			allocptr = pagebase;
	}

	//Commit the memory for this page if required, and do some later pages too
	if (!allocptr->IsMemCommitted()) {
		BufferPage* commit_from = allocptr;

		//See comments in Tidy(..) for why this extra amount isn't more
		BufferPage* commit_to = allocptr + 63;

		if (commit_to >= pagetop)
			commit_to = pagetop-1;

		MemCommitRange(commit_from, commit_to);
	}

	ActivateBuffer(allocptr);
	return allocptr;
}

//******************************************************************************************
BufferPage* BufferedFileInterface::GetFirstReusableBuffer
(DatabaseServices* dbapi, bool& need_flush)
{
	BufferPage* bp = BufferPage::lruq_earliest;

	//All pages are actively in use - hopefully rare unless very small buffer pool
	if (!bp)
		return NULL;

	//Clean page staight away!
	if (!bp->IsDirty()) {

		//So loosen the scan criteria for next time in encouragement (see comment below)
		if (dirty_skips < max_dirty_skips)
			dirty_skips++;
	}

	//First page is dirty, so scan for others.
	else {

		//Fairly close to the meaning of this M204 stat I think
		dbfile->IncStatDKSFBS(dbapi);

		//We ideally keep away from dirty pages, but dirty pages that are currently
		//being written out are *completely* off bounds
		BufferPage* first_usable_dirty = (bp->IsBeingWritten()) ? NULL : bp;
		BufferPage* next = bp;

		int skips;
		for (skips = 1;;skips++) {

			next = next->lruq_later;

			//If a lot of old pages are dirty, we're probably better off flushing them,
			//even if there are newer clean pages.  That way we get the benefit of the
			//full buffer pool.  In a load-type situation it's also beneficial to avoid
			//always scanning the whole buffer pool (which will be totally dirty).  Either
			//way, only scan part of the pool.  The fraction of the pool to scan adjusts
			//dynamically throughout the run based on recent history.
			if (next && skips < dirty_skips) {
				if (next->IsDirty()) {
					if (!first_usable_dirty && !next->IsBeingWritten())
						first_usable_dirty = next;
				}

				//Found a clean page
				else {
					bp = next;
					break;
				}
			}

			//Too many dirty pages - pick the oldest to flush
			else {

				//Tighten the scan criteria a bit for next time case it's persistent.
				//Ultimately with a mostly-dirty pool we will just scan a couple.
				if (dirty_skips > min_dirty_skips)
					dirty_skips--;

				if (!first_usable_dirty)
					return NULL;

				bp = first_usable_dirty;

				bp->MarkBeingPurged();
				bp->MarkBeingWritten();

				need_flush = true;
				break;
			}
		}

		dbfile->AddToStatDKSKIPT(dbapi, skips); //total pages skipped 
		dbfile->HWMStatDKSKIP(dbapi, skips); //hwm - will top out at max_dirty_skips
	}

	bp->RemoveFromLRUQueue();
	bp->DecPageTypeStats();

	//Don't remove the index entry for the reused page if we still have to flush it.
	//If we do that another thread may reread it before the flush happens.
	if (!need_flush)
		bp->buffapi->pagetable.erase(bp->filepage);

	//Reused pages stat
	dbfile->IncStatDKSFNU(dbapi);

	return bp;
}

//******************************************************************************************
void BufferedFileInterface::ReleasePage(DatabaseServices* dbapi, BufferPage* bp)
{
	WTSentry s(dbapi->Core(), 8);
	LockingSentry ls(&bufflock);

	bp->DecrementUseCount();
}

//******************************************************************************************
//Disk writes always happen outside the buffer lock
//******************************************************************************************
void BufferedFileInterface::FlushPage(DatabaseServices* dbapi, BufferPage* bp)
{
	//If this is the first write since the file was completely flushed, the physical
	//file is "page broken" until it's completely flushed again.  This is a nice-to-have
	//if checkpointing is on, but is essential otherwise.
	//* * * 
	//Note to the future:
	//This is a feature which imposes a significant overhead when update units consist
	//of writes to a small number of pages per file, since there will always be a FCT
	//write as the first and last writes in an update.  The FCT would often be written
	//once anyway (MSTRADD/BXDEL etc.) but there is still the extra one.  This is a
	//relatively significant factor if the rest of the update touches say 1 or 2 pages.
	//Having said that, smart users do generally know that less frequent commits can
	//improve performance, so nothing really surprisong is going on.
	//In any case there is potential benefit in rethinking this area, if nothing else
	//to turn off the page-broken mechanism if checkpointing is turned on.
	//
	//* * * 

	//V2.10 - Dec 2007.  The disk file to flush out to is the home file of the page 
	//being flushed, of course, not that of the page which wants a buffer.
	BufferedFileInterface* buffapi = bp->buffapi;
	DatabaseFile* dbfile = buffapi->dbfile;

	bool fct_written_through = dbfile->MarkPageBroken(dbapi, true);

	//No need to write the FCT page again if we just did that directly above
	if (bp->filepage != 0 || !fct_written_through)
		buffapi->PhysicalPageWrite(dbapi, bp->filepage, bp->PageData());
}

//******************************************************************************************
void BufferedFileInterface::PhysicalPageWrite
(DatabaseServices* dbapi, int page, RawPageData* rawpage)
{
	dbfile->IncStatDKWR(dbapi);
	_int64 tbefore = win::GetWinSysTime();

	WTSentry s(dbapi->Core(), 1);
	pagedfile->WritePage(page, rawpage->CharData());

	_int64 tdiff = win::GetWinSysTime() - tbefore;

	if (tdiff > 0)
		dbfile->AddToStatDKUPTIME(dbapi, tdiff);
}

//******************************************************************************************
void BufferedFileInterface::PhysicalPageRead
(DatabaseServices* dbapi, int page, RawPageData* rawpage, bool* dkrdflag)
{
	WTSentry s(dbapi->Core(), 1);

	//April 2008.  This is new so that the join engine can factor in observed
	//disk speed to make more realistic cost estimates.  Decided against maintaining
	//a new stat (DKRDTIME) as that would mean an overhead across the board, whereas
	//we only need to enable this processing if a join is going to happen, and it can
	//be off most of the time.
	//* * Currently untested * *
#ifdef BBJOIN
	_int64 tbefore;
	if (dkrd_timing_remain) {

		//Therefore we may as well use hires counter (cf. disk write above)
		if (win::HFPC_Allowed())
			tbefore = win::HFPC_Query();
		else
			tbefore = win::GetWinSysTime();
	}
#endif

	//Get the page off disk
	bool did_physical_read = pagedfile->ReadPageWithSeqopt(page, rawpage);
	if (dkrdflag)
		*dkrdflag = did_physical_read;

	//DKRD stat when SEQOPT is active:  It gives nice visibly different behaviour if 
	//we only inc DKRD when actual disk access happened, rather than SEQOPT buffer read.
	if (did_physical_read)
		dbfile->IncStatDKRD(dbapi);

#ifdef BBJOIN
	//See comment at top of func.
	if (dkrd_timing_remain) {
		double tdiff;

		if (win::HFPC_Allowed()) {
			tdiff = win::HFPC_Query() - tbefore;       //HFPC quanta
			tdiff = win::HFPC_ConvertToSec(tdiff);
		}
		else {
			tdiff = win::GetWinSysTime() - tbefore;    //100ns units
			tdiff /= 1E7;
		}

		if (tdiff > 0) { //wraparound is vaguely possible
			dkrd_timed_ct++;
			dkrd_timing_remain--;
			dkrd_time += tdiff;
			TRACE("******************* %I64d \n", tdiff);
		}
	}
#endif
}

//******************************************************************************************
#ifdef BBJOIN
//April 2008.  
void BufferedFileInterface::InitiateDKRDTiming()
{
	//Additional effect after a certain number point would be negligible.
	if (dkrd_timed_ct < 10000) {
		dkrd_timing_remain = 100;
		if (seqopt != 0)
			dkrd_timing_remain *= seqopt;
	}
}
#endif









//******************************************************************************************
//Miscellaneous other functions
//******************************************************************************************

//******************************************************************************************
//Used at create time
//******************************************************************************************
void BufferedFileInterface::DirectCreateFile
(DatabaseServices* dbapi, RawPageData* data, int newfilesize)
{
	//Physical OS file resize is the most likely thing to fail so do this next.
	pagedfile->SetSize(newfilesize);

	try {
		//We use direct disk writes here because a) the normal buffer system would log 
		//pre-images which is futile because we can't roll back anyway, and b) we want 
		//to guarantee the disk writes happen now.	
		PhysicalPageWrite(dbapi, 0, data);
	}
	catch (...) {
		//Some kind of IO error, so make sure the file's unusable
		pagedfile->SetSize(0);
	}
}

//***************************************************************************************
//Called when the first user opens the file
//***************************************************************************************
void BufferedFileInterface::ValidateFileSize()
{
	const char* msg = NULL;
	int numpages = pagedfile->GetSize();

	//V2.27 This often happens if a file has not been created yet, so give a wider
	//range of meaningful messages.
	_int64 numbytes = pagedfile->GetNumBytes();

//	Mbox(util::Int64ToString(numbytes).c_str());

	if (numpages == 0 && numbytes == 0)
		msg = "The file is empty (issue 'Create' to turn it into an openable database)";
	else if (numbytes % DBPAGE_SIZE != 0)
		msg = "The file size is not valid for a DPT database file";

	if (msg)
		throw Exception(DB_BAD_FILE_CONTENTS, msg);
}

//******************************************************************************************
//This is the function which is called when the last updater of a file finishes their
//update unit.  It's driven from a single pass across the whole buffer pool, merely to 
//avoid possible problems with map iterators becoming out of date when we release the 
//buffer lock.  This way is just as simple really, even if it means scanning thousands
//of pages just to flush one or two.  The disk writes make any such concerns trivial.
//******************************************************************************************
int BufferedFileInterface::FlushAllDirtyPages
(DatabaseServices* dbapi, BufferedFileInterface* single_file)
{
	int num_flushed = 0;
	std::set<BufferedFileInterface*> still_dirty;
	BufferPage* bp = pagebase - 1; //prime inner loop

	//Loop until no more pages to flush.  Note that we assume here that the caller has
	//blocked any further updates from starting for the file, so a single pass is OK.
	for (;;) {

		bool writeit = true;

		//Dummy block for lock auto-release
		{
			LockingSentry ls(&bufflock);

			//This inner loop saves thrashing the lock on and off
			for (;;) {
				bp++;
				if (bp == pagetop)
					break;

				//Ignore clean pages
				if (!bp->IsDirty())
					continue;

				//Ignore other files' pages in single-file mode
				if (single_file && bp->buffapi != single_file)
					continue;

				//Since this function is called in the context of a single file there's no
				//point really skipping over pages that are currently being written and 
				//coming back to them later, since we would just end up blocked at the 
				//disk IO level when trying to write out the next one we found.  
				//Therefore just spin - should be fairly uncommon anyway.
				if (bp->IsBeingWritten())
					writeit = false;

				break;
			}
			if (bp == pagetop)
				break;

			//The purpose of this flag is to make sure that if we are writing a page out,
			//the LRU code doesn't try and write it too.  And vice versa - see tech doc.
			if (writeit)
				bp->MarkBeingWritten();
		}
		if (bp == pagetop)
			break;

		//Spin then check the same page again, since the thread currently writing the 
		//page may fail, meaning we'd then have to do it.
		if (!writeit) {
			Sleep(1); //real sleep to allow lower priority threads to run.
			bp--;
			continue;
		}

		//OK, do the disk write.  NB. outside lock here.
		try {
			bp->buffapi->FlushPage(dbapi, bp);

			LockingSentry ls(&bufflock);
			bp->MarkNotDirty();
			bp->MarkNotBeingWritten();

			//Make a note that we did it
			num_flushed++;
			if (!single_file)
				still_dirty.insert(bp->buffapi);
		}
		catch (...) {
			LockingSentry ls(&bufflock);
			bp->MarkNotBeingWritten();
			throw;
		}
	}

	//If we get to here we have successfully flushed all pages
	if (single_file)
		single_file->dbfile->MarkPageBroken(dbapi, false);
	else {
		std::set<BufferedFileInterface*>::iterator i;
		for (i = still_dirty.begin(); i != still_dirty.end(); i++)
			(*i)->dbfile->MarkPageBroken(dbapi, false);
	}

	return num_flushed;
}

//******************************************************************************************
//Possible future function for speculative pre-emptive writes.
//NB. A key difference from the above is that here we don't need to provide the guarantee
//to the caller that on return there are no dirty pages left for the file.  This will only
//ever be called *hoping* to flush a page.
//******************************************************************************************
/*
bool BufferedFileInterface::PreEmptiveFlushOneDirtyPage(DatabaseServices* dbapi)
{
	throw Exception("Untested function!"); //see comment above and throughout func

	BufferPage* bp;

	//Dummy block for lock auto-release
	{
		LockingSentry ls(&bufflock);

		//Scan back through the LRUQ
		bp = BufferPage::lruq_earliest;

		for (;;) {
			if (!bp)
				return false;

			//Ignore pages currently being written.  NB. No guarantee for a caller of
			//this function that the write will succeed.  Compare with prev function.
			if (bp->IsDirty() && !bp->IsBeingWritten())
				break;

			bp = bp->lruq_later;
		}

		//This is still required though - see prev func for comment
		bp->MarkBeingWritten();
	}

	try {
		bp->buffapi->FlushPage(dbapi, bp);

		LockingSentry ls(&bufflock);
		bp->MarkNotDirty();
		bp->MarkNotBeingWritten();

		return true;
	}
	catch (...) {
		LockingSentry ls(&bufflock);
// * * * undo any more specific control info changes for this function
		bp->MarkNotBeingWritten();
		throw; 
	}
}
*/

//******************************************************************************************
void BufferedFileInterface::ChkpLogPage(DatabaseServices* dbapi, BufferPage* bp)
{
	chkp->WritePreImage(dbapi, bp->PageData(), 
						bp->buffapi->ddname.c_str(), bp->filepage);
}

//******************************************************************************************
void BufferedFileInterface::CheckpointProcessing(DatabaseServices* dbapi, time_t cptimestamp)
{
	if (FlushAllDirtyPages(dbapi, NULL) > 0)
		dbapi->Core()->GetRouter()->Issue(TXN_EOT_FLUSH_FAILED, std::string
			("Info: Pages had to be flushed before taking checkpoint"));
	chkp->Reinitialize(dbapi, cptimestamp);

	//Could have maintained a list but why waste memory - not performance critical
	LockingSentry ls(&bufflock);
	for (BufferPage* bp = pagebase; bp < pagetop; bp++)
		if (bp->IsChkpLogged())
			bp->MarkNotChkpLogged();
}

//******************************************************************************************
int BufferedFileInterface::Tidy(time_t cutoff)
{
	LockingSentry ls(&bufflock);

	int pages_freed = 0;

	//Starting with the oldest not-in-use page.
	BufferPage* next = BufferPage::lruq_earliest;

	while (next) {
		//Slightly peculiar control logic because we may remove bp from the LRUQ below
		BufferPage* bp = next;
		next = bp->lruq_later;

		//Reached the first page that's recent enough
		if (bp->last_used_time >= cutoff)
			break;

		//Leave alone dirty pages or those in interim states
		if (bp->IsDirty() || bp->IsBeingPurged() || bp->IsBeingWritten())
			continue;

		bp->RemoveFromLRUQueue();
		bp->buffapi->pagetable.erase(bp->filepage);
		bp->DecPageTypeStats();
		DeactivateBuffer(bp);
		pages_freed++;
	}

/* * * 
	Note:
	It would be ideal to clean up buffer pages that have been committed to Windows
	but not yet used as buffers here.  In normal use perhaps not, but if the above
	code had otherwise left the buffer pool empty then going this extra step would
	be logical.  Unfortunately I can't think of a way to determine here whether 
	there are any buffers in use but not on the LRUQ without scanning every single
	page.  Not ideal.  This is the reason the allocation chunk amount is not more -
	for example at the time of writing it is 64 pages, meaning on average there will 
	be 32 pages (256K) left committed when the system goes totally quiet.  I guess 
	this isn't too bad when the executable itself is a couple of meg.
* * */

	return pages_freed;
}













//******************************************************************************************
//Memory management
//******************************************************************************************
void BufferedFileInterface::MemCommitRange(BufferPage* bpfrom, BufferPage* bpto)
{
	int ixfrom = GetIxFromBuffPtr(bpfrom);

	void* v = VirtualAlloc(
				GetMemPtrFromIx(ixfrom), 
				(bpto - bpfrom + 1) * DBPAGE_SIZE, 
				MEM_COMMIT, 
				PAGE_READWRITE);
		
	if (!v)
		throw Exception(BUFF_MEMORY, std::string(
			"Error committing page of virtual buffer pool: ")
			.append(win::GetLastErrorMessage()));

	for (BufferPage* x = bpfrom; x <= bpto; x++)
		x->MarkCommitted();
}

//******************************************************************************************
void BufferedFileInterface::MemDeCommitPage(BufferPage* bp)
{
	int ix = GetIxFromBuffPtr(bp);
	RawPageData* mem = GetMemPtrFromIx(ix);

	BOOL b = VirtualFree(mem, DBPAGE_SIZE, MEM_DECOMMIT);
	if (!b)
		throw Exception(BUFF_MEMORY, 
			"Error de-committing page of virtual buffer pool");

	bp->MarkNotCommitted();
}

//******************************************************************************************
void BufferedFileInterface::ActivateBuffer(BufferPage* bp)
{
	numbuf++;
	if (numbuf > numbuf_hwm)
		numbuf_hwm = numbuf;
	bp->MarkActive();
}

//******************************************************************************************
void BufferedFileInterface::DeactivateBuffer(BufferPage* bp)
{
	numbuf--;

	//NB. As it stands, pages that are deactivated are always decommitted too.  The usual
	//place for this function to be called is from Tidy(), in which case the whole point
	//is to recover memory.  However we also call it occasionally if there is a read error 
	//after a buffer has been prepared.  Just deactivating the buffer would mean it *would*
	//eventually get picked up again for use, but would mean it was invisible to the Tidy
	//routine which is driven by from the reuse queue.  Therefore it's cleanest to decommit
	//the buffer in this instance too.
	//A late addition that I'd forgotten about:  When freeing files we call this routine
	//to clear all their buffer pages from memory.  The same point applies as above, and
	//depending on how good Windows is at decommitting pages this may or may not introduce
	//a noticeable delay when freeing a file.
	bp->Initialize();
	MemDeCommitPage(bp);
}











//******************************************************************************************
//Buffer page class
//******************************************************************************************

//******************************************************************************************
//NB. These two are only called under general buffer lockdown - see above
//******************************************************************************************
void BufferPage::RemoveFromLRUQueue()
{
	if (lruq_earlier)
		lruq_earlier->lruq_later = lruq_later;
	else
		lruq_earliest = lruq_later;

	if (lruq_later)
		lruq_later->lruq_earlier = lruq_earlier;
	else
		lruq_latest = lruq_earlier;

	lruq_earlier = NULL;
	lruq_later = NULL;

	//The timestamp now becomes the use count again
	use_count = 0;
}

//******************************************************************************************
void BufferPage::DecrementUseCount()
{
	use_count--;

	//Add to the reuse queue if appropriate
	if (use_count > 0)
		return;

	lruq_later = NULL;
	lruq_earlier = lruq_latest;
	lruq_latest = this;

	if (lruq_earlier)
		lruq_earlier->lruq_later = this;
	else
		lruq_earliest = this;

	//The use count now gets replaced with a timestamp for tidying up later on
	//V2.05. Apr 07.  Low word of time is fine even when 64 bit time comes in.
	//time(&last_used_time);
	time_t temp;
	time(&temp);
	last_used_time = temp;
}

//******************************************************************************************
//NB. It is assumed that the file algorithms will be using CFRs correctly.  This means here
//that we can be confident that no other threads have any interest in the page and we can
//access the buffer chkp flag without using the buffer lock.
//******************************************************************************************
void BufferPage::MakeDirty(DatabaseServices* dbapi)
{
	//--------------
	//Bit of a drag to have to pass this pointer all the way in just to do the assert
	//but I think it's worth it.  Found a couple of bugs already through this.
	//Could rejig later.   Maybe automatically create a NBU for the file, although that 
	//would then entail a map search for every page update.  
	//Better for the code to remember to do it.
	//NB. It's crucial for the page-level integrity of the whole file IO system
	//that when any page is modified, the file has been registered as updated, EVEN
	//FOR "NON-SIGNIFICANT" UPDATES, such as opening the file in some cases.
	assert(dbapi);
	assert(dbapi->FileIsBeingUpdated(buffapi->dbfile));
	//--------------

	//This flag reflects the *physical* update status of the file, as reported by
	//the STATUS command, not logical update status.  Hence it's set here and not
	//in DatabaseFile::BeginUpdate(), although it should in theory give the same 
	//result.  See also the related updated_since_checkpoint.
	if (!IsDirty())
		buffapi->dbfile->MarkPhysicallyUpdatedEver();

	//Write a pre-image to the checkpoint file if necessary.
	if (dbapi->ChkpIsEnabled() && !IsChkpLogged()) {

		//Log first to prevent FCT getting logged twice
		MarkChkpLogged();

		//Log the FCT first if this is the first dirty page in the file since last CP.
		//Therefore note that the FCT is *always* the first page after CP for each file
		buffapi->dbfile->MarkPhysicallyUpdatedSinceCheckpoint(dbapi, true);
		
		BufferedFileInterface::ChkpLogPage(dbapi, this);
	}

	MarkDirty();

	//The caller is going to change the page data, so set page update time now.
	//* * * Couldn't see the need for this - not doing it now.  See docs for details.
//	GenericPage tp(this);
//	tp.SetTimeStamp();
}

//******************************************************************************************
//V2.12. April 2008.
//Functions that help maintain summary stats by file and page type.  Initially this was
//added to support the join engine, but might prove generally useful one day.  NB.  The 
//Dump2 function later gets this info by scanning, but the one(s) here may be needed fast.
//******************************************************************************************
//Called when the DBMS formats a previously-acquired empty page
void BufferPage::NoteFreshFormattedPage(char pagetype)
{
	if (pagetype == 'T')
		buffapi->npages_btree++;
}

//**********************************
//Called when a formatted page is read from table B or D
void BufferPage::IncPageTypeStats()
{
	GenericPage gp(PageData());
	char pagetype = gp.PageType();

	if (pagetype == 'T')
		buffapi->npages_btree++;
}

//**********************************
//Called when removing a page from buffers (falls off, or is forced off the LRUQ)
void BufferPage::DecPageTypeStats()
{
	GenericPage gp(PageData());
	char pagetype = gp.PageType();

	if (pagetype == 'T')
		buffapi->npages_btree--;
}





//******************************************************************************************
//Diagnostics
//******************************************************************************************
#ifdef _BBHOST
void BufferedFileInterface::Dump1(IODev* op, int pagefrom, int pageto)
#else
void BufferedFileInterface::Dump1(util::LineOutput* op, int pagefrom, int pageto)
#endif
{
	if (pagefrom < 0) 
		pagefrom = 0;
	if (pageto < 0 || pageto >= maxbuf) 
		pageto = maxbuf - 1;
	if (pagefrom > pageto) 
		pageto = pagefrom;

	LockingSentry ls(&bufflock);

	op->WriteLine("                                    <---Flags---> Use ct/  <-----LRUQ------>");
	op->WriteLine("Buffer/Range      File     Page#    M A P D C W P LRU Time Earlier  Later   ");
	op->WriteLine("----------------- -------- -------- - - - - - - - -------- -------- --------");

	std::string prevpageinfo;
	int prevpagenum = -1;
	int pagenum;

	BufferPage* bp;
	for (bp = pagebase + pagefrom; bp < pagebase + pageto; bp++) {

		std::string pageinfo = bp->Dump();
		pagenum = bp - pagebase;

		//Don't print each line unless something is different
		if (pageinfo != prevpageinfo) {

			if (prevpagenum != -1) {
				std::string numinfo = util::IntToString(prevpagenum);

				//Several pages or just one may have had the same info
				if (pagenum - 1 > prevpagenum) {
					numinfo.append("-");
					numinfo.append(util::IntToString(pagenum - 1));
				}
				op->Write(util::PadRight(numinfo, ' ', 18));
				op->WriteLine(prevpageinfo);
			}

			prevpageinfo = pageinfo;
			prevpagenum = pagenum;
		}
	}

	//Any unprinted info
	std::string numinfo = util::IntToString(prevpagenum);

	//Several pages or just one may have had the same info
	pagenum = bp - pagebase;
	if (pagenum - 1 > prevpagenum) {
		numinfo.append("-");
		numinfo.append(util::IntToString(pagenum));
	}
	op->Write(util::PadRight(numinfo, ' ', 18));
	op->WriteLine(prevpageinfo);
}

//******************************************************************************************
std::string BufferPage::Dump()
{
	std::string result;
	if (buffapi) {
		result = util::PadRight(buffapi->ddname, ' ', 9);
		result.append(util::PadRight(util::IntToString(filepage), ' ', 9));
	}
	else {
		result = "                  "; //2 x 9
	}

	result.append( (IsMemCommitted()) ? "Y " : "  ");
	result.append( (IsActive())       ? "Y " : "  ");
	result.append( (IsPopulated())    ? "Y " : "  ");
	result.append( (IsDirty())        ? "Y " : "  ");
	result.append( (IsChkpLogged())   ? "Y " : "  ");
	result.append( (IsBeingWritten()) ? "Y " : "  ");
	result.append( (IsBeingPurged())  ? "Y " : "  ");

	if (!IsOnReuseQueue()) {
		result.append(util::SpacePad(use_count, 8));
		result.append(1, ' ');
	}
	else {
		tm t_tm = win::GetDateAndTime_tm(last_used_time);
		char buff[16];
		strftime(buff, 16, "%H:%M:%S ", &t_tm);
		result.append(buff);

		if (lruq_earlier)
			result.append(util::PadRight(
				util::IntToString(lruq_earlier - BufferedFileInterface::pagebase), ' ', 8));
		else
			result.append("*--->   ");

		result.append(1, ' ');

		if (lruq_later)
			result.append(util::PadRight(
				util::IntToString(lruq_later - BufferedFileInterface::pagebase), ' ', 8));
		else
			result.append("*<---   ");
	}

	return result;
}

//******************************************************************************************
//This delivers a special variety of the MONITOR DISKBUFF command.
//******************************************************************************************
#ifdef _BBHOST
void BufferedFileInterface::Dump2(IODev* op)
#else
void BufferedFileInterface::Dump2(util::LineOutput* op)
#endif
{
	//Get the current set of files
	std::vector<FileHandle> files;
	AllocatedFile::ListAllocatedFiles(files, BOOL_SHR, FILETYPE_DB);

	int tot_dat = 0;
	int tot_ebm = 0;
	int tot_att = 0;
	int tot_btn = 0;
	int tot_btl = 0;
	int tot_inv = 0;
	int tot_oth = 0;
	int tot_tot = 0;
	std::vector<std::string> result;

	LockingSentry ls(&bufflock);

	for (size_t x = 0; x < files.size(); x++) {
		DatabaseFile* f = static_cast<DatabaseFile*>(files[x].GetFile());
		BufferedFileInterface* fi = f->BuffAPI();

		if (fi->pagetable.size() == 0)
			continue;

		//Process each buffered page for the current file and accumulate totals by type
		int fil_dat = 0;
		int fil_ebm = 0;
		int fil_att = 0;
		int fil_btn = 0;
		int fil_btl = 0;
		int fil_inv = 0;
		int fil_oth = 0;

		std::map<int, BufferPage*>::const_iterator pi;
		for (pi = fi->pagetable.begin(); pi != fi->pagetable.end(); pi++) {

			GenericPage gp(pi->second->PageData());
			char ptype = gp.PageType();

			switch (ptype)
			{
			//I knew there would be some use for the page type character!
			case 'B':
				fil_dat++; break;
			case 'E':
			case 'P':
				fil_ebm++; break;
			case 'A':
				fil_att++; break;
			case 'T':				//btree
				fil_btn++;
				if (gp.PageSubType() == 'L')
					fil_btl++;
				break;
			case 'I':				//ILMR
			case 'M':				//IL bitmap
			case 'V':				//IL list
				fil_inv++; break;
			default:
				fil_oth++; break;
			}
		}

		//Write the file totals
		std::string line = util::PadRight(files[x].GetDD(), ' ', 8);
		line.append(util::SpacePad(fil_dat, 8, false, true));
		line.append(util::SpacePad(fil_ebm, 8, false, true));
		line.append(util::SpacePad(fil_att, 8, false, true));
		line.append(util::SpacePad(fil_btn, 8, false, true));
		line.append(util::SpacePad(fil_btl, 8, false, true));
		line.append(util::SpacePad(fil_inv, 8, false, true));
		line.append(util::SpacePad(fil_oth, 8, false, true));

		//Don't count leaves twice!
//		int fil_tot = fil_dat + fil_ebm + fil_att + fil_btn + fil_btl + fil_inv + fil_oth;
		int fil_tot = fil_dat + fil_ebm + fil_att + fil_btn + fil_inv + fil_oth;
		line.append(util::SpacePad(fil_tot, 8, false, true));

		result.push_back(line);

		//Add to overall totals
		tot_dat += fil_dat;
		tot_ebm += fil_ebm;
		tot_att += fil_att;
		tot_btn += fil_btn;
		tot_btl += fil_btl;
		tot_inv += fil_inv;
		tot_oth += fil_oth;
		tot_tot += fil_tot;
	}

	//Column headings
	//             0         10      18      26      34      42      50      58      66
	op->WriteLine("File      Data    EBM     F Atts  BtNode  (Leaf)  IvList  Other   Total");
	op->WriteLine("--------  ------  ------  ------  ------  ------  ------  ------  ------");

	//Sort the file totals into alphabetical order before printing
	std::sort(result.begin(), result.end());
	for (size_t y = 0; y != result.size(); y++)
		op->WriteLine(result[y]);

	//Append overall totals
	result.push_back("--------  ------  ------  ------  ------  ------  ------  ------  ------");

	std::string line = "Total   ";
	line.append(util::SpacePad(tot_dat, 8, true, true));
	line.append(util::SpacePad(tot_ebm, 8, true, true));
	line.append(util::SpacePad(tot_att, 8, true, true));
	line.append(util::SpacePad(tot_btn, 8, true, true));
	line.append(util::SpacePad(tot_btl, 8, true, true));
	line.append(util::SpacePad(tot_inv, 8, true, true));
	line.append(util::SpacePad(tot_oth, 8, true, true));
	line.append(util::SpacePad(tot_tot, 8, true, true));
	op->WriteLine(line);
}

//******************************************************************************************
//IDEA: Could make this generic by page type - e.g. x = NumPages(BUFF_MEM_COMMITTED);
int BufferedFileInterface::NumCommittedPages()
{
	if (numbuf == maxbuf)
		return maxbuf;

	LockingSentry ls(&bufflock);

	int result = 0;
	for (BufferPage* bp = pagebase; bp < pagetop; bp++)
		if (bp->IsMemCommitted())
			result++;

	return result;
}

} //close namespace
