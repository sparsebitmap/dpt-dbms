
#include "stdafx.h"

#include "dbf_tabled.h"

//Utils
#include "dataconv.h"
//API tiers
#include "dbfile.h"
#include "dbctxt.h"
#include "dbserv.h"
#include "update.h"
#include "page_f.h" //#include "page_F.h" : V2.24 case is less interchangeable on *NIX - Roger M.
#include "page_i.h" //#include "page_I.h"
#include "page_l.h" //V3.0
#include "page_v.h" //#include "page_V.h"
#include "page_x.h" //#include "page_X.h"
//Diagnostics
#include "except.h"
#include "msg_core.h"
#include "msg_db.h"

namespace dpt {

//*************************************************************************************
BufferPageHandle DatabaseFileTableDManager::GetTableDPage
(DatabaseServices* dbapi, int p, bool fresh)
{
	FCTPage_D fctpage(dbapi, file->fct_buff_page);

	//NB no lock required here, as DEFRAG etc that change the LPM take file EXCL
	if (p < 0)
		throw Exception(DB_BAD_PAGE_NUMBER, "Bug: Negative table D page number");
	if (p > fctpage.GetDhighpg())
		throw Exception(DB_BAD_PAGE_NUMBER, "Bug: Table D page number exceeds DHIGHPG");

	return fctpage.GetAbsoluteDPageHandle(dbapi, file->buffapi, p, fresh);
}

//***********************************************************************************
int DatabaseFileTableDManager::AllocatePage
(DatabaseServices* dbapi, char pagetype, bool maybe_benign)
{
	LockingSentry ls(&heap_lock);
	FCTPage_D fctpage(dbapi, file->fct_buff_page);

	//Only if TBO is active can we use the reserved DPGSRES pages
	int davail = fctpage.GetDsize() - fctpage.GetDpgsused();
	if (dbapi->GetUU()->IsCurrentlyBackingOut())
		davail += cached_dpgsres;

	int allocated_page = -1;

	if (davail > 0) {
		//First we use any pages that have been freed up by other structures
		int dqhead = fctpage.GetDReuseQHead();
		if (dqhead != -1) {
			allocated_page = dqhead;

			//Remove the page from the queue (stack)
			BufferPageHandle bh = GetTableDPage(dbapi, dqhead);
			ReusableTableDPage px(bh);

			int dqnext = px.GetDReuseQPtr();
			fctpage.SetDReuseQHead(dqnext);
		}

		if (allocated_page != -1)
			fctpage.DecDpgsX();

		//Otherwise take a fresh page from the top of table D if one's available
		else {
			if (fctpage.GetDhighpg() != (file->cached_dsize - 1)) {
				fctpage.IncDhighpg();
				allocated_page = fctpage.GetDhighpg();
			}
		}
	}

	//In some circumstances nothing's been touched yet so we could back out
	if (allocated_page == -1) {
		file->MarkTableDFull(dbapi);

		int ec = (maybe_benign) ? TXN_BENIGN_ATOM : DB_INSUFFICIENT_SPACE;
		throw Exception(ec, "File is full");
	}

	fctpage.IncDpgsused();

	//More precision for the enhanced "VIEW TABLES" command - interesting info
	switch (pagetype) {
		case 'A': fctpage.IncDpgsA(); break;
		case 'E': fctpage.IncDpgsE(); break;
		case 'I': fctpage.IncDpgsI(); break;
		case 'L': fctpage.IncDpgsL(); break; //V3.0
		case 'M': fctpage.IncDpgsM(); break;
		case 'P': fctpage.IncDpgsP(); break;
		case 'T': fctpage.IncDpgsT(); break;
		case 'V': fctpage.IncDpgsV(); break;
		case 'X': throw Exception(DB_ALGORITHM_BUG, "Bug: Allocating fresh X page?!");
	}

	return allocated_page;
}

//***********************************************************************************
void DatabaseFileTableDManager::ReturnPage_S
(DatabaseServices* dbapi, int returned_page, BufferPageHandle* pbhin)
{
	LockingSentry ls(&heap_lock);

	BufferPageHandle bhlocal;
	BufferPageHandle& bh = bhlocal;

	//The caller may have supplied just a page number or the handle of a buffer
	if (pbhin)
		bh = *pbhin;
	else
		//If page number, get the page into a buffer now
		bhlocal = GetTableDPage(dbapi, returned_page);

	ReusableTableDPage px(bh);

	FCTPage_D fctpage(dbapi, file->fct_buff_page);

	int old_dqhead = fctpage.GetDReuseQHead();
	char old_pagetype = px.SetDReuseQPtr(old_dqhead);

	//The returned page becomes the new front of the queue - i.e. it's a stack really
	fctpage.SetDReuseQHead(returned_page);
	fctpage.IncDpgsX();

	fctpage.DecDpgsused();

	//Decrement appropriate page type stat too - the reverse of allocate above
	switch (old_pagetype) {
		case 'A': fctpage.DecDpgsA(); break;
		case 'E': fctpage.DecDpgsE(); break;
		case 'I': fctpage.DecDpgsI(); break;
		case 'L': fctpage.DecDpgsL(); break; //V3.0
		case 'M': fctpage.DecDpgsM(); break;
		case 'P': fctpage.DecDpgsP(); break;
		case 'T': fctpage.DecDpgsT(); break;
		case 'V': fctpage.DecDpgsV(); break;
		case 'X': throw Exception(DB_ALGORITHM_BUG, "Bug: Returning X page?!");
	}
}






//****************************************************************************************
//Inverted lists in "array" form.
//-------------------------------
//Relevant info used below:
//
//- Page type                       : V (slot/record type)
//- Mapper class                    : InvertedIndexListPage
//- File parameter for current page : DACTIVE
//- Page reserve space              : DRESERVE
//- Reuse queue                     : fct.GetListQHead()
//****************************************************************************************
BufferPageHandle DatabaseFileTableDManager::AllocateRecNumList
(DatabaseServices* dbapi, short num_entries, int& pagenum, short& slot, bool maybe_benign)
{
	//List entries take up 2 bytes each
	short insertion_size = num_entries * 2;
	short expansion_space = DBPAGE_SIZE * cached_dreserve / 100;
	BufferPageHandle bh;

	FCTPage_D fctpage(dbapi, file->fct_buff_page);

	//First try the current DACTIVE
	pagenum = fctpage.GetDactive();

	if (pagenum != -1) {
		bh = GetTableDPage(dbapi, pagenum);
		InvertedIndexListPage p(bh);

		slot = p.AllocateSlot(expansion_space, insertion_size);
		if (slot == -1)
			pagenum = -1;
	}

	//Next try the list page reuse queue if possible
	if (pagenum == -1) {
		pagenum = fctpage.GetListQHead();

		//Move along the queue as required
		while (pagenum != -1) {
			bh = GetTableDPage(dbapi, pagenum);
			InvertedIndexListPage p(bh);
		
			slot = p.AllocateSlot(expansion_space, insertion_size);
			if (slot != -1) {
				fctpage.SetDactive(pagenum);
				break;
			}

			//As with table B, failure here means the page comes off the queue
			int newqhead = p.RemoveFromReuseQueue();
			fctpage.SetListQHead(newqhead);

			//Try the next
			pagenum = newqhead;
		}
	}

	//The last resort is to create a brand new list page and make it DACTIVE
	if (pagenum == -1) {
		pagenum = AllocatePage(dbapi, 'V', maybe_benign);
		fctpage.SetDactive(pagenum);

		bh = GetTableDPage(dbapi, pagenum, true);
		InvertedIndexListPage pv(bh, true);

		//This can't fail now as it's an empty page
		slot = pv.AllocateSlot(expansion_space, insertion_size);
	}

	return bh;
}

//****************************************************************************************
void DatabaseFileTableDManager::DeleteRecNumList(BufferPageHandle& bh, int pagenum, short slot)
{
	InvertedIndexListPage p(bh);

	//Will always have been cleared by the time we get to here
	p.DeleteEmptySlotData(slot);

	FCTPage_D fctpage(bh.DBAPI(), file->fct_buff_page);

	//An entirely empty page can go back to the heap
	if (p.NumSlotsInUse() == 0) {

		//See technical note on record slots (part 4) for comments on this.  Remove from
		//the listpage queue chain first to save pain later.
		if (p.IsOnReuseQueue()) {
			int qnext = p.GetReuseQPointer();
			int qpage = fctpage.GetListQHead();

			//A common case
			if (qpage == pagenum)
				fctpage.SetListQHead(qnext); //point past the empty page

			else {
				for (;;) {
					BufferPageHandle bhq = GetTableDPage(bh.DBAPI(), qpage);
					InvertedIndexListPage pq(bhq);
				
					qpage = pq.GetReuseQPointer();
					if (qpage == pagenum) {
						pq.SetReuseQPointer(qnext); //point past the empty page
						break;
					}
				}
			}
		}

		ReturnPage(bh, pagenum);

		//Might also be DACTIVE - e.g. often at the start of a backout
		if (pagenum == fctpage.GetDactive())
			fctpage.SetDactive(-1);

		return;
	}

	//Otherwise maybe it can go on the V reuse queue.  See tech doc re. trigger level
	short freespace_trigger_level = DBPAGE_SIZE * cached_dreserve / 100;

	if (p.NumFreeBytes() >= freespace_trigger_level) {
		int qhead = fctpage.GetListQHead();
		if (p.QueueForSlotReuse(qhead))
			fctpage.SetListQHead(pagenum);
	}
}








//****************************************************************************************
//Inverted list master records (ILMRs)
//------------------------------------
//Relevant info used below:
//
//- Page type                       : I (slot/record type)
//- Mapper class                    : InvertedListMasterRecordPage
//- File parameter for current page : ILACTIVE
//- Page reserve space              : ILMR_RESERVE_BYTES (half a page - see tech docs)
//- Reuse queue                     : fct.GetILMRQHead()
//****************************************************************************************
BufferPageHandle DatabaseFileTableDManager::AllocateILMR
(DatabaseServices* dbapi, short num_segribs, int& pagenum, short& slot, bool maybe_benign)
{
	//RIBs take up 8 bytes each
	short insertion_size = num_segribs * 8;
	short expansion_space = ILMR_RESERVE_BYTES;
	BufferPageHandle bh;

	FCTPage_D fctpage(dbapi, file->fct_buff_page);

	//First try the current ILACTIVE
	pagenum = fctpage.GetIlactive();

	if (pagenum != -1) {
		bh = GetTableDPage(dbapi, pagenum);
		InvertedListMasterRecordPage p(bh);

		slot = p.AllocateSlot(expansion_space, insertion_size);
		if (slot == -1)
			pagenum = -1;
	}

	//Next try the ILMR page reuse queue if possible
	if (pagenum == -1) {
		pagenum = fctpage.GetILMRQHead();

		//Move along the queue as required
		while (pagenum != -1) {
			bh = GetTableDPage(dbapi, pagenum);
			InvertedListMasterRecordPage p(bh);
		
			slot = p.AllocateSlot(expansion_space, insertion_size);
			if (slot != -1) {
				fctpage.SetIlactive(pagenum);
				break;
			}

			//As with table B, failure here means the page comes off the queue
			int newqhead = p.RemoveFromReuseQueue();
			fctpage.SetILMRQHead(newqhead);

			//Try the next
			pagenum = newqhead;
		}
	}

	//The last resort is to create a brand new list page
	if (pagenum == -1) {
		pagenum = AllocatePage(dbapi, 'I', maybe_benign);
		fctpage.SetIlactive(pagenum);

		bh = GetTableDPage(dbapi, pagenum, true);
		InvertedListMasterRecordPage pi(bh, true);

		//This can't fail now as it's an empty page
		slot = pi.AllocateSlot(expansion_space, insertion_size);
	}

	return bh;
}

//****************************************************************************************
void DatabaseFileTableDManager::DeleteILMR(BufferPageHandle& bh, int pagenum, short slot)
{
	InvertedListMasterRecordPage p(bh);

	//Will always have been cleared by the time we get to here
	p.DeleteEmptySlotData(slot);

	FCTPage_D fctpage(bh.DBAPI(), file->fct_buff_page);

	//An entirely empty page can go back to the heap
	if (p.NumSlotsInUse() == 0) {

		//Same comment as in the listpage function above.
		if (p.IsOnReuseQueue()) {
			int qnext = p.GetReuseQPointer();
			int qpage = fctpage.GetILMRQHead();

			//A common case
			if (qpage == pagenum)
				fctpage.SetILMRQHead(qnext); //point past the empty page

			else {
				for (;;) {
					BufferPageHandle bhq = GetTableDPage(bh.DBAPI(), qpage);
					InvertedListMasterRecordPage pq(bhq);
				
					qpage = pq.GetReuseQPointer();
					if (qpage == pagenum) {
						pq.SetReuseQPointer(qnext); //point past the empty page
						break;
					}
				}
			}
		}

		ReturnPage(bh, pagenum);

		//Might also be ILACTIVE - e.g. often at the start of a backout
		if (pagenum == fctpage.GetIlactive())
			fctpage.SetIlactive(-1);

		return;
	}

	//Otherwise maybe it can go on the I reuse queue.  See tech doc re. trigger level
	short freespace_trigger_level = ILMR_RESERVE_BYTES;

	if (p.NumFreeBytes() >= freespace_trigger_level) {
		int qhead = fctpage.GetILMRQHead();
		if (p.QueueForSlotReuse(qhead))
			fctpage.SetILMRQHead(pagenum);
	}
}








//****************************************************************************************
//V3.0. BLOBs
//-----------
//Relevant info used below:
//
//- Page type                       : L (slot/record type)
//- Mapper class                    : BLOBPage
//- File parameter for current page : EACTIVE
//- Page reserve space              : no reserve, but require BLOB_SMALLEST_EXTENT (1/2 page)
//- Reuse queue                     : fct.GetBLOBQHead()
//****************************************************************************************
BufferPageHandle DatabaseFileTableDManager::StoreBLOBExtent
(DatabaseServices* dbapi, const char** ppblobdata, int& blob_remaining, 
 int& epage, short& eslot, bool maybe_benign)
{
	FCTPage_D fctpage(dbapi, file->fct_buff_page);

	//First try the current EACTIVE
	epage = fctpage.GetEactive();

	if (epage != -1) {
		BufferPageHandle bh = GetTableDPage(dbapi, epage);
		BLOBPage p(bh);

		eslot = p.AllocateSlotAndStoreBLOB(ppblobdata, blob_remaining);
		if (eslot != -1)
			return bh;
	}

	//Next try the BLOB page reuse queue if possible
	epage = fctpage.GetBLOBQHead();

	//Move along the queue as required
	while (epage != -1) {
		BufferPageHandle bh = GetTableDPage(dbapi, epage);
		BLOBPage p(bh);

		eslot = p.AllocateSlotAndStoreBLOB(ppblobdata, blob_remaining);

		if (eslot != -1) {
			fctpage.SetEactive(epage);
			return bh;
		}

		//As with table B, failure here means the page comes off the queue
		int newqhead = p.RemoveFromReuseQueue();
		fctpage.SetBLOBQHead(newqhead);

		//Try the next
		epage = newqhead;
	}

	//The last resort is to create a brand new BLOB page
	epage = AllocatePage(dbapi, 'L', maybe_benign);
	fctpage.SetEactive(epage);

	BufferPageHandle bh = GetTableDPage(dbapi, epage, true);
	BLOBPage p(bh, true);

	//This can't fail now as it's an empty page
	eslot = p.AllocateSlotAndStoreBLOB(ppblobdata, blob_remaining);
	return bh;
}

//****************************************************************************************
void DatabaseFileTableDManager::DeleteBLOB(DatabaseServices* dbapi, int epage, short eslot)
{
	//Might be zero or several extents.  Unlike ILMRs etc. we delete the whole thing all 
	//in one.  Keeps it simple as there is never any modification with BLOB extents.
	while (epage != -1) {
		BufferPageHandle bh = GetTableDPage(dbapi, epage);
		BLOBPage p(bh);

		//Get these before we delete
		int extension_page;
		short extension_slot;
		p.SlotGetExtensionPointers(eslot, extension_page, extension_slot);

		//No need to delete extent items first, they have no substructure
		p.DeleteSlotData(eslot);

		FCTPage_D fctpage(bh.DBAPI(), file->fct_buff_page);

		//An entirely empty page can go back to the heap
		if (p.NumSlotsInUse() == 0) {

			//Same comment as in the listpage function above.
			if (p.IsOnReuseQueue()) {
				int qnext = p.GetReuseQPointer();
				int qpage = fctpage.GetBLOBQHead();

				//A common case
				if (qpage == epage)
					fctpage.SetBLOBQHead(qnext); //point past the empty page

				else {
					for (;;) {
						BufferPageHandle bhq = GetTableDPage(bh.DBAPI(), qpage);
						BLOBPage pq(bhq);
					
						qpage = pq.GetReuseQPointer();
						if (qpage == epage) {
							pq.SetReuseQPointer(qnext); //point past the empty page
							break;
						}
					}
				}
			}

			ReturnPage(bh, epage);

			//Might also be EACTIVE (e.g. often at the start of a backout)
			if (epage == fctpage.GetEactive())
				fctpage.SetEactive(-1);
		}

		//Otherwise maybe it can go on the L reuse queue.
		else {
			short freespace_trigger_level = BLOB_SMALLEST_EXTENT;

			if (p.NumFreeBytes() >= freespace_trigger_level) {
				int qhead = fctpage.GetBLOBQHead();
				if (p.QueueForSlotReuse(qhead))
					fctpage.SetBLOBQHead(epage);
			}
		}

		//Finally set these up for deleting the next extent
		epage = extension_page;
		eslot = extension_slot;
	}
}










//*************************************************************************************
//This object maintains some of the file parameters.  I just like it better this way.
//They are all maintained by this class.
//*************************************************************************************
std::string DatabaseFileTableDManager::ViewParm
(SingleDatabaseFileContext* context, const std::string& parmname)
{
	DatabaseServices* dbapi = context->DBAPI();

	//Parms that require FILE EXCL to be changed, so no lock required and use cached vals
	if (parmname == "DPGSRES") 
		return util::IntToString(cached_dpgsres);
	if (parmname == "DRESERVE") 
		return util::IntToString(cached_dreserve);

	LockingSentry ls(&heap_lock);
	FCTPage_D fctpage(dbapi, file->fct_buff_page);

	if (parmname == "DACTIVE") 
		return util::IntToString(fctpage.GetDactive());
	if (parmname == "DHIGHPG") 
		return util::IntToString(fctpage.GetDhighpg());
	if (parmname == "DPGSUSED") 
		return util::IntToString(fctpage.GetDpgsused());
	if (parmname == "ILACTIVE") 
		return util::IntToString(fctpage.GetIlactive());

	//Custom heap stats
	if (parmname == "DPGS_1") 
		return util::IntToString(fctpage.GetDpgsA());
	if (parmname == "DPGS_2") 
		return util::IntToString(fctpage.GetDpgsE());
	if (parmname == "DPGS_3") 
		return util::IntToString(fctpage.GetDpgsP());
	if (parmname == "DPGS_4") 
		return util::IntToString(fctpage.GetDpgsI());
	if (parmname == "DPGS_5") 
		return util::IntToString(fctpage.GetDpgsV());
	if (parmname == "DPGS_6") 
		return util::IntToString(fctpage.GetDpgsM());
	if (parmname == "DPGS_7") 
		return util::IntToString(fctpage.GetDpgsT());
	if (parmname == "DPGS_8") 
		return util::IntToString(fctpage.GetDpgsL()); //V3.0
	if (parmname == "DPGS_X") 
		return util::IntToString(fctpage.GetDpgsX());

	throw "surely shome mishtake";
}

//*************************************************************************************
void DatabaseFileTableDManager::ResetParm
(SingleDatabaseFileContext* context, const std::string& parmname, int inew)
{
	DatabaseServices* dbapi = context->DBAPI();

	file->StartNonBackoutableUpdate(context, true);

	FCTPage_D fctpage(dbapi, file->fct_buff_page);

	if (parmname == "DPGSRES") {
		if (inew > fctpage.GetDsize() - fctpage.GetDpgsused())
			throw Exception(PARM_PARMCONFLICT, 
				"There is insufficient table D space for the requested new DPGSRES");

		fctpage.SetDpgsres(inew);
	}
	else if (parmname == "DRESERVE")
		fctpage.SetDreserve(inew);
	else
		throw "surely shome mishtake";

	CacheParms();
}

//*************************************************************************************
void DatabaseFileTableDManager::CacheParms()
{
	FCTPage_D fctpage(NULL, file->fct_buff_page);
	cached_dpgsres = fctpage.GetDpgsres();
	cached_dreserve = fctpage.GetDreserve();
}


} //close namespace


