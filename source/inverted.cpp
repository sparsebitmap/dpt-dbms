
#include "stdafx.h"

#include "inverted.h"

//Utils
#include "dataconv.h"
//API Tiers
#include "bmset.h"
#include "dbctxt.h"
#include "du1step.h"
#include "dbf_tabled.h"
#include "dbfile.h"
#include "pageixval.h"
#include "page_v.h" //#include "page_V.h" : V2.24 case is less interchangeable on *NIX - Roger M.
#include "page_m.h" //#include "page_M.h"
#include "page_i.h" //#include "page_I.h"
//Diagnostics
#include "except.h"
#include "msg_db.h"

namespace dpt {


//****************************************************************************************
InvertedListAPI::InvertedListAPI(SingleDatabaseFileContext* c, BufferPageHandle* b, short o)
: context(c), index_valarea_buffpage(b), index_value_page_offset(o), num_segribs(0) 
{
	//These are used all over the code below - retrieve once now for readability
	file = context->GetDBFile();
	tdmgr = file->GetTableDMgr();
	ixmgr = file->GetIndexMgr();
	dbapi = context->DBAPI();

	CacheMasterRecordPage();
}

//****************************************************************************************
void InvertedListAPI::Copy(const InvertedListAPI& from) 
{
	//Memberwise copy unfortunately required because of this handle
	ilmr_buffpage = from.ilmr_buffpage;

	context = from.context;
	dbapi = from.dbapi;
	tdmgr = from.tdmgr;
	ixmgr = from.ixmgr;
	file = from.file;

	index_valarea_buffpage = from.index_valarea_buffpage; 
	index_value_page_offset = from.index_value_page_offset;
	
	ilmr_page_slot = from.ilmr_page_slot;
	ilmr_page_number = from.ilmr_page_number;

	ilmr_page_offset = from.ilmr_page_offset;
	num_segribs = from.num_segribs;
}

//****************************************************************************************
inline void InvertedListAPI::SetILMRInfo(int pagenum, short listnum) {
	AnyIndexValueEntryPage piv(*index_valarea_buffpage);
	piv.SetInvertedListInfo(index_value_page_offset, pagenum, listnum);
	ilmr_page_number = pagenum;
	ilmr_page_slot = listnum;}





//****************************************************************************************
void InvertedListAPI::CacheMasterRecordPage(BufferPageHandle* newbuff) const
{
	//Refer to the index value area (btree leaf) to get this info
	AnyIndexValueEntryPage piv(*index_valarea_buffpage);
	ilmr_page_number = piv.GetILMRPageNumber(index_value_page_offset);
	ilmr_page_slot = piv.GetILMRPageSlot(index_value_page_offset);

	//A couple of situations where there will be no ILMR
	if (!IndexValueValid())
		return;
	if (UniqueValInFile())
		return;

	if (newbuff)
		ilmr_buffpage = *newbuff;

	else if (!ilmr_buffpage.IsEnabled())
		ilmr_buffpage = tdmgr->GetTableDPage(dbapi, ilmr_page_number);

	//Cacheing the length of the ILMR once here saves continual scans for the next
	//ILMR on the page (since we always need to avoid reading past the end of the record).
	//Also cache the page offset, as the index is locked for the life of this object.
	InvertedListMasterRecordPage pi(ilmr_buffpage);
	short ilmr_reclen;
	pi.FindILMROnPage(ilmr_page_slot, ilmr_page_offset, ilmr_reclen);
	num_segribs = ilmr_reclen / 8;
}






//****************************************************************************************
void InvertedListAPI::GetSegRIBInfo(short ribix, SegmentRIB& rib) const
{
	InvertedListMasterRecordPage pi(ilmr_buffpage);

	short rib_page_offset = ilmr_page_offset + (ribix * 8);
	pi.PageGetSegRIBInfo
		(rib_page_offset, rib.seg_num, rib.list_pageslot, rib.list_page);
}

//****************************************************************************************
void InvertedListAPI::SetSegRIBInfo(short ribix, const SegmentRIB& rib)
{
	InvertedListMasterRecordPage pi(ilmr_buffpage);

	short rib_page_offset = ilmr_page_offset + (ribix * 8);
	pi.PageSetSegRIBInfo(rib_page_offset, rib.list_pageslot, rib.list_page);
}

//****************************************************************************************
void InvertedListAPI::InsertSegRIB
(short ribix, const SegmentRIB& newrib, bool maybe_benign, bool moving)
{
	InvertedListMasterRecordPage pi(ilmr_buffpage);

	//Is there room?
	short nfb = pi.NumFreeBytes();
	if (nfb >= 8) {

		//Yes.  RIBs are kept in segment order, so calculate the insert position within
		//the current array simply from the fact that each RIB is 8 bytes long.
		short new_rib_page_offset = ilmr_page_offset + (ribix * 8);

		pi.InsertSegRIB(ilmr_page_slot, new_rib_page_offset, newrib.seg_num, 
							newrib.list_pageslot, newrib.list_page);

		num_segribs++;
		if (!moving)
			file->IncStatILSADD(dbapi);

		return;
	}

	//No, so move the ILMR to a page that does have room - probably the current ILACTIVE.
	//* * * NOTE * * *
	//Currently only allowing ILMRs to fill one page each.  This would mean 800ish 
	//segments with the value, and files can in theory go up to 32000 segments, so 
	//this is definitely a potential problem in the future, but during the initial 
	//development I didn't think it was worth building an ILMR extension capability.
	//Perfectly doable though - just spill to another page and link.
	//* * * NOTE * * *
	if (pi.NumSlotsInUse() == 1) {
		throw Exception(DB_STRUCTURE_BUG,
			"Bug: ILMR has filled a page - too many segments with value! "
			"Can we can fix it?  Yes we can!");
	}

	//Collect up the RIB info off the current ILMR
	std::vector<SegmentRIB> temp(num_segribs);
	for (short x = 0; x < num_segribs; x++)
		GetSegRIBInfo(x, temp[x]);

	//Create new ILMR
	int new_ilmr_page;
	short new_ilmr_slot;
	BufferPageHandle newbh = tdmgr->AllocateILMR
		(dbapi, num_segribs+1, new_ilmr_page, new_ilmr_slot, maybe_benign);

	//Delete current ILMR
	while (num_segribs > 0)
		RemoveSegRIB(0, true);

	//Adjust pointers on btree leaf page to indicate the new ILMR
	SetILMRInfo(new_ilmr_page, new_ilmr_slot);
	CacheMasterRecordPage(&newbh);

	//Add back the RIB info we collected above
	for (size_t y = 0; y < temp.size(); y++)
		InsertSegRIB((short)y, temp[y], false, true);

	file->IncStatILMRMOVE(dbapi);

	//Finally the new one we wanted to insert all along - recursive call this func.
	InsertSegRIB(ribix, newrib, false);
}

//****************************************************************************************
void InvertedListAPI::RemoveSegRIB(short ribix, bool moving)
{
	InvertedListMasterRecordPage pi(ilmr_buffpage);

	short rib_page_offset = ilmr_page_offset + (ribix * 8);

	pi.DeleteSegRIB(ilmr_page_slot, rib_page_offset);
	num_segribs--;

	if (!moving)
		file->IncStatILSDEL(dbapi);

	//Last one - remove the ILMR altogether
	if (num_segribs == 0) {
		tdmgr->DeleteILMR(ilmr_buffpage, ilmr_page_number, ilmr_page_slot);

		SetILMRInfo(-1, INDEX_INVALID);
		ilmr_buffpage.Release();

		if (!moving)
			file->IncStatILMRDEL(dbapi);
	}
}







//****************************************************************************************
//Used when assembling the set for use in a find, during FILE RECORDS, and during ANALYZE.
//V2.14 - also in loads.  Loads are the reason for the final parameter which tells
//the function to selectively retrieve or delete inverted lists.  The idea is that
//the load process (caller) is happy to leave lists in place if they will be unaffected,
//often making it much faster (particularly the new one-step process).
//3.0. See also Unload later which is pretty much a stripped down version of this.
//****************************************************************************************
bool InvertedListAPI::Iterate(BitMappedFileRecordSet** ppoutset, 
InvertedListAnalyze1Info* info, bool deletelists, const BitMappedFileRecordSet* parm4,
std::vector<short>* loading)
{
	//V2.14. Jan 09.  Parm 4 is dual purpose now, determined by presence of parm 5.
	const BitMappedFileRecordSet* find_restricting_set = NULL;
	const BitMappedFileRecordSet* load_set = NULL;
	std::vector<short>* ribixes_to_delete = loading;
	if (loading)
		load_set = parm4;
	else
		find_restricting_set = parm4;

	//V2.03.  Set might be passed in.
	bool outset_created_here = false;

	try {

		//-----------------------------------------------------------------------
		//Simple case if there's only one record with the value in the whole file
		if (UniqueValInFile()) {
			int absrec = UniqueValRecNum();

			//During a find we can ignore records already eliminated
			if (find_restricting_set && !find_restricting_set->ContainsAbsRecNum(absrec))
				return outset_created_here;

			//Add to result set if requested.  NB in a load, always return this as
			//part of the set, since even though it's unique now, it won't be afterwards.
			if (ppoutset) {

				CreateOutputSetNowIfNotDoneYet(context, ppoutset, outset_created_here);

				SegmentRecordSet_SingleRecord* segset = 
					new SegmentRecordSet_SingleRecord(absrec);

				(*ppoutset)->AppendSegmentSet(segset);
			}

			//Increment stats if requested
			if (info)
				info->val_unique++;
			
			return outset_created_here;
		}

		//-----------------------------------------------------------------------
		//Otherwise process each segment RIB in turn - there should be at least one
		//unless this is a set being filed under a fresh value.
		for (short x = 0; x < num_segribs; x++) {
			if (x == 0 && info)
				info->val_invlist++;

			SegmentRIB rib;
			GetSegRIBInfo(x, rib);

			//During a find we can ignore records in segments already eliminated
			if (find_restricting_set && !find_restricting_set->ContainsSegment(rib.seg_num))
				continue;

			//V2.14. Likewise in load, leave it if there's no overlap with the load set
			if (loading && !load_set->ContainsSegment(rib.seg_num))
				continue;

			//-----------------------------------------------------------------------
			//4 different types of ILMR...
			//-----------------------------------------------------------------------

			//-----------------------------------------------------------------------
			//A. Value is unique within its segment.  Use a special singleton set object.
			//-----------------------------------------------------------------------
			if (rib.UniqueValInSegment()) {

				//Add to result set if requested
				if (ppoutset) {
					CreateOutputSetNowIfNotDoneYet(context, ppoutset, outset_created_here);
					if (ribixes_to_delete)
						ribixes_to_delete->push_back(x);

					(*ppoutset)->AppendSegmentSet(new SegmentRecordSet_SingleRecord
						(rib.seg_num, rib.SegUniqueValRelRecNumA()));
				}

				//Increment stats if requested
				if (info) {
					info->seg_unique++;
					info->segrecs_unique++;
				}

				continue;
			}

			//-----------------------------------------------------------------------
			//B. "Semi-unique" values: Two segment-relative record numbers are holdable 
			//because they're only 2 bytes each instead of the pointer to a list/bitmap
			//page which would be 4 bytes.  (More than one record still means a bitmap 
			//page is put into the output set though).
			//-----------------------------------------------------------------------
			if (rib.SemiUniqueValInSegment()) {

				//Add to result set if requested
				if (ppoutset) {

					CreateOutputSetNowIfNotDoneYet(context, ppoutset, outset_created_here);
					if (ribixes_to_delete)
						ribixes_to_delete->push_back(x);

					SegmentRecordSet_BitMap* bmset = 
						new SegmentRecordSet_BitMap(rib.seg_num);
					bmset->Data()->Set(rib.SegUniqueValRelRecNumA());
					bmset->Data()->Set(rib.SegUniqueValRelRecNumB());

					(*ppoutset)->AppendSegmentSet(bmset);
				}

				//Increment stats if requested
				if (info) {
					info->seg_semi++;
					info->segrecs_semi += 2;
				}

				continue;
			}

			//We'll have to retrieve another page.  See tech docs for details on why
			//DPT does not hold more than 2 (M204-IMMED-style) rec#s here.  (NB if we
			//did it would make the issue of ILMR extensions more pressing than it
			//currently is - see comments elsewhere in this file).
			BufferPageHandle bh = tdmgr->GetTableDPage(dbapi, rib.list_page);

			//-----------------------------------------------------------------------
			//C. List pages
			//-----------------------------------------------------------------------
			if (!rib.IsBitMap()) {
				InvertedIndexListPage pv(bh);
				int numrecs;

				//V2.14 Jan 09. Lists come out as arrays now.  Not a major change here.
				//std::vector<unsigned short> listrelrecs;
				ILArray ilarray;

				if (ppoutset || info) {
					pv.SlotRetrieveList(rib.list_pageslot, ilarray);
					numrecs = ilarray.NumEntries();
				}

				//Add to result set if requested
				if (ppoutset) {

					CreateOutputSetNowIfNotDoneYet(context, ppoutset, outset_created_here);
					if (ribixes_to_delete)
						ribixes_to_delete->push_back(x);

					//Note: lists don't get demoted on file if they're reduced to 1 record,
					//(see Remove...) so we might be able to create singleton seg set here.
					if (numrecs == 1) {
						(*ppoutset)->AppendSegmentSet(new SegmentRecordSet_SingleRecord
//							(rib.seg_num, (*listrelrecs)[0]));
							(rib.seg_num, ilarray.GetEntry(0)));
					}
					else {
						SegmentRecordSet_BitMap* bmset = 
							new SegmentRecordSet_BitMap(rib.seg_num);

						for (int x = 0; x < numrecs; x++)
//								bmset->Data()->Set((*listrelrecs)[x]);
							bmset->Data()->Set(ilarray.GetEntry(x));

						(*ppoutset)->AppendSegmentSet(bmset);
					}
				}

				//Increment stats if requested
				if (info) {
					info->seg_multi++;
					info->segrecs_multi += numrecs;
				}

				//Can we discard the old list now ?  (e.g. FILE RECORDS, DELETE FIELD)
				if (deletelists) {
					pv.SlotClearList(rib.list_pageslot);
					tdmgr->DeleteRecNumList(bh, rib.list_page, rib.list_pageslot);
				}

				continue;
			}

			//-----------------------------------------------------------------------
			//D. Must be a bitmap page then
			//-----------------------------------------------------------------------
			InvertedIndexBitMapPage pm(bh);
			int bmrecs;

			if (ppoutset || info)
				bmrecs = pm.SetCount();

			//Add to result set if requested
			if (ppoutset) {

				CreateOutputSetNowIfNotDoneYet(context, ppoutset, outset_created_here);
				if (ribixes_to_delete)
					ribixes_to_delete->push_back(x);

				//See comment in C. above about singletons (bitmaps aren't demoted either).
				if (bmrecs == 1) {
					unsigned int singlerec;
					pm.Data().FindNext(singlerec);

					(*ppoutset)->AppendSegmentSet(new SegmentRecordSet_SingleRecord
						(rib.seg_num, singlerec));
				}
				else {
					SegmentRecordSet_BitMap* bmset = 
						new SegmentRecordSet_BitMap(rib.seg_num);
					bmset->Data()->CopyBitsFrom(pm.Data());

					(*ppoutset)->AppendSegmentSet(bmset);
				}
			}

			//Increment stats if requested
			if (info) {
				info->seg_comp++;
				info->segrecs_comp += bmrecs;
			}

			//As per lists above
			if (deletelists)
				tdmgr->ReturnPage(bh, rib.list_page);
		}

		return outset_created_here;
	}
	catch (...) {
		if (ppoutset) {
			if (*ppoutset && outset_created_here)
				delete *ppoutset;
		}
		throw;
	}
}

//*****************************************
//V2.14 Jan 09.  Just a little neater here
void InvertedListAPI::CreateOutputSetNowIfNotDoneYet
(SingleDatabaseFileContext* context, BitMappedFileRecordSet** ppoutset, 
 bool& outset_created_here)
{
	if (ppoutset && *ppoutset == NULL) {
		*ppoutset = new BitMappedFileRecordSet(context);
		outset_created_here = true;
	}
}


//****************************************************************************************
bool InvertedListAPI::AddRecord(int recnum)
{
	//If it's a new index value, it can just have an IMMED record pointer
	if (!IndexValueValid()) {
		SetILMRInfo(recnum, INDEX_UNIQUE);
		return true;
	}

	//If there's currently a file-unique record the chances are we need to promote it now
	bool unique_promoted = false;
	if (UniqueValInFile()) {
		int unique_recnum = UniqueValRecNum();
		if (unique_recnum == recnum)
			return false; //same record

		short unique_segnum = SegNumFromAbsRecNum(unique_recnum);
		unsigned short unique_relrecnum = RelRecNumFromAbsRecNum(unique_recnum, unique_segnum);

		//Create a brand new master record with space for a single segment RIB
		int new_ilmr_page;
		short new_ilmr_slot;
		BufferPageHandle newbh = tdmgr->AllocateILMR
			(dbapi, 1, new_ilmr_page, new_ilmr_slot, true);

		//Map to it
		SetILMRInfo(new_ilmr_page, new_ilmr_slot);
		CacheMasterRecordPage(&newbh);

		file->IncStatILMRADD(dbapi);

		//Promote (the value is now just segment-unique rather than file-unique)
		SegmentRIB rib(unique_segnum, unique_relrecnum);
		InsertSegRIB(0, rib, false);

		unique_promoted = true;
	}

	//Then proceed as normal
	short segnum = SegNumFromAbsRecNum(recnum);
	unsigned short relrecnum = RelRecNumFromAbsRecNum(recnum, segnum);

	//Locate the appropriate segment RIB if possible
	SegmentRIB rib;
	short x;
	for (x = 0; x < num_segribs; x++) {
		GetSegRIBInfo(x, rib);
		if (rib.seg_num >= segnum)
			break;
	}

	//No records in this segment yet
	if (rib.seg_num != segnum) {
		SegmentRIB newsegrib(segnum, relrecnum);
		InsertSegRIB(x, newsegrib, !unique_promoted);
		file->IncStatILRADD(dbapi);
		return true;
	}

	//There is already at least one record in this segment - is it just one?
	if (rib.UniqueValInSegment()) {
		if (rib.SegUniqueValRelRecNumA() == relrecnum)
			return false; //same record

		//There are now two - we can still do it locally on the ILMR though
		rib.SegUniqueValRelRecNumB() = relrecnum;
		rib.ControlValue() = SEG_SEMI_UNIQUE;
		SetSegRIBInfo(x, rib);

		file->IncStatILRADD(dbapi);
		return true;
	}

	//Already two records - possibly need to promote these to a separate list page now
	if (rib.SemiUniqueValInSegment()) {
		unsigned short surna = rib.SegUniqueValRelRecNumA();
		unsigned short surnb = rib.SegUniqueValRelRecNumB();

		if (relrecnum == surna || relrecnum == surnb)
			return false; //one of the two

		//New list (request initial space for 3 entries)
		BufferPageHandle bh = tdmgr->AllocateRecNumList
			(dbapi, 3, rib.list_page, rib.list_pageslot, true);

		//Add the two previously semi-unique records and the new record to it
		InvertedIndexListPage pv(bh);
		pv.SlotAddRecordToList(rib.list_pageslot, surna);
		pv.SlotAddRecordToList(rib.list_pageslot, surnb);
		pv.SlotAddRecordToList(rib.list_pageslot, relrecnum);

		SetSegRIBInfo(x, rib);
		
		file->IncStatILRADD(dbapi);
		return true;
	}

	//Already more than two records but not a bitmap
	if (!rib.IsBitMap()) {
		BufferPageHandle bh = tdmgr->GetTableDPage(dbapi, rib.list_page);
		InvertedIndexListPage pv(bh);

		//Scan the list for the record number
		short rc = pv.SlotAddRecordToList(rib.list_pageslot, relrecnum);
		if (rc == 1)
			return false; //already there

		if (rc == 0) {
			file->IncStatILRADD(dbapi);
			return true; //added with no probs
		}

		//The other two possible outcomes entail retrieving the list
		//V2.14 Jan 09. Replace vector with array
//		std::vector<unsigned short> listrelrecs;
		ILArray ilarray;
		pv.SlotRetrieveList(rib.list_pageslot, ilarray);

		//Tried to add it but no more room on page - move to a page with more room
		if (rc == 3) {
			SegmentRIB newrib(segnum);
			BufferPageHandle newbh = tdmgr->AllocateRecNumList
				(dbapi, ilarray.NumEntries() + 1, newrib.list_page, newrib.list_pageslot, true);

			//Now we have the new page delete the list off its old one
			pv.SlotClearList(rib.list_pageslot);
			tdmgr->DeleteRecNumList(bh, rib.list_page, rib.list_pageslot);  

			//Rebuild into the new one
			InvertedIndexListPage newpv(newbh);
			newpv.SlotPopulateNewList(newrib.list_pageslot, ilarray);

			//Add the record we originally wanted to
			newpv.SlotAddRecordToList(newrib.list_pageslot, relrecnum);

			SetSegRIBInfo(x, newrib);
			file->IncStatILSMOVE(dbapi);
			file->IncStatILRADD(dbapi);

			return true;
		}

		//Not there but the list has become big enough to promote to a bitmap
		SegmentRIB newrib(segnum, SEG_BITMAP);
		newrib.list_page = tdmgr->AllocatePage(dbapi, 'M', true);

		//Now we have the new page delete the list off its old page
		pv.SlotClearList(rib.list_pageslot);
		tdmgr->DeleteRecNumList(bh, rib.list_page, rib.list_pageslot);  

		BufferPageHandle newbh = tdmgr->GetTableDPage(dbapi, newrib.list_page, true);
		InvertedIndexBitMapPage pm(newbh, true);

		//Add the ex-list record numbers plus the new one to the bitmap
		pm.InitializeFromList(ilarray);
		pm.FlagBit(relrecnum, true);

		SetSegRIBInfo(x, newrib);

		file->IncStatILRADD(dbapi);
		return true;
	}

	//Must be a bitmap already
	BufferPageHandle bh = tdmgr->GetTableDPage(dbapi, rib.list_page);
	InvertedIndexBitMapPage pm(bh);

	bool already_set = pm.FlagBit(relrecnum, true);
	if (already_set)
		return false;

	file->IncStatILRADD(dbapi);
	return true;
}

//****************************************************************************************
bool InvertedListAPI::RemoveRecord(int recnum)
{
	//With values that are unique in the file there is no ILMR
	if (UniqueValInFile()) {
		if (UniqueValRecNum() != recnum)
			return false;

		SetILMRInfo(-1, INDEX_INVALID);
		return true;
	}

	//Otherwise locate the appropriate segement RIB if possible
	short segnum = SegNumFromAbsRecNum(recnum);
	unsigned short relrecnum = RelRecNumFromAbsRecNum(recnum, segnum);

	SegmentRIB rib;
	short x;
	for (x = 0; x < num_segribs; x++) {
		GetSegRIBInfo(x, rib);

		//No records in the desired segment at all
		if (rib.seg_num > segnum)
			return false;

		if (rib.seg_num == segnum)
			break;
	}

	//Single record has the value
	if (rib.UniqueValInSegment()) {
		if (rib.SegUniqueValRelRecNumA() != relrecnum)
			return false; //different record

		//Therefore no more records in this segment now
		RemoveSegRIB(x);
		file->IncStatILRDEL(dbapi);
		return true;
	}

	//Two records have the value
	if (rib.SemiUniqueValInSegment()) {

		//If it's rec A, move rec B up to slot A, and say it's now totally unique
		if (rib.SegUniqueValRelRecNumA() == relrecnum) {
			rib.SegUniqueValRelRecNumA() = rib.SegUniqueValRelRecNumB();
			rib.SegUniqueValRelRecNumB() = USHRT_MAX;
			rib.ControlValue() = SEG_UNIQUE;
			SetSegRIBInfo(x, rib);
			file->IncStatILRDEL(dbapi);
			return true;
		}
		//If it's rec B, rec A is now totally unique
		else if (rib.SegUniqueValRelRecNumB() == relrecnum) {
			rib.SegUniqueValRelRecNumB() = USHRT_MAX;
			rib.ControlValue() = SEG_UNIQUE;
			SetSegRIBInfo(x, rib);
			file->IncStatILRDEL(dbapi);
			return true;
		}
		else
			return false; //neither A nor B
	}

	BufferPageHandle bh = tdmgr->GetTableDPage(dbapi, rib.list_page);

	//List on a separate page
	if (!rib.IsBitMap()) {
		InvertedIndexListPage pv(bh);

		bool finalrec = false;
		bool removed = pv.SlotRemoveRecordFromList(rib.list_pageslot, relrecnum, finalrec);
		if (!removed)
			return false;

		//The list may now be empty, and if so remove it.  Note however that there is no
		//attempt to demote a list witht just one or two record number on it to segment-
		//unique or semi-unique RIB pointers.  This would run the risk of unpleasant yoyoing
		//between the two states (see also bitmaps below).
		if (finalrec) {
			tdmgr->DeleteRecNumList(bh, rib.list_page, rib.list_pageslot);  
			RemoveSegRIB(x);
		}

		file->IncStatILRDEL(dbapi);
		return true;
	}

	//Bitmap
	InvertedIndexBitMapPage pm(bh);

	bool removed = pm.FlagBit(relrecnum, false);
	if (!removed)
		return false;

	//The bitmap may now be empty, in which case table D can have the page back.  As above
	//we don't make any attempt to demote to a list, or even seg-unique pointers, for the
	//same.  Here it would probably be more of an issue too.
	if (pm.SetCount() == 0) {
		tdmgr->ReturnPage(bh, rib.list_page);
		RemoveSegRIB(x);
	}

	file->IncStatILRDEL(dbapi);
	return true;
}

//****************************************************************************************
BitMappedFileRecordSet* InvertedListAPI::AssembleRecordSet
(const BitMappedFileRecordSet* restricting_set)
{
	BitMappedFileRecordSet* set = NULL; 
	Iterate(&set, NULL, false, restricting_set); 
	return set;
}

//****************************************************************************************
bool InvertedListAPI::ReplaceRecordSet
(BitMappedFileRecordSet* newset, BitMappedFileRecordSet** ppoldset)
{
	//Collect old set if required for TBO and delete lists/bitmaps at the same time.
	bool tbo_set_created = Iterate(ppoldset, NULL, true, NULL);

	//Remove all the empty segment RIBs.  This is not done during the iterate
	//just to keep the iteration logic simpler in there.
	while (num_segribs > 0)
		RemoveSegRIB(0);
	SetILMRInfo(-1, INDEX_INVALID); //may have been file unique before

	//Missing or empty replacement set - the index entry can be deleted now
	if (!newset)
		return tbo_set_created;
	if (newset->IsEmpty())
		return tbo_set_created;

	//If the new set has a single record, we don't need an ILMR at all
	int singlerec = newset->SingleRecordNumber();
	if (singlerec != -1) {
		SetILMRInfo(singlerec, INDEX_UNIQUE);
		return tbo_set_created;
	}

	//OK so create the new ILMR
	int new_ilmr_page;
	short new_ilmr_slot;
	BufferPageHandle newbh = tdmgr->AllocateILMR
		(dbapi, newset->NumSegs(), new_ilmr_page, new_ilmr_slot, false);

	//Adjust pointers on btree leaf page
	SetILMRInfo(new_ilmr_page, new_ilmr_slot);
	CacheMasterRecordPage(&newbh);
	file->IncStatILMRADD(dbapi);

	//Then build new RIBs from the replacement set.
	InsertSegRIBs(newset, false, NULL);

	return tbo_set_created;
}

//****************************************************************************************
//V2.14 Jan 09.  This processing is sufficiently different now from FILE RECORDS (above)
//to make a separate function clearer, rather than lots of conditional parameters etc.
//V2.19 June 2009.  Passed stats object in so that we can get an accurate array/bitmap
//breakdown during the final build phase.  If we count the bitmap before it comes in here
//it will often say far more bitmaps that is eventually the case.  This is quite a high
//profile stat so it would be good to get it right since we can.  
//Note 1: If there are existing index entries, we report as best we can as per final sitch
//Note 2: The DU chunk stats can also only ever be a best-guess in this respect.
//****************************************************************************************
void InvertedListAPI::AugmentRecordSet
(BitMappedFileRecordSet* addset, DU1FlushStats* stats)
{
	BitMappedFileRecordSet* pcurrset = NULL;

	try {
		//Retrieve old set where it overlaps with new.  Leave unaffected segments in place.
		std::vector<short> ribixes_to_delete;
		Iterate(&pcurrset, NULL, true, addset, &ribixes_to_delete);

		//Remove RIBs for the segment sets retrieved and deleted.  We'll reinsert them later.
		if (num_segribs > 0) {
			
			//The vector obtained above makes this easier (RIBs are *not* keyed by seg num)
			for (int vix = ribixes_to_delete.size() - 1; vix >= 0; vix--) {
				short ribix = ribixes_to_delete[vix];
				RemoveSegRIB(ribix);
			}
		}
		if (num_segribs == 0)
			SetILMRInfo(-1, INDEX_INVALID);

		//Bitwise OR the overlapping segments in the new and old sets
		if (pcurrset && pcurrset->NumSegs() > 0)
			addset->BitOr(pcurrset);

		//If just a file-unique record now, no ILMR is needed
		if (num_segribs == 0) {
			int singlerec = addset->SingleRecordNumber();
			if (singlerec != -1) {
				SetILMRInfo(singlerec, INDEX_UNIQUE);
				if (stats) stats->unique_f++;
				return;
			}
		}

		//Create a new ILMR if there isn't one still holding the unaffected seg sets
		if (num_segribs == 0) {
			int new_ilmr_page;
			short new_ilmr_slot;
			BufferPageHandle newbh = tdmgr->AllocateILMR
				(dbapi, addset->NumSegs(), new_ilmr_page, new_ilmr_slot, false);

			//Adjust pointers on btree leaf page
			SetILMRInfo(new_ilmr_page, new_ilmr_slot);
			CacheMasterRecordPage(&newbh);

			file->IncStatILMRADD(dbapi);
		}

		//Then insert RIBs for the new set.  Remember there may be some still left
		//there from earlier, for the segments unaffected by the OR.
		InsertSegRIBs(addset, true, stats);

		if (pcurrset)
			delete pcurrset;
	}
	catch (...) {
		if (pcurrset)
			delete pcurrset;
		throw;
	}
}

//****************************************************************************************
void InvertedListAPI::InsertSegRIBs
(BitMappedFileRecordSet* set, bool augmenting, DU1FlushStats* stats)
{
	short ribix = 0;

	std::map<short, SegmentRecordSet*>::const_iterator i;
	for (i = set->Data()->begin(); i != set->Data()->end(); i++) {

		//V2.19.  Only report on segments we are touching now, not the final number
		if (stats) stats->seglists++;

		SegmentRecordSet* segset = i->second;
		short segnum = segset->SegNum();

		//When augmenting, we won't always be just appending RIBs to the end, so 
		//scan along to find the right place to insert each segment in the list.
		ribix = (augmenting) ? AugmentFindRibixForSeg(segnum, ribix) : num_segribs;

		//Unique value in segment
		SegmentRecordSet_SingleRecord* singset = segset->CastToSingle();
		if (singset) {
			SegmentRIB rib(segnum, singset->RelRecNum()); 
			InsertSegRIB(ribix, rib, false);

			if (stats) stats->unique_s1++;
			continue;
		}

		//Since there are no equivalents of the table D lists in record sets, and also
		//record sets do not maintain a count like the file pages do (too complicated 
		//when ANDing and ORing sets), we are going to have to count a bitmap now.

		//V2.14 Jan 09.  The above comment is no longer always true.  For the special 
		//case of the new fast index-build facility,  record sets *can* contain an array 
		//representation, and it does contain a count.

		unsigned short segreccount = segset->Count();
		SegmentRecordSet_BitMap* bmset = segset->CastToBitMap();
		SegmentRecordSet_Array* arrayset = segset->CastToArray();

		//Semi-unique value in segment.  Same comment as above.
		if (segreccount == 2) {
			SegmentRIB rib(segnum, SEG_SEMI_UNIQUE);

			//V2.14 Jan 09. 2 recs in an array set during index build
			if (arrayset) {
				rib.SegUniqueValRelRecNumA() = arrayset->Array().GetEntry(0);
				rib.SegUniqueValRelRecNumB() = arrayset->Array().GetEntry(1);
			}
			//2 recs in a bitmap set at all other times
			else {
				unsigned short relrec = USHRT_MAX;
				bmset->FindNextRelRec(relrec);
				rib.SegUniqueValRelRecNumA() = relrec; 
				bmset->FindNextRelRec(relrec);
				rib.SegUniqueValRelRecNumB() = relrec; 
			}

			InsertSegRIB(ribix, rib, false);

			if (stats) stats->unique_s2++;
			continue;
		}

		//More than 2 but not enough for a bitmap
		if (segreccount <= DLIST_MAXRECS) {
			ILArray dummy;
			ILArray* pilarray = &dummy;

			//V2.14 Jan 09. Just access pointer to existing array
			if (arrayset) {
				pilarray = &(arrayset->Array());
			}
			//Make temp array from bitmap
			else {
				pilarray->InitializeAndReserve(segreccount);
				unsigned short relrec = USHRT_MAX;
				for (unsigned short x = 0; x < segreccount; x++) {
					bmset->FindNextRelRec(relrec);
					pilarray->AppendEntry(relrec);
				}
			}

			int listpage;
			short listslot;
			BufferPageHandle bh = tdmgr->AllocateRecNumList
				(dbapi, segreccount, listpage, listslot, false);

			InvertedIndexListPage pv(bh);
			pv.SlotPopulateNewList(listslot, *pilarray);

			SegmentRIB rib(segnum, listslot, listpage);
			InsertSegRIB(ribix, rib, false);

			if (stats) stats->arrays++;
			continue;
		}

		//Bitmap
		int bmpagenum = tdmgr->AllocatePage(dbapi, 'M', false);
		BufferPageHandle bmbuff = tdmgr->GetTableDPage(dbapi, bmpagenum, true);
		InvertedIndexBitMapPage bmpage(bmbuff, true);

		//V2.14 Jan 09.  Fixed serious bug - see comments in this new function.
		bmpage.InitializeFromBitMap(bmset->CData(), &segreccount);
		//bmpage.Data().CopyBitsFrom(*(bmset->CData()));

		SegmentRIB rib(segnum, SEG_BITMAP, bmpagenum);
		InsertSegRIB(ribix, rib, false);

		if (stats) stats->bmaps++;
	}
}

//******************************
short InvertedListAPI::AugmentFindRibixForSeg(short segnum, short prev_ribix)
{
	if (num_segribs == 0)
		return 0;

	SegmentRIB rib;
	short ribix;

	//We can save time by starting where the last one was inserted
	for (ribix = prev_ribix + 1; ribix < num_segribs; ribix++) {
		GetSegRIBInfo(ribix, rib);

		if (rib.seg_num == segnum)
			throw Exception(DB_ALGORITHM_BUG, "Bug: Duplicate Seg RIB in IL::Augment()");

		if (rib.seg_num > segnum)
			break;
	}

	return ribix;
}


//****************************************************************************************
//V3.0 Fast unload.
//See also comments in BitMappedFileRecordSet::Unload().
//****************************************************************************************
int InvertedListAPI::Unload(FastUnloadOutputFile* tapei, 
		const BitMappedFileRecordSet* baseset, bool crlf, bool pai_option)
{
	//Unique value in file.  This special case lets us skip the value and segment 
	//terminators, making the extract smaller, as well as faster to unload/reload.
	//(Undocumented for user I/O but DPT uses this by default).
	if (UniqueValInFile()) {
		int absrec = UniqueValRecNum();
		if (baseset && !baseset->ContainsAbsRecNum(absrec))
			return 0;

		if (pai_option) {
			tapei->AppendTextLine(util::IntToString(UniqueValRecNum()));
			tapei->AppendCRLF();
		}
		else {
			tapei->AppendBinaryInt16(0);
			tapei->AppendBinaryInt32(UniqueValRecNum());
		}
		return 1;
	}

	int segs_unloaded = 0;

	//Process each segment.  This could have been built into Iterate() above but that func
	//has been growing an absurd number of optional parameters lately and is getting ugly.
	for (short x = 0; x < num_segribs; x++) {
		SegmentRIB rib;
		GetSegRIBInfo(x, rib);

		//Ignore whole segment if not in the base set - like a find - very efficient.
		SegmentRecordSet* basesegset = NULL;
		if (baseset) {
			basesegset = baseset->GetSegmentSubSet(rib.seg_num);
			if (!basesegset)
				continue;
		}

		//Handle the 4 different types of segment RIB:

		//1. Seg-unique -------------------
		if (rib.UniqueValInSegment()) {
			unsigned short relrec = rib.SegUniqueValRelRecNumA();

			if (!baseset || basesegset->ContainsRelRecNum(relrec)) {

				if (pai_option) {
					int absrec = AbsRecNumFromRelRecNum(relrec, rib.seg_num);
					tapei->AppendTextLine(util::IntToString(absrec));
				}
				else {
					//We don't always know how many segs, so predict max and terminate later
					if (segs_unloaded == 0)
						tapei->AppendBinaryUint16(0xFFFF);

					tapei->AppendBinaryInt16(rib.seg_num);
					tapei->AppendBinaryUint16(1);
					tapei->AppendBinaryUint16(relrec);

					if (crlf)
						tapei->AppendCRLF();
				}

				segs_unloaded++;
			}

			continue;
		}

		//2. Seg-semi-unique -------------------
		if (rib.SemiUniqueValInSegment()) {
			unsigned short relreca = rib.SegUniqueValRelRecNumA();
			unsigned short relrecb = rib.SegUniqueValRelRecNumB();

			bool doreca = (!baseset || basesegset->ContainsRelRecNum(relreca));
			bool dorecb = (!baseset || basesegset->ContainsRelRecNum(relrecb));

			if (doreca || dorecb) {

				if (pai_option) {
					if (doreca) {
						int absrec = AbsRecNumFromRelRecNum(relreca, rib.seg_num);
						tapei->AppendTextLine(util::IntToString(absrec));
					}
					if (dorecb) {
						int absrec = AbsRecNumFromRelRecNum(relrecb, rib.seg_num);
						tapei->AppendTextLine(util::IntToString(absrec));
					}
				}
				else {
					if (segs_unloaded == 0)
						tapei->AppendBinaryUint16(0xFFFF);

					tapei->AppendBinaryInt16(rib.seg_num);
					tapei->AppendBinaryUint16( (doreca && dorecb) ? 2 : 1);

					if (doreca) tapei->AppendBinaryUint16(relreca);
					if (dorecb) tapei->AppendBinaryUint16(relrecb);

					if (crlf)
						tapei->AppendCRLF();
				}

				segs_unloaded++;
			}

			continue;
		}

		//--- Arrays and bitmaps ---
		BufferPageHandle bh = tdmgr->GetTableDPage(dbapi, rib.list_page);

		//3. Array -------------------
		if (!rib.IsBitMap()) {

			InvertedIndexListPage pv(bh);

			ILArray pagearray;
			pv.SlotRetrieveList(rib.list_pageslot, pagearray);
			ILArray* array = &pagearray;

			unsigned short segrecs = pagearray.NumEntries();

			//If masking, rather than build a bitmap we may as well test each entry 
			//against the baseset - effectively do the AND since we'd have to loop anyway.  
			//Less work in the end, plus we produce an array which is smaller to unload.
			ILArray maskarray;

			if (basesegset) {
				maskarray.InitializeAndReserve(segrecs);

				unsigned short* ptr = pagearray.Data();
				unsigned short* end = ptr + segrecs;

				while (ptr != end) {
					if (basesegset->ContainsRelRecNum(*ptr))
						maskarray.AppendEntry(*ptr);
					ptr++;
				}

				segrecs = maskarray.NumEntries();
				array = &maskarray;
			}

			//We may have masked the segment down to nothing
			if (segrecs == 0)
				continue;

			//Nope, still some left
			if (pai_option) {
				const unsigned short* ptr = array->CData();
				const unsigned short* end = ptr + array->NumEntries();
				while (ptr != end) {
					int absrec = AbsRecNumFromRelRecNum(*ptr, rib.seg_num);
					tapei->AppendTextLine(util::IntToString(absrec));
					ptr++;
				}
			}
			else {
				if (segs_unloaded == 0)
					tapei->AppendBinaryUint16(0xFFFF);

				tapei->AppendBinaryInt16(rib.seg_num);
				tapei->AppendBinaryUint16(segrecs);
				tapei->AppendBinaryUint16Array(array->Data(), segrecs);

				if (crlf)
					tapei->AppendCRLF();
			}

			segs_unloaded++;
			continue;
		}

		//4. Bitmap -------------------
		InvertedIndexBitMapPage pm(bh);

		util::BitMap& pagebitmap = pm.Data();
		util::BitMap* bitmap = &pagebitmap;

		//The DBMS maintains this so we can avoid an explicit count here
		unsigned short segrecs = pm.SetCount();

		//Much like the array case above, do any masking with the aid of a temp copy
		util::BitMap maskbitmap;

		if (basesegset) {

			//...although making a bitmap copy just for 1 record would be distasteful
			SegmentRecordSet_SingleRecord* basesegsing = basesegset->CastToSingle();
			if (basesegsing) {
				unsigned short relrec = basesegsing->RelRecNum();

				if (pagebitmap.Test(relrec)) {

					if (pai_option) {
						int absrec = AbsRecNumFromRelRecNum(relrec, rib.seg_num);
						tapei->AppendTextLine(util::IntToString(absrec));
					}
					else {
						if (segs_unloaded == 0)
							tapei->AppendBinaryUint16(0xFFFF);

						tapei->AppendBinaryInt16(rib.seg_num);
						tapei->AppendBinaryUint16(1);
						tapei->AppendBinaryUint16(relrec);

						if (crlf)
							tapei->AppendCRLF();
					}
					segs_unloaded++;
				}

				continue;
			}

			//Two bitmaps - and finally the bitwise operators get to shine
			maskbitmap.EnsureCapacity(DBPAGE_BITMAP_SIZE, false);
			maskbitmap.CopyBitsFrom(pagebitmap);
			maskbitmap &= *(basesegset->CastToBitMap()->Data());

			bitmap = &maskbitmap;

			//No real way to avoid the count here
			segrecs = bitmap->Count();
		}

		//We may have masked the segment down to nothing
		if (segrecs == 0)
			continue;

		//Nope, still some left
		if (pai_option) {
			for (unsigned int bit = 0; bit < (unsigned int)DBPAGE_BITMAP_SIZE; bit++) {
				if (!bitmap->FindNext(bit, bit))
					break;
				int absrec = AbsRecNumFromRelRecNum(bit, rib.seg_num);
				tapei->AppendTextLine(util::IntToString(absrec));
			}
		}
		else {
			if (segs_unloaded == 0)
				tapei->AppendBinaryUint16(0xFFFF);

			tapei->AppendBinaryInt16(rib.seg_num);

			//The user would expect an extract to be small if the baseset was small, so 
			//we definitely should go to the trouble of demoting sparse bitmaps to array form 
			//here if possible rather than unloading whole bitmaps for just a handful of bits.
			if (segrecs <= DLIST_MAXRECS) {
				tapei->AppendBinaryUint16(segrecs);

				for (unsigned int bit = 0; bit < (unsigned int)DBPAGE_BITMAP_SIZE; bit++) {
					if (!bitmap->FindNext(bit, bit))
						break;
					tapei->AppendBinaryUint16(bit);
				}
			}
			else {
				tapei->AppendBinaryUint16(USHRT_MAX);
				tapei->AppendRawData((const char*)bitmap->CData(), DBPAGE_BITMAP_BYTES);
			}

			if (crlf)
				tapei->AppendCRLF();
		}

		segs_unloaded++;
	}

	//Terminator to indicate no more inverted lists for this value.  We could not know
	//up front how many there'd be unless we built it all in memory first, which would
	//be awkward and just more memcpy etc. overhead during file write.
	if (segs_unloaded != 0) {
		if (pai_option)
			tapei->AppendCRLF();
		else
			tapei->AppendBinaryInt32(-1);
	}

	return segs_unloaded;
}

} //close namespace

