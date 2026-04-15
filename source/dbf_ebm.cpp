
#include "stdafx.h"

#include "dbf_ebm.h"

//Utils
//API tiers
#include "bmset.h"
#include "cfr.h"
#include "dbf_tabled.h"
#include "dbfile.h"
#include "dbctxt.h"
#include "page_p.h" //#include "page_P.h" : V2.24 case is less interchangeable on *NIX - Roger M.
#include "page_f.h" //#include "page_F.h"
//Diagnostics
#include "except.h"
#include "msg_db.h"

namespace dpt {

class SegmentRecordSet;

//*************************************************************************************
BufferPageHandle DatabaseFileEBMManager::GetSegmentBitMapPage
(DatabaseServices* dbapi, short segnum)
{
	int chunknum = ChunkNumFromSegNum(segnum);

	FCTPage_E fctpage(dbapi, file->fct_buff_page);
	int ix_pagenum = fctpage.GetEBMIndexPageNum(chunknum);

	if (ix_pagenum == -1)
		return BufferPageHandle();

	BufferPageHandle hix = file->GetTableDMgr()->GetTableDPage(dbapi, ix_pagenum);
	ExistenceBitMapIndexPage pix(hix);

	int relsegnum = RelSegNumFromAbsSegNum(segnum);
	int bm_pagenum = pix.GetBitmapPageNum(relsegnum);

	if (bm_pagenum == -1)
		return BufferPageHandle();

	return file->GetTableDMgr()->GetTableDPage(dbapi, bm_pagenum);
}

//*************************************************************************************
bool DatabaseFileEBMManager::DoesPrimaryRecordExist(DatabaseServices* dbapi, int recnum) 
{
	CFRSentry s(dbapi, file->cfr_exists, BOOL_SHR);

	short segnum = SegNumFromAbsRecNum(recnum);
	BufferPageHandle hbm = GetSegmentBitMapPage(dbapi, segnum);

	if (!hbm.IsEnabled())
		return false;

	ExistenceBitMapPage pbm(hbm);

	unsigned short relrecnum = RelRecNumFromAbsRecNum(recnum, segnum);
	return pbm.TestBit(relrecnum);
}

//*************************************************************************************
BitMappedFileRecordSet* DatabaseFileEBMManager::MaskOffNonexistentInSet
(DatabaseServices* dbapi, BitMappedFileRecordSet* oldset) 
{
//index manager has take EXISTS already
//CFRSentry s(dbapi, file->cfr_exists, BOOL_SHR);

	short prevseg = -1;

	//Retrieve all existence bitmap pages corresponding to segments in the set
	std::map<short, SegmentRecordSet*>* oldsetdata = oldset->Data();
	std::map<short, SegmentRecordSet*>::iterator i;

	for (i = oldsetdata->begin(); i != oldsetdata->end(); /* no ++ cos of relocate later */) {
		int currseg = i->first;

		BufferPageHandle hbm = GetSegmentBitMapPage(dbapi, currseg);

		//We would generally only get to here with a set that had no EBM page if
		//FRN was used with a record number higher than any yet stored.
		if (!hbm.IsEnabled()) {
			delete i->second;
			i->second = NULL;
		}
		else {
			ExistenceBitMapPage pbm(hbm);

			//Make a temporary segment set so we can use the standard BitAnd function
			//which deletes sets that get emptied, and demotes from bitmaps too.
			SegmentRecordSet_BitMap sstemp(currseg, pbm);

			i->second = SegmentRecordSet::BitAnd(i->second, &sstemp);
		}

		//Go to next segment
		if (i->second) {
			prevseg = currseg;
			i++;
		}
		//Remove from the file set if now no records left in segment
		else {
			oldsetdata->erase(i);

			//Relocate the iterator after map work
			if (prevseg == -1)
				i = oldsetdata->begin();
			else
				i = oldsetdata->find(prevseg);
		}

	}

	//If there are no segments left with records we delete the whole file set.
	if (oldsetdata->empty()) {
		delete oldset;
		return NULL;
	}

	return oldset;
}

//*************************************************************************************
BitMappedFileRecordSet* DatabaseFileEBMManager::CreateWholeFileSet
(DatabaseServices* dbapi, SingleDatabaseFileContext* sfc, BitMappedFileRecordSet* stackset) 
{
	CFRSentry s(dbapi, file->cfr_exists, BOOL_SHR);

	BitMappedFileRecordSet* result = stackset;
	
	try {
		//This entails copying every single bitmap in the EBM!
		for (short segnum = 0; ; segnum++) {
			BufferPageHandle hbm = GetSegmentBitMapPage(dbapi, segnum);

			//This happens when the requested segment is outside the existing range
			if (!hbm.IsEnabled())
				break;

			ExistenceBitMapPage pbm(hbm);

			if (pbm.SetCount() > 0) {
				
				if (!result)
					result = new BitMappedFileRecordSet(sfc);

				//If only 1 bit in this map, create special runt object
				if (pbm.SetCount() == 1) {
					unsigned int temp;
					pbm.Data().FindNext(temp);

					unsigned short relrec = temp;
					result->AppendSegmentSet
						(new SegmentRecordSet_SingleRecord(segnum, relrec));
				}
				else {
					SegmentRecordSet_BitMap* bs = new SegmentRecordSet_BitMap(segnum);
					bs->Data()->CopyBitsFrom(pbm.Data());
					result->AppendSegmentSet(bs);
				}
			}
		}

		return result;
	}
	catch (...) {
		if (stackset)
			stackset->Data()->clear();
		else
			delete result;
		throw;
	}
}



//*************************************************************************************
//Atom handlers
//*************************************************************************************

// Used in STORE and DELETE, and their backouts
void DatabaseFileEBMManager::Atom_FlagSingleRecordExistence
(DatabaseServices* dbapi, int recnum, bool flag, bool storing) 
{
	CFRSentry s(dbapi, file->cfr_exists, BOOL_EXCL);

	DatabaseFileTableDManager* tabledmgr = file->GetTableDMgr();

	FCTPage_E fctpage(dbapi, file->fct_buff_page);

	//EBM is a 2-level structure of pages.  First find level 1 index page.
	short segnum = SegNumFromAbsRecNum(recnum);
	int chunknum = ChunkNumFromSegNum(segnum);
	int ix_pagenum = fctpage.GetEBMIndexPageNum(chunknum);

	//During forward STORE, we can extend the whole EBM structure.  Note that since
	//BRECPPG can only go up to 1024, the maximum gap between stored record numbers is
	//1023 which means there will never be a gap in the EBM.  Every page (64K bits) will
	//always have at least some bits set on it, at least before the records get deleted.
	//So, this algorithm is not complicated by having to insert placeholder pages.
	bool fresh = false;
	if (ix_pagenum == -1) {
		if (!storing)
			throw Exception(DB_STRUCTURE_BUG, "Bug: Existence bitmap index is corrupt");

		//So allocate new level 1 index page in table D
		ix_pagenum = tabledmgr->AllocatePage(dbapi, 'E', true);
		fresh = true;
	}

	//Get handle to the level 1 index page now
	BufferPageHandle hix = tabledmgr->GetTableDPage(dbapi, ix_pagenum, fresh);
	ExistenceBitMapIndexPage pix(hix, fresh);

	//Initialize it?
	if (fresh)
		fctpage.SetEBMIndexPageNum(chunknum, ix_pagenum);

	//Now find the level 2 page, which will contains the actual rec# bitmap
	int relsegnum = RelSegNumFromAbsSegNum(segnum);
	int bm_pagenum = pix.GetBitmapPageNum(relsegnum);

	//As above, extend if necessary
	fresh = false;
	if (bm_pagenum == -1) {
		if (!storing)
			throw Exception(DB_STRUCTURE_BUG, "Bug: Existence bitmap is corrupt");
	
		bm_pagenum = tabledmgr->AllocatePage(dbapi, 'P', true);
		fresh = true;
	}

	//Get handle to bitmap page now
	BufferPageHandle hbm = tabledmgr->GetTableDPage(dbapi, bm_pagenum, fresh);
	ExistenceBitMapPage pbm(hbm, fresh);

	//Initialize it?
	if (fresh)
		pix.SetBitmapPageNum(relsegnum, bm_pagenum);

	//And finally set the bit
	unsigned short relrecnum = RelRecNumFromAbsRecNum(recnum, segnum);

	//This should never happen with the EBM
	if (pbm.FlagBit(relrecnum, flag) == flag)
		throw Exception(DB_STRUCTURE_BUG, 
			"Bug: Existence bitmap is corrupt (bit already has requested value)");
}

//*************************************************************************************

// Used during dirty DELETE of a record set, and its backout
int DatabaseFileEBMManager::Atom_FlagRecordSetExistence
(DatabaseServices* dbapi, BitMappedFileRecordSet* fset_to_flag, 
 bool flag, BitMappedFileRecordSet** tbo_required_set, bool during_fastload)
{
	int num_bits_flagged = 0;

	{ //Dummy code block - see large comment at the bottom of this func
		CFRSentry s_exists(dbapi, file->cfr_exists, BOOL_EXCL);

		//V3.0
		if (during_fastload)
			ExtendEBMToCoverSet(dbapi, fset_to_flag);

		if (tbo_required_set)
			*tbo_required_set = new BitMappedFileRecordSet(fset_to_flag->HomeFileContext());

		try {
			//Process each segment in the input set
			std::map<short, SegmentRecordSet*>* setdata = fset_to_flag->Data();
			std::map<short, SegmentRecordSet*>::iterator i;

			for (i = setdata->begin(); i != setdata->end(); i++) {
				int seg = i->first;
				SegmentRecordSet* segset_to_flag = i->second;

				//We can be confident that the pages are here by this point, so no extend needed
				BufferPageHandle hbm = GetSegmentBitMapPage(dbapi, seg);
				ExistenceBitMapPage pbm(hbm);

				SegmentRecordSet* segset_flagged = NULL;
				try {
					if (segset_to_flag->CastToBitMap()) {

						//We need to do two operations - an AND to get the set of bits to 
						//use in TBO if required, and an ANDNOT to do the bit-clearing 
						//forward update.  There would be some benefit in doing them both 
						//in a single loop over both bitmaps, but then we would have to do 
						//extra work to construct the appropriate segset object for TBO.  
						//It's simpler to do it in two distinct steps as all the code already 
						//exists too, and it's probably not much extra processing anyway.  
						//And finally, separating the two functions means we can skip the 
						//AND if TBO is off.
						if (tbo_required_set) {
							segset_flagged = segset_to_flag->MakeCopy();

							SegmentRecordSet_BitMap segset_ebm(seg, pbm);
							segset_flagged = SegmentRecordSet::BitAnd(segset_flagged, &segset_ebm);
						}

						num_bits_flagged += 
							pbm.FlagBits(segset_to_flag->CastToBitMap()->CData(), flag);
					}

					//When only 1 rec in seg, it's simpler
					else {
						if (pbm.FlagBit(segset_to_flag->CastToSingle()->RelRecNum(), flag)) {
							num_bits_flagged++;
							
							if (tbo_required_set)
								segset_flagged = segset_to_flag->MakeCopy();
						}
					}

					if (segset_flagged && tbo_required_set)
						(*tbo_required_set)->AppendSegmentSet(segset_flagged);
					segset_flagged = NULL;
				}
				catch(...) {
					if (segset_flagged)
						delete segset_flagged;
					throw;
				}
			}
		}
		catch(...) {
			if (tbo_required_set)
				delete *tbo_required_set;
			throw;
		}
	}

	//This is as per Model 204, but I don't like it.  The name of this stat suggests
	//that master records have actually been deleted, and they haven't really.  In a
	//normal "clean" DELETE RECORD the stat is incremented under the protection of
	//CFR_DIRECT after the master record is removed from its table B page, whereas
	//here we are not actually touching table B, and so don't have CFR_DIRECT
	//enqueued.  One possibility would be to change things round so that MSTRDEL is
	//protected by EXISTS instead of DIRECT as a matter of course, since EXISTS
	//is taken during normal DELETE RECORD too, but that does not seem correct to me.
	//I am happier making this a special case, to be clear that the M204 convention
	//is in fact a kluge to partially disguise the dirtiness of dirty deletes.
	//Note that the above dummy code block is there merely to make it so we don't
	//hold 2 CFRs at the same time.  As it happens, this order (INDEX, DIRECT) is the
	//same as during a find, so it would be OK, but it's good not to get into the 
	//habit of taking multiple locks unless there is a good reason to, like in a find.
	if (!during_fastload) {
	//V3.0
	//{
		CFRSentry s_direct(dbapi, file->cfr_direct, BOOL_EXCL);

		FCTPage_E fctpage(dbapi, file->fct_buff_page);
		fctpage.ChangeMstrdel( (flag) ? -num_bits_flagged : num_bits_flagged);
	}

	return num_bits_flagged;
}

//*************************************************************************************
//V3.0
void DatabaseFileEBMManager::ExtendEBMToCoverSet
(DatabaseServices* dbapi, BitMappedFileRecordSet* set)
{
	DatabaseFileTableDManager* tabledmgr = file->GetTableDMgr();
	FCTPage_E fctpage(dbapi, file->fct_buff_page);

	//As noted in the regular EBM extension during STORE above, there can never be
	//gaps in the EBM structure, so we can confidently allocate all the pages up to
	//the highest rec# in the set knowing they'll all be used.
	int reqd_hirec = set->HiRecNum();
	int map_hirec = -1;

	//Iterate through the EBM index from the start
	for (int chunk = 0; chunk <=3 && map_hirec < reqd_hirec; chunk++) {

		//Level 1 index page
		int ix_pagenum = fctpage.GetEBMIndexPageNum(chunk);

		//Allocate a fresh one if required
		bool fresh = false;
		if (ix_pagenum == -1) {
			ix_pagenum = tabledmgr->AllocatePage(dbapi, 'E', true);
			fresh = true;
		}

		BufferPageHandle hix = tabledmgr->GetTableDPage(dbapi, ix_pagenum, fresh);
		ExistenceBitMapIndexPage pix(hix, fresh);

		if (fresh)
			fctpage.SetEBMIndexPageNum(chunk, ix_pagenum);

		//Iterate through each segment bitmap in this EBM chunk, as high as required
		for (int relseg = 0; relseg < DBP_E_NUMSLOTS && map_hirec < reqd_hirec; relseg++) {

			int bm_pagenum = pix.GetBitmapPageNum(relseg);

			//Again allocate a fresh one if required
			fresh = false;
			if (bm_pagenum == -1) {
				bm_pagenum = tabledmgr->AllocatePage(dbapi, 'P', true);
				fresh = true;
			}

			BufferPageHandle hbm = tabledmgr->GetTableDPage(dbapi, bm_pagenum, fresh);
			ExistenceBitMapPage pbm(hbm, fresh);

			if (fresh)
				pix.SetBitmapPageNum(relseg, bm_pagenum);

			//So what's the highest record we now have EBM for?
			int ebmsegs = chunk * DBP_E_NUMSLOTS + relseg + 1;
			int ebmrecs = ebmsegs * DBPAGE_SEGMENT_SIZE;
			map_hirec = ebmrecs - 1;
		}
	}
}







//*************************************************************************************
//*************************************************************************************
//*************************************************************************************
//Atom manager objects: store record
//*************************************************************************************
//*************************************************************************************
//*************************************************************************************
AtomicBackout* AtomicUpdate_ExistizeStoredRecord::CreateCompensatingUpdate()
{
	backout = new AtomicBackout_DeExistizeStoredRecord(context, recnum);
	return backout;
}

//*************************************************************************************
void AtomicUpdate_ExistizeStoredRecord::Perform()
{
	//V2.14 Feb 09.  The file must already be UU-registered so we can always skip that.
//	RegisterAtomicUpdate();
	RegisterAtomicUpdate(true);

	context->GetDBFile()->GetEBMMgr()->Atom_FlagSingleRecordExistence
		(context->DBAPI(), recnum, true, true);

	if (backout)
		backout->Activate();
}

//*************************************************************************************
void AtomicBackout_DeExistizeStoredRecord::Perform()
{
	context->GetDBFile()->GetEBMMgr()->Atom_FlagSingleRecordExistence
		(context->DBAPI(), recnum, false);
}

//*************************************************************************************
//Delete record
//*************************************************************************************
AtomicBackout* AtomicUpdate_DeExistizeDeletedRecord::CreateCompensatingUpdate()
{
	backout = new AtomicBackout_ExistizeDeletedRecord(context, recnum);
	return backout;
}

//*************************************************************************************
void AtomicUpdate_DeExistizeDeletedRecord::Perform()
{
	RegisterAtomicUpdate();

	context->GetDBFile()->GetEBMMgr()->Atom_FlagSingleRecordExistence
		(context->DBAPI(), recnum, false);

	if (backout)
		backout->Activate();
}

//*************************************************************************************
void AtomicBackout_ExistizeDeletedRecord::Perform()
{
	context->GetDBFile()->GetEBMMgr()->Atom_FlagSingleRecordExistence
		(context->DBAPI(), recnum, true, false);
}

//*************************************************************************************
//Dirty delete
//*************************************************************************************
AtomicBackout* AtomicUpdate_DirtyDeleteRecords::CreateCompensatingUpdate()
{
	backout = new AtomicBackout_DeDirtyDeleteRecords(context);
	return backout;
}

//*************************************************************************************
void AtomicUpdate_DirtyDeleteRecords::Perform()
{
	RegisterAtomicUpdate();

	//Avoid building old set if not required
	BitMappedFileRecordSet* bits_to_turn_on_again = NULL;
	BitMappedFileRecordSet** pp_tbo_bits = (tbo_is_off) ? NULL : &bits_to_turn_on_again;
	
	int num_bits_turned_off = context->GetDBFile()->GetEBMMgr()->
		Atom_FlagRecordSetExistence(context->DBAPI(), bits_to_turn_off, false, pp_tbo_bits);

	//The set will be null if no bits were turned off - so no need to delete it
	if (backout && num_bits_turned_off > 0) {
		static_cast<AtomicBackout_DeDirtyDeleteRecords*>(backout)->
			NoteBitsTurnedOff(bits_to_turn_on_again);
		backout->Activate();
	}
}

//*************************************************************************************
AtomicBackout_DeDirtyDeleteRecords::~AtomicBackout_DeDirtyDeleteRecords()
{
	if (bits_to_turn_back_on)
		delete bits_to_turn_back_on;
}

//*************************************************************************************
void AtomicBackout_DeDirtyDeleteRecords::Perform()
{
	if (bits_to_turn_back_on)
		context->GetDBFile()->GetEBMMgr()->
			Atom_FlagRecordSetExistence(context->DBAPI(), bits_to_turn_back_on, true);
}



} //close namespace


