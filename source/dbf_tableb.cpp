
#include "stdafx.h"

#include "dbf_tableb.h"

//Utils
#include "dataconv.h"
//API tiers
#include "cfr.h"
#include "page_b.h" //#include "page_B.h" : V2.24 case is less interchangeable on *NIX - Roger M.
#include "page_f.h" //#include "page_F.h"
#include "dbfile.h"
#include "dbctxt.h"
#include "dbserv.h"
//Diagnostics
#include "except.h"
#include "msg_db.h"

//For tableB command
#ifdef _BBHOST
#include "iodev.h"
#else
#include "lineio.h"
#endif

namespace dpt {

//*************************************************************************************
BufferPageHandle DatabaseFileTableBManager::GetTableBPage
(DatabaseServices* dbapi, int p, bool fresh)
{
	FCTPage_B fctpage(dbapi, file->fct_buff_page);

	//NB no lock required here, as DEFRAG etc that change the LPM take file EXCL
	if (p < 0)
		throw Exception(DB_BAD_PAGE_NUMBER, "Bug: Negative table B page number");
	if (p > fctpage.GetBhighpg())
		throw Exception(DB_BAD_PAGE_NUMBER, "Bug: Table B page number exceeds BHIGHPG");

	return fctpage.GetAbsoluteBPageHandle(dbapi, file->buffapi, p, fresh);
}

//*************************************************************************************
int DatabaseFileTableBManager::AllocateNewRecordSlot
//(DatabaseServices* dbapi, bool maybe_benign, const FieldValue* extending_val) //V3.0
(DatabaseServices* dbapi, bool maybe_benign, bool extending, short extending_len)
{
	FCTPage_B fctpage(dbapi, file->fct_buff_page);

	short slotnum = -1;

	//First check current high page for a free slot
	int pagenum = fctpage.GetBhighpg();
	int baserec = BaseRecNumOnPage(pagenum);

	if (pagenum != -1) {

		//V3.0. No need to try this for extensions during load - we know a whole page was requested
		if (extending && extending_len > 256)
			;
		else {
			BufferPageHandle h = GetTableBPage(dbapi, pagenum);
			RecordDataPage p(h);

			//slotnum = p.AllocateSlotWithoutReuse(baserec, cached_breserve, extending_val); //V3.0
			slotnum = p.AllocateSlotWithoutReuse(baserec, cached_breserve, extending, extending_len);
		}
	}

	//No luck, so go to the reuse queue if appropriate

	//* * * Note:
	//Strictly speaking this is a reuse stack, not a queue.  A queue would be better
	//from the point of view that the most recently deleted records are most likely to
	//foil attempts to reuse the record numbers because their transaction has not yet
	//been committed.  The trade-off is that maintaining a queue would require us to
	//retrieve and modify another table B page when appending pages to the queue.  The
	//constraints log issue could cause pages to be removed from the stack almost as
	//soon as they're added, but as with M204 this problem is partially addressed by 
	//the "try-random-pages" phase below.
	//Interestingly the stack method has a sort of nice feature that if a large number
	//of stores are backed out, the pages are placed on the queue (i.e. stack) so that
	//they are reused in the same order that they were originally used.
	//* * * 
	//if (slotnum == -1 && cached_rrn_flag && !extending_val) { //V3.0
	if (slotnum == -1 && cached_rrn_flag && !extending) {

		pagenum = fctpage.GetBReuseQHead();
		while (pagenum != -1) {
			baserec = BaseRecNumOnPage(pagenum);

			BufferPageHandle h = GetTableBPage(dbapi, pagenum);
			RecordDataPage p(h);

			slotnum = p.AllocateSlotWithReuse(baserec, cached_breserve);

			//OK, the page at the front of the queue has a free slot and some room
			if (slotnum != -1) {
				fctpage.IncBreused();
				break;
			}

			//No slots or no room, so take the page off the queue...
			int newqhead = p.RemoveFromReuseQueue();
			fctpage.SetBReuseQHead(newqhead, false);

			//...and try the next
			pagenum = newqhead;
		}
	}

	//Next we resort to increasing bhighpg, and use the first slot on that
	if (slotnum == -1) {
		int current_bhi = fctpage.GetBhighpg();
		if (current_bhi < file->cached_bsize - 1) {

			pagenum = fctpage.GetBhighpg() + 1;
			fctpage.IncBhighpg();

			//Get a fresh buffer page
			BufferPageHandle h = GetTableBPage(dbapi, pagenum, true);

			//And format it as a table B page with no records on it yet
			RecordDataPage p(h, cached_brecppg, cached_rrn_flag);

			baserec = BaseRecNumOnPage(pagenum);
			//slotnum = p.AllocateSlotWithoutReuse(baserec, cached_breserve, NULL); //V3.0
			slotnum = p.AllocateSlotWithoutReuse(baserec, cached_breserve, false, 0);
		}
	}

	//See above.  In RRN files we try some random pages.
	//if (slotnum == -1 && cached_rrn_flag && !extending_val) { //V3.0
	if (slotnum == -1 && cached_rrn_flag && !extending) {

		//Just using the CRT random function here
		srand(time(NULL));
		std::set<int> tried_pages;

		double factor =  file->cached_bsize;
		factor /= RAND_MAX;

		//This number, 16, is simply used because that's what M204 is reputed to use
		for (int x = 0; x < 16; x++) {
			pagenum = (int) (rand() * factor);

			//Don't try the same page again
			std::pair<std::set<int>::iterator, bool> insit = tried_pages.insert(pagenum);
			if (insit.second == false)
				continue;

			BufferPageHandle h = GetTableBPage(dbapi, pagenum);
			RecordDataPage p(h);

			baserec = BaseRecNumOnPage(pagenum);
			slotnum = p.AllocateSlotWithReuse(baserec, cached_breserve);

			//We still increment BREUSED even though the page didn't come off the queue
			if (slotnum != -1) {
				fctpage.IncBreused();
				break;
			}
		}
	}

	//Can only fail now if the max possible record number was reached, since the
	//max for BRESERVE/BRECPPG means there is always enough space for one empty record.
	if (slotnum == -1) {
		file->MarkTableBFull(dbapi);

		//Depending on the situation we'll be able to back out
		int ec = (maybe_benign) ? TXN_BENIGN_ATOM : DB_INSUFFICIENT_SPACE;
		throw Exception(ec, "File is full");
	}

	//if (extending_val) //V3.0
	if (extending)
		fctpage.IncExtnadd();
	else
		fctpage.IncMstradd();

	return baserec + slotnum;
}

//*************************************************************************************
//V3.0. Retain a function with this prototype to avoid complicating the old caller code
int DatabaseFileTableBManager::AllocateExtensionRecordExtent
(DatabaseServices* dbapi, bool maybe_benign, const FieldValue* extending_val) 
{
	short extending_len = 10;
	if (!extending_val->CurrentlyNumeric())
		extending_len = 3 + extending_val->StrLen();

	return AllocateNewRecordSlot(dbapi, maybe_benign, true, extending_len);
}

//*************************************************************************************
void DatabaseFileTableBManager::RestoreDeletedPrimaryExtent
(DatabaseServices* dbapi, int tborecnum)
{
	FCTPage_B fctpage(dbapi, file->fct_buff_page);

	int bpagenum = BPageNumFromRecNum(tborecnum);
	BufferPageHandle h = GetTableBPage(dbapi, bpagenum);
	RecordDataPage p(h);

	short slotnum = BPageSlotFromRecNum(tborecnum);
	p.UndeleteSlot(slotnum);

	//This is a little counterintuitive, just like on M204 
	fctpage.DecMstrdel();
}

//*************************************************************************************
void DatabaseFileTableBManager::DeletePrimaryExtent
(DatabaseServices* dbapi, int recnum)
{
	int bpagenum = BPageNumFromRecNum(recnum);

	BufferPageHandle h = GetTableBPage(dbapi, bpagenum);
	RecordDataPage p(h);

	short slotnum = BPageSlotFromRecNum(recnum);
	p.DeleteEmptySlotData(slotnum);

	//MSTRDEL is increased both in a forward delete and in TBO of store.
	FCTPage_B fctpage(dbapi, file->fct_buff_page);
	fctpage.IncMstrdel();

	//Place the page on the reuse queue if it isn't there already.  
	if (p.RRN() && p.NumFreeBytes() >= cached_breuse) {
		int rqhead = fctpage.GetBReuseQHead();
		if (p.QueueForSlotReuse(rqhead))
			fctpage.SetBReuseQHead(bpagenum, true);
	}
}

//*************************************************************************************
void DatabaseFileTableBManager::DeleteExtensionRecordExtent
(DatabaseServices* dbapi, RecordDataPage* p, int extent, int primary)
{
	short slot = BPageSlotFromRecNum(extent);

	//Note the forward extension pointer (i.e. this one may be a "middle" extent)
	int next_extent = p->SlotGetExtensionPtr(slot);
	p->DeleteEmptySlotData(slot);

	//Then look for the previous extent so the two loose chain ends can be joined.
	//Since it's a singly-linked chain, we have to scan from the first extent.
	//See tech doc for discussion of double linking for this purpose.
	int chainrec = primary;
	for (;;) {
		int chainpage = BPageNumFromRecNum(chainrec);
		BufferPageHandle ch = GetTableBPage(dbapi, chainpage);
		RecordDataPage cp(ch);

		short chainslot = BPageSlotFromRecNum(chainrec);

		int chain_ern = cp.SlotGetExtensionPtr(chainslot);
		if (chain_ern == -1)
			throw Exception(DB_STRUCTURE_BUG, "Bug: Corrupt record extent chain");

		if (chain_ern == extent) {
			cp.SlotSetExtensionPtr(chainslot, next_extent);
			break;
		}

		chainrec = chain_ern;
	}

	//This goes up regardless of direction, as per M204
	FCTPage_B fctpage(dbapi, file->fct_buff_page);
	fctpage.IncExtndel();

	//Place the page on the reuse queue if it isn't there already.  
	if (p->RRN() && p->NumFreeBytes() >= cached_breuse) {
		int rqhead = fctpage.GetBReuseQHead();
		if (p->QueueForSlotReuse(rqhead))
			fctpage.SetBReuseQHead(BPageNumFromRecNum(extent), true);
	}
}

//*************************************************************************************
//V3.01 Used during Increase and Create
void DatabaseFileTableBManager::ValidateBsize(DatabaseServices* dbapi, _int64 bsize, _int64 brecppg)
{
	//At create time we don't have a value cached yet
	if (brecppg == -1)
		brecppg = cached_brecppg;

	if (bsize * brecppg > dbapi->GetParmMAXRECNO())
		throw Exception(DB_BAD_CREATE_PARM, 
			"BSIZE * BRECPPG must not exceed the MAXRECNO parameter value");
}


//*************************************************************************************
//TABLEB command
//*************************************************************************************
void DatabaseFileTableBManager::Dump(DatabaseServices* dbapi, BB_OPDEVICE* op, 
										bool list, bool reclen, int pagefrom, int pageto)
{
	//Decided (as with ANALYZE) to allow this if file is broken for diagnostic purposes
//	file->CheckFileStatus(false, false, true, false);

	CFRSentry s(dbapi, file->cfr_direct, BOOL_SHR);

	_int64 tot_free_space = 0;
	_int64 tot_free_slots = 0;
//	_int64 tot_reclen = 0;
	int tot_pages = 0;

	FCTPage_B fctpage(dbapi, file->fct_buff_page);

	if (pagefrom == -1)
		pagefrom = 0;
	else if (pagefrom > fctpage.GetBhighpg())
		throw Exception(DB_API_BAD_PARM, 
			std::string("Invalid table B page number: ").append(util::IntToString(pagefrom)));

	if (pageto == -1)
		pageto = fctpage.GetBhighpg();
	else if (pageto > fctpage.GetBhighpg())
		throw Exception(DB_API_BAD_PARM, 
			std::string("Invalid table B page number: ").append(util::IntToString(pageto)));

	//This is because the current method of determining the number of primary record
	//extents is to just use MSTRADD-MSTRDEL.  A partial-file figure could be arrived
	//at if we checked each record as we went and determined if it was a primary or
	//extension record.  Unfortunately at the moment the flag that would be required
	//to do that is not maintained (although it would be fairly straightforward to do).
	if (reclen) {
		if (pagefrom != 0 || pageto != fctpage.GetBhighpg())
			throw Exception(DB_API_BAD_PARM, 
				"Record length option is invalid unless the whole file is scanned");
	}

	//-------------------------
	//Loop over the range of pages
	for (int pagenum = pagefrom; pagenum <= pageto; pagenum++) {
		if (list && pagenum == pagefrom)
			op->WriteLine("Page no.    Free space    Free slots");

		BufferPageHandle h = GetTableBPage(dbapi, pagenum);
		RecordDataPage p(h);

		//Note info off page
		int page_free_space = p.NumFreeBytes();
		int page_free_slots = p.MapFreeSlots();

		tot_pages++;
		tot_free_space += page_free_space;
		tot_free_slots += page_free_slots;

		//Print this info if list option requested
		if (list) {
			op->Write(util::PadRight(util::IntToString(pagenum), ' ', 12));
			op->Write(util::PadRight(util::IntToString(page_free_space), ' ', 14));
			op->WriteLine(util::IntToString(page_free_slots));
		}
	}

	//-------------------------
	//Calculate averages etc.
	int i_ave_free_space = (tot_pages == 0) ? 0 : tot_free_space / tot_pages;
	int i_ave_free_slots = (tot_pages == 0) ? 0 : tot_free_slots / tot_pages;

	int i_ave_reclen = 0;
	if (reclen) {
		//As with M204 this will be inaccurate (or at least confusing) after a dirty delete
		int tot_master_records = fctpage.GetMstradd() - fctpage.GetMstrdel();

		_int64 tot_page_space = DBPAGE_SIZE - DBP_SAR_SLOTAREA - (fctpage.GetBrecppg() * 2);
		tot_page_space *= tot_pages; //NB. separate to ensure 64-bit multiply
		_int64 tot_used_space = tot_page_space - tot_free_space;

		if (tot_master_records > 0) 
			i_ave_reclen = tot_used_space / tot_master_records;
	}

	std::string s_ave_free_space = util::IntToString(i_ave_free_space);
	std::string s_ave_free_slots = util::IntToString(i_ave_free_slots);
	std::string s_tot_pages = util::IntToString(tot_pages);
	std::string s_brecppg = util::IntToString(cached_brecppg);
	std::string s_breserve = util::IntToString(cached_breserve);
	std::string s_ave_reclen = util::IntToString(i_ave_reclen);

	//This is beyond the call of duty really
	size_t widest = s_ave_free_space.length();
	if (s_ave_free_slots.length() > widest)
		widest = s_ave_free_slots.length();
	if (s_tot_pages.length() > widest)
		widest = s_tot_pages.length();
	if (s_brecppg.length() > widest)
		widest = s_brecppg.length();
	if (s_breserve.length() > widest)
		widest = s_breserve.length();
	if (s_ave_reclen.length() > widest)
		widest = s_ave_reclen.length();
	
	//-------------------------
	//Print averages etc.
	op->Write(util::PadLeft(s_ave_free_space, ' ', widest));
	op->WriteLine("  Average free space per page");
	
	op->Write(util::PadLeft(s_ave_free_slots, ' ', widest));
	op->WriteLine("  Average free slots per page");
	
	op->Write(util::PadLeft(s_tot_pages, ' ', widest));
	op->WriteLine("  Number of pages processed");
	
	op->Write(util::PadLeft(s_brecppg, ' ', widest));
	op->WriteLine("  BRECPPG - Table B records per page");
	
	op->Write(util::PadLeft(s_breserve, ' ', widest));
	op->WriteLine("  BRESERVE - Table B reserved space per page");
	
	if (reclen) {
		op->Write(util::PadLeft(s_ave_reclen, ' ', widest));
		op->WriteLine("  Average record length");
	}
}





//*************************************************************************************
//This object maintains some of the file parameters.  I just like it better this way.
//*************************************************************************************
void DatabaseFileTableBManager::CacheParms()
{
	FCTPage_B fctpage(NULL, file->fct_buff_page);

	cached_brecppg = fctpage.GetBrecppg();
	cached_breserve = fctpage.GetBreserve();
	cached_breuse = fctpage.GetBreuse();
	cached_rrn_flag = file->IsRRN();
}

//*************************************************************************************
std::string DatabaseFileTableBManager::ViewParm
(SingleDatabaseFileContext* context, const std::string& parmname)
{
	DatabaseServices* dbapi = context->DBAPI();

	//Parms that require FILE EXCL to be changed, so no lock required and use cached vals
	if (parmname == "BRECPPG") 
		return util::IntToString(cached_brecppg);
	if (parmname == "BRESERVE") 
		return util::IntToString(cached_breserve);
	if (parmname == "BREUSE") 
		return util::IntToString(cached_breuse);

	//Parms requiring lock
	CFRSentry cs(dbapi, file->cfr_direct, BOOL_SHR);
	FCTPage_B fctpage(dbapi, file->fct_buff_page);

	if (parmname == "BHIGHPG") 
		return util::IntToString(fctpage.GetBhighpg());
	if (parmname == "BQLEN") 
		return util::IntToString(fctpage.GetBqlen());

	if (parmname == "BREUSED") 
		return util::Int64ToString(fctpage.GetBreused());
	if (parmname == "MSTRADD") 
		return util::Int64ToString(fctpage.GetMstradd());
	if (parmname == "MSTRDEL") 
		return util::Int64ToString(fctpage.GetMstrdel());
	if (parmname == "EXTNADD") 
		return util::Int64ToString(fctpage.GetExtnadd());
	if (parmname == "EXTNDEL") 
		return util::Int64ToString(fctpage.GetExtndel());

	throw "surely shome mishtake";
}

//*************************************************************************************
void DatabaseFileTableBManager::ResetParm
(SingleDatabaseFileContext* context, const std::string& parmname, int inew)
{
	DatabaseServices* dbapi = context->DBAPI();

	file->StartNonBackoutableUpdate(context, true);

	FCTPage_B fctpage(dbapi, file->fct_buff_page);

	if (parmname == "BRESERVE")
		fctpage.SetBreserve(inew);
	else if (parmname == "BREUSE")
		fctpage.SetBreuse(inew);
	else
		throw "surely shome mishtake";

	CacheParms();
}




} //close namespace


