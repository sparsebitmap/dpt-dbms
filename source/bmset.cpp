
#include "stdafx.h"

#include "bmset.h"

//Utils
#include "bitmap3.h"
#include "handles.h"
//API Tiers
#include "record.h"
#include "sortset.h"
#include "sortspec.h"
#include "dbf_field.h"
#include "dbf_rlt.h"
#include "du1step.h"
#include "dbfile.h"
#include "dbctxt.h"
#include "dbserv.h"
//Diagnostics
#include "except.h"
#include "msg_db.h"
#include "msg_file.h"

namespace dpt {

//***************************************************************************************
BitMappedFileRecordSet::BitMappedFileRecordSet(SingleDatabaseFileContext* sfc) 
: FileRecordSet(sfc->GetDBFile()), home_context(sfc), append_hi_segset(NULL) 
{}

//most often deleted when cleared
void BitMappedFileRecordSet::ClearButNoDelete() 
{
	DestroySetData();
	append_hi_segset = NULL;
} 


//***************************************************************************************
void BitMappedFileRecordSet::AppendSegmentSet(SegmentRecordSet* segset)
{
	try {
		std::pair<short, SegmentRecordSet*> newentry;
		newentry = std::make_pair<short, SegmentRecordSet*>(segset->SegNum(), segset);

		std::pair<std::map<short, SegmentRecordSet*>::iterator, bool> insflag;
		insflag = data.insert(newentry);

		//The find engine should never add the same segment twice.  It will generally be
		//processing segments in order - hence the function name "append...".
		if (insflag.second == false)
			throw Exception(DB_MRO_MGMT_BUG, 
				"Bug: processed the same segment twice whilst building bm set");
	}

	//This saves all the callers having to code their own catch blocks
	catch (...) {
		delete segset;
		throw;
	}
}

//***************************************************************************************
int BitMappedFileRecordSet::Count() const
{
	int all_seg_count = 0;

	std::map<short, SegmentRecordSet*>::const_iterator i;
	for (i = data.begin(); i != data.end(); i++)
		all_seg_count += i->second->Count();

	return all_seg_count;
}

//***************************************************************************************
int BitMappedFileRecordSet::SingleRecordNumber() const
{
	if (NumSegs() != 1)
		return -1;

	SegmentRecordSet* segset = data.begin()->second;

	//Note that whenever bitmap sets are reduced, we make every effort to demote
	//them to single-record versions.  Therefore we can make this assumption now.
	if (segset->CastToSingle())
		return segset->CastToSingle()->AbsRecNum();
	else
		return -1;
}

//***************************************************************************************
bool BitMappedFileRecordSet::ContainsAbsRecNum(int absrec) const
{
	short segnum = SegNumFromAbsRecNum(absrec);

	std::map<short, SegmentRecordSet*>::const_iterator i = data.find(segnum);
	if (i == data.end())
		return false;

	SegmentRecordSet* ss = i->second;
	unsigned short relrec = RelRecNumFromAbsRecNum(absrec, segnum);

	if (ss->CastToSingle())
		return (ss->CastToSingle()->RelRecNum() == relrec);
	else
		return ss->CastToBitMap()->ContainsRelRecNum(relrec);
}

//***************************************************************************************
//To ease the EBM phase in loads
int BitMappedFileRecordSet::HiRecNum() const
{
	if (IsEmpty())
		return -1;

	std::map<short, SegmentRecordSet*>::const_reverse_iterator i = data.rbegin();
	SegmentRecordSet* topsegset = i->second;

	if (topsegset->CCastToSingle())
		return topsegset->CCastToSingle()->AbsRecNum();

	SegmentRecordSet_BitMap* bms = topsegset->CastToBitMap();
	unsigned short relrec = USHRT_MAX;
	bms->FindPrevRelRec(relrec);
	return AbsRecNumFromRelRecNum(relrec, topsegset->SegNum());
}

//***************************************************************************************
void BitMappedFileRecordSet::LockShr(DatabaseServices* d)
{
	lock = file->GetRLTMgr()->ApplyShrFoundSetLock(this, d);
}

//***************************************************************************************
void BitMappedFileRecordSet::LockExcl(DatabaseServices* d)
{
	lock = file->GetRLTMgr()->ApplyExclFoundSetLock(this, d);
}

//***************************************************************************************
void BitMappedFileRecordSet::Unlock()
{
	if (lock)
		file->GetRLTMgr()->ReleaseNormalLock(lock);
	lock = NULL;
}

//***************************************************************************************
void BitMappedFileRecordSet::TBSHandOverLock(BitMappedFileRecordSet* recipient)
{
	file->GetRLTMgr()->TBSReplaceLockSet(this, recipient);
	recipient->lock = lock;
	lock = NULL;
}

//***************************************************************************************
void BitMappedFileRecordSet::TBSDropSegmentLock(short segnum)
{
	LockingSentry ls(file->GetRLTMgr()->TBSExposeShrTableLock());

	//In theory it should be the first segment present
	std::map<short, SegmentRecordSet*>::iterator i = data.begin();
	if (i->first != segnum)
		throw Exception(DB_ALGORITHM_BUG, "Bug: Rogue segment in BMFRS during TBS");

	delete i->second;
	data.erase(i);
}

//***************************************************************************************
void BitMappedRecordSet::TBSDropSegmentLock(short segnum, RecordSetCursor* rsc)
{
	//There's only one file in these sets - no need to search
	BitMappedFileRecordSet* frs = data.begin()->second;

	frs->TBSDropSegmentLock(segnum);

	if (frs->IsEmpty())
		throw Exception(DB_ALGORITHM_BUG, "Bug: missing segment in BMRS during TBS");

	//Reposition the cursor after that map deletion
	static_cast<BitMappedRecordSetCursor*>(rsc)->TBSReposition(frs->Data());
}

//***************************************************************************************
BitMappedFileRecordSet* BitMappedFileRecordSet::MakeCopy
(BitMappedFileRecordSet* stackset, short loseg, short hiseg) const
{
	BitMappedFileRecordSet* copy = NULL;
	if (stackset)
		copy = stackset;

	try {
		std::map<short, SegmentRecordSet*>::const_iterator i;
		for (i = data.begin(); i != data.end(); i++) {
			if (i->first < loseg || i->first > hiseg)
				continue;

			if (!copy)
				copy = new BitMappedFileRecordSet(home_context);

			copy->data[i->first] = i->second->MakeCopy();
		}
	}
	catch (...) {
		if (stackset)
			stackset->DestroySetData();
		else if (copy)
			delete copy;

		throw;
	}

	return copy;
}

//***************************************************************************************
void BitMappedFileRecordSet::BitOr(const BitMappedFileRecordSet* s2, bool rhs_dispensable)
{
	if (s2 == this || s2 == NULL)
		return;

	//Use a merge-style approach with the two sets
	std::map<short, SegmentRecordSet*>::iterator i = data.begin();
	std::map<short, SegmentRecordSet*>::const_iterator i2 = s2->data.begin();

	for (;;) {
		//No more segment sets to apply
		if (i2 == s2->data.end())
			break;

		//Decide if it's the same segment by comparing segment numbers
		short segnum2 = i2->first;
		short segnum;
		if (i != data.end())
			segnum = i->first;
		else
			segnum = SHRT_MAX;

		//Same segment - bitOR the set data
		if (segnum == segnum2) {

			//Might result in promoting to a bitmap
			i->second = SegmentRecordSet::BitOr(i->second, i2->second);
			i++;
			i2++;
		}

		//Nothing to add in this segment
		else if (segnum < segnum2)
			i++;

		//A whole segment set must be added 
		else {

			//V2.14 Jan 09.  In some cases the RHS is a set we don't mind trashing,
			//in which case we can save a simple 8K bitmap copy here.
//			SegmentRecordSet* insert_segset = i2->second->MakeCopy();
			SegmentRecordSet* insert_segset = i2->second->MakeCopy(rhs_dispensable);

			try {
				data[segnum2] = insert_segset;
			}
			catch (...) {
				delete insert_segset;
				throw;
			}

			//Relocate iterator after that map work
			i = data.find(segnum2);
			i++;
			i2++;
		}
	}
}

//***************************************************************************************
BitMappedFileRecordSet* BitMappedFileRecordSet::BitAnd_S
(BitMappedFileRecordSet* s, const BitMappedFileRecordSet* s2, bool rhs_not, bool seglevel)
{
	if (s == NULL)
		return NULL;
	if (s2 == NULL) {
		if (rhs_not)
			return s;
		else {
			delete s;
			return NULL;
		}
	}
	if (s == s2) {
		if (rhs_not) {
			delete s;
			return NULL;
		}
		else
			return s;
	}

	//Use a merge-style approach with the two sets
	std::map<short, SegmentRecordSet*>::iterator i = s->data.begin();
	std::map<short, SegmentRecordSet*>::const_iterator i2 = s2->data.begin();

	BitMappedFileRecordSet* survived = NULL;

	for (;;) {
		//All LHS segments modified
		if (i == s->data.end())
			break;

		short deleted_segment = -1;

		//All RHS segments applied
		if (i2 == s2->data.end()) {
			if (rhs_not) {
				survived = s;
				break;
			}

			//With basic 'AND' all remaining segments in the LHS set can be cleared
			delete i->second;
			deleted_segment = i->first;
		}

		else {
			short segnum = i->first;
			short segnum2 = i2->first;

			//LHS set has a segment not in the RHS set
			if (segnum < segnum2) {

				//Either keep or delete the whole segment
				if (rhs_not)
					survived = s;
				else {
					delete i->second;
					deleted_segment = i->first;
				}

				i++;
			}

			//RHS set has a segment not in the LHS set - it has no effect
			else if (segnum > segnum2)
				i2++;

			//A segment present in both sets
			else {

				//When just segment-restricting during FIND, this is enough to keep the seg
				if (seglevel)
					survived = s;

				else {
					//This might demote from a bitmap or even delete
					i->second = SegmentRecordSet::BitAnd_S(i->second, i2->second, rhs_not);

					if (i->second)
						survived = s;
					else
						deleted_segment = i->first;
				}

				i++;
				i2++;
			}
		}

		//Happens for various reasons above
		if (deleted_segment != -1) {
			s->data.erase(deleted_segment);

			//Repos iterator to next highest entry
			i = s->data.lower_bound(deleted_segment);
		}
	}

	//All segments got deleted
	if (!survived)
		delete s;

	return survived;
}

//***************************************************************************************
//Much simpler function for single record. Kept separate for this and performance reasons.
//***************************************************************************************
void BitMappedFileRecordSet::BitOr(int absrec)
{
	SegmentRecordSet_SingleRecord sing(absrec);
	short segnum = sing.SegNum();

	std::map<short, SegmentRecordSet*>::iterator i = data.find(segnum);

	//Segment already exists in set
	if (i != data.end()) {

		//Might result in promoting to a bitmap
		i->second = SegmentRecordSet::BitOr(i->second, &sing);
	}

	//Insert new segment
	else {
		SegmentRecordSet* singnew = sing.MakeCopy();

		try {
			data[segnum] = singnew;
		}
		catch (...) {
			delete singnew;
			throw;
		}
	}
}

//***************************************************************************************
//V3.0. As above but avoids some <map> some internal work and the find. Used in fastload.
//***************************************************************************************
void BitMappedFileRecordSet::FastAppend(int absrec)
{
	//We can cache this pointer because this function is only used when building up a set.  
	//I.e. we know segsets will not go away between calls like with UL REMOVE RECORD etc.
	if (append_hi_segset != NULL) {

		SegmentRecordSet_SingleRecord sing(absrec);

		short thisrec_segnum = sing.SegNum();
		short hi_segnum = append_hi_segset->SegNum();

		//The fastpath case
		if (hi_segnum == thisrec_segnum) {

			//Might result in promotion to bitmap
			SegmentRecordSet* promoted = SegmentRecordSet::BitOr(append_hi_segset, &sing);

			if (promoted != append_hi_segset) {
				append_hi_segset = promoted;
				data[thisrec_segnum] = promoted;
			}

			return;
		}
	}

	//This part caters for (a) first call when no cached ptr yet, (b) when the input record
	//is the first for a new seg, or (c) the user unhelpfully loads recs out of order.
	BitOr(absrec);

	std::map<short, SegmentRecordSet*>::iterator i = data.end();
	i--;
	append_hi_segset = i->second;
}

//********************************************************
void BitMappedFileRecordSet::BitOr(const ReadableRecord* r)
{
	BitOr(r->RecNum());
}

//***************************************************************************************
//Same comment as recordwise BitOR
BitMappedFileRecordSet* BitMappedFileRecordSet::BitAnd_S
(BitMappedFileRecordSet* s, const ReadableRecord* r, bool rhs_not)
{
	SegmentRecordSet_SingleRecord sing(r->RecNum());
	short segnum = sing.SegNum();

	std::map<short, SegmentRecordSet*>::iterator i = s->data.find(segnum);
	
	//The record's segment does not currently exist in the set
	if (i == s->data.end()) {
		if (rhs_not)
			return s;	//nothing changed
		else {
			delete s;	//nothing left
			return NULL;
		}
	}

	//See what's left after processing the segment
	i->second = SegmentRecordSet::BitAnd_S(i->second, &sing, rhs_not);
	if (i->second)
		return s; 

	//No records left in it
	s->data.erase(segnum);

	//Any segments left at all?
	if (s->NumSegs() == 0) {
		delete s;
		return NULL;
	}

	return s;
}

//***************************************************************************************
//An ugly function just to implement POINT$
//***************************************************************************************
BitMappedFileRecordSet* BitMappedFileRecordSet::Point$MaskOff
(BitMappedFileRecordSet* set, int absrec, bool mask_off_lt)
{
	if (!set)
		return NULL;

	short segnum = SegNumFromAbsRecNum(absrec);
	unsigned short relrec = RelRecNumFromAbsRecNum(absrec, segnum);

	if (mask_off_lt && relrec == 0)
		return set;
	if (!mask_off_lt && relrec >= (DBPAGE_SEGMENT_SIZE-1))
		return set;

	//Turn off bits before/after the record if we have its segment
	std::map<short, SegmentRecordSet*>* data = set->Data();
	std::map<short, SegmentRecordSet*>::iterator i = data->find(segnum);
	if (i != data->end()) {
		SegmentRecordSet* ss = i->second;

		bool emptied = false;

		if (ss->CastToSingle()) {
			unsigned short singrec = ss->CastToSingle()->RelRecNum();
			if (mask_off_lt)
				emptied = (singrec < relrec);
			else
				emptied = (singrec >= relrec);
		}
		else {
			util::BitMap* bmap = ss->CastToBitMap()->Data();

			if (mask_off_lt)
				bmap->SetRange(0, relrec-1, false);
			else
				bmap->SetRange(relrec, DBPAGE_SEGMENT_SIZE-1, false);

			int ct = bmap->Count();
			if (ct == 0)
				emptied = true;
			else if (ct == 1) {
				//No attempt to streamline this like there is in BitAnd()
				unsigned int lastrec;
				if (mask_off_lt)
					lastrec = relrec;
				else {
					lastrec = 0;
					bmap->FindNext(lastrec);
				}
				i->second = new SegmentRecordSet_SingleRecord(segnum, lastrec);
				delete ss;
			}
		}

		if (emptied) {
			delete ss;
			data->erase(i);

			//Did that delete the only segment?
			if (data->empty()) {
				delete set;
				return NULL;
			}
		}
	}
	
	return set;
}

//***************************************************************************************
int BitMappedFileRecordSet::RLCRec()
{
	SegmentRecordSet* firstseg = data.begin()->second;
	SegmentRecordSet_BitMap* bms = firstseg->CastToBitMap();

	unsigned short relrec;
	if (bms) {
		relrec = USHRT_MAX;
		bms->FindNextRelRec(relrec);
	}
	else
		relrec = firstseg->CastToSingle()->RelRecNum();

	return AbsRecNumFromRelRecNum(relrec, firstseg->SegNum());
}

//***************************************************************************************
//V2.17.  Used in the pre-merge phase of an index build.
//V3.0.   Was going to be used for fast unload (hence name) but is not now.  FU needs
//        to do reformatting, plus creating a BMSet is often unnecessary.  The two
//        functions do much the same though and should be kept in step if changed.
//***************************************************************************************
void BitMappedFileRecordSet::Unload(DU1SeqOutputFile* seq_file)
{
	//File-unique value
	int urn = SingleRecordNumber();
	if (urn != -1) {
		seq_file->SetNumSegs(0);
		seq_file->WriteURN(urn); 
		return;
	}

	seq_file->SetNumSegs(data.size());
	seq_file->WriteValueHeader(false);

	//Per-segment ILMRs
	std::map<short, SegmentRecordSet*>::iterator i;
	for (i = data.begin(); i != data.end(); i++) {

		short segnum = i->first;
		SegmentRecordSet* segset = i->second;

		SegmentRecordSet_SingleRecord* singset = segset->CastToSingle();
		if (singset) {
			unsigned short relrec = singset->RelRecNum();
			seq_file->WriteSegInvList(segnum, 1, &relrec);
		}
		else {
			SegmentRecordSet_BitMap* bmset = segset->CastToBitMap();
			if (bmset) {
				seq_file->WriteSegInvList(segnum, USHRT_MAX, bmset->Data()->Data());
			}
			else {
				SegmentRecordSet_Array* arrayset = segset->CastToArray();
				seq_file->WriteSegInvList(segnum, arrayset->Count(), arrayset->Array().Data());
			}
		}
	}
}








//***************************************************************************************
//***************************************************************************************
//***************************************************************************************
//Complete groupwide base class for lists and found sets
//***************************************************************************************
//***************************************************************************************
//***************************************************************************************
void BitMappedRecordSet::DestroySetData()
{
	std::map<int, BitMappedFileRecordSet*>::iterator i;
	for (i = data.begin(); i != data.end(); i++)
		delete i->second;

	data.clear();
}

//***************************************************************************************
int BitMappedRecordSet::Count() const
{
	int ct = 0;

	std::map<int, BitMappedFileRecordSet*>::const_iterator i;
	for (i = data.begin(); i != data.end(); i++)
		ct += i->second->Count();

	return ct;
}

//***************************************************************************************
BitMappedFileRecordSet* BitMappedRecordSet::GetFileSubSet(int membid) const
{
	std::map<int, BitMappedFileRecordSet*>::const_iterator i = data.find(membid);
	if (i == data.end())
		return NULL;
	else
		return i->second;
}

//***************************************************************************************
RecordSetCursor* BitMappedRecordSet::OpenCursor(bool gotofirst)
{
	BitMappedRecordSetCursor* c = new BitMappedRecordSetCursor(this);
	RegisterNewCursor(c);

	if (gotofirst)
		c->GotoFirst();

	return c;
}

//***************************************************************************************
void BitMappedRecordSet::ContextCheck(const BitMappedRecordSet* s) const
{
	DatabaseFileContext* c = Context();
	DatabaseFileContext* c2 = s->Context();

	if (c == c2)
		return;

	//Like User Language on M204, we can't place records from a single file context
	//onto a group list, even if the sfc is a member of the group.  The reason is that
	//the sfc itself would have no group file index so the merge would be impossible.
	//To use this function with single file sets the user should create them in group
	//context using the single-member special functions.
//	if (c->CastToSingle() || c2->CastToGroup())
		throw Exception(CONTEXT_MISMATCH, std::string
			("Context mismatch placing records on list: ")
			.append(c2->GetFullName()).append(" -> ")
			.append(c->GetFullName()));

	//Well actually we could write a special function to take the member ID from this
	//function and create tweak the file index in the bmset but that's a bit fiddly
	//so I'm not going to do it for now.  In UL the compiler enforces GROUP MEMBER
	//syntax to put member file lists on a group list, and that works fine, so any
	//change would only be for API users, and then it's debatable whether deviating 
	//from understood UL conventions is a good idea anyway.
//	c->CastToGroup()->GetGroupIndex(c2->CastToSingle(), "placing record set on list");
}

//***************************************************************************************
int BitMappedRecordSet::ContextCheck(SingleDatabaseFileContext* c2, const char* usage) const
{
	//This should never happen - only RecordCopy objects have no context
	if (c2 == NULL)
		throw Exception(CONTEXT_MISMATCH, 
		std::string("No context information ").append(usage));

	DatabaseFileContext* c = Context();

	if (c->CastToSingle() == c2)
		return 0;

	if (c->CastToSingle())
		throw Exception(CONTEXT_MISMATCH, std::string
			("Context mismatch ").append(usage).append(": ")
			.append(c2->GetFullName()).append(" -> ")
			.append(c->GetFullName()));

	//With a single record it's OK as we have separate functions to construct
	//the necessary set objects below.
	return c->CastToGroup()->GetGroupIndex(c2, usage);
}
 
//***************************************************************************************
//List PLACE RECORDS ON
//***************************************************************************************
void BitMappedRecordSet::BitOr(const BitMappedRecordSet* s2)
{
	if (s2 == this)
		return;

	ContextCheck(s2);

	//Still reposition when placing records on the list, because the map data
	//may well get changed, and that will require relocating the iterators.
	RequestRepositionCursors(12);

	//Use a merge-style approach with the two sets
	std::map<int, BitMappedFileRecordSet*>::iterator i = data.begin();
	std::map<int, BitMappedFileRecordSet*>::const_iterator i2 = s2->data.begin();

	try {
		for (;;) {
			//No more file sets to place on the old list
			if (i2 == s2->data.end())
				return;

			//Decide if it's the same file by comparing group index keys
			int membid2 = i2->first;
			int membid;
			if (i != data.end())
				membid = i->first;
			else
				membid = INT_MAX;

			//Same key - OR the new set data
			if (membid == membid2) {
				i->second->BitOr(i2->second);
				i++;
				i2++;
			}

			//Nothing to add in this file
			else if (membid < membid2)
				i++;

			//A whole file set must be added 
			else {
				BitMappedFileRecordSet* newfset = i2->second->MakeCopy();
				try {
					data[membid2] = newfset;
				}
				catch (...) {
					delete newfset;
					throw;
				}

				//Relocate iterator after that map work
				i = data.find(membid2);
				i++;
				i2++;
			}
		}
	}

	//It will probably be a memory problem if that throws.  If so we clear the list.
	//This is a compromise between the desirability of modifying the target list in place
	//and the undesirability of ending up with a half-modified list. (That is, the 
	//alternative algorithm would create and modify a new set, then delete the old). If APSY
	//is being used UL applications can catch MEMORY and proceed, and the list could be 
	//global, so this is not totally academic in the UL situation.
	catch (...) {
		Clear();
		throw;
	}
}

//***************************************************************************************
//List REMOVE RECORDS and also used in the find engine.
//***************************************************************************************
void BitMappedRecordSet::BitAnd_S(const BitMappedRecordSet* s2, bool rhs_not)
{
	if (s2 == this) {
		if (rhs_not) {
			RequestRepositionCursors(12);
			Clear();
		}
		return;
	}

	ContextCheck(s2);
	RequestRepositionCursors(12);

	//Use a merge-style approach with the two sets
	std::map<int, BitMappedFileRecordSet*>::iterator i = data.begin();
	std::map<int, BitMappedFileRecordSet*>::const_iterator i2 = s2->data.begin();

	try {
		for (;;) {

			//All LHS files modified
			if (i == data.end())
				break;

			int deleted_membid = -1;

			//All RHS files applied
			if (i2 == s2->data.end()) {
				if (rhs_not)
					break;

				//With basic 'AND' all remaining files in the LHS set can be cleared
				delete i->second;
				deleted_membid = i->first;
			}

			else {
				int membid = i->first;
				int membid2 = i2->first;

				//LHS set has a file not in the RHS set
				if (membid < membid2) {

					//Either keep or delete the whole file
					if (!rhs_not) {
						delete i->second;
						deleted_membid = i->first;
					}

					i++;
				}

				//RHS set has a file not in the LHS set - it has no effect
				else if (membid > membid2)
					i2++;

				//A file present in both sets
				else {

					//This might delete
					i->second = BitMappedFileRecordSet::BitAnd_S(i->second, i2->second, rhs_not);

					if (!i->second)
						deleted_membid = i->first;

					i++;
					i2++;
				}
			}		

			//Happens for various reasons above
			if (deleted_membid != -1) {
				data.erase(deleted_membid);

				//Repos iterator to next highest entry
				i = data.lower_bound(deleted_membid);
			}
		}
	}

	//See setwise BitOr - much much less likely in this case
	catch (...) {
		Clear();
		throw;
	}
}

//***************************************************************************************
void BitMappedRecordSet::BitOr(const ReadableRecord* r)
{
	int membid = ContextCheck(r->HomeFileContext(), "adding single record to list");
	RequestRepositionCursors(12);

	std::map<int, BitMappedFileRecordSet*>::iterator i = data.find(membid);

	try {
		//The record's home file already exists in set
		if (i != data.end())
			i->second->BitOr(r);

		//Create new file set
		else {
			BitMappedFileRecordSet* newfset = new BitMappedFileRecordSet(r->HomeFileContext());
			try {
				newfset->BitOr(r);
				data[membid] = newfset;
			}
			catch (...) {
				delete newfset;
				throw;
			}
		}
	}
	//See setwise BitOr
	catch (...) {
		Clear();
		throw;
	}
}

//***************************************************************************************
void BitMappedRecordSet::BitAnd_S(const ReadableRecord* r, bool rhs_not)
{
	int membid = ContextCheck(r->HomeFileContext(), "removing single record from list");
	RequestRepositionCursors(12);

	std::map<int, BitMappedFileRecordSet*>::iterator i = data.find(membid);

	//The record's home file does not exist in the set
	if (i == data.end()) {
		if (rhs_not)
			return;		//nothing changed
		else {
			Clear();	//nothing left
			return;
		}
	}

	//See what's left after processing the file
	try {
		i->second = BitMappedFileRecordSet::BitAnd_S(i->second, r, rhs_not);
		if (i->second)
			return;

		//The file set got emptied and deleted
		data.erase(membid);
	}

	//See setwise BitOr - much much less likely in this case
	catch (...) {
		Clear();
		throw;
	}
}

//***************************************************************************************
//Group member context CLEAR statement in UL
void BitMappedRecordSet::ClearMember_B(SingleDatabaseFileContext* member)
{
	int membid = ContextCheck(member, "clearing single file member component of list");

	std::map<int, BitMappedFileRecordSet*>::iterator i = data.find(membid);
	if (i == data.end())
		return;

	delete i->second;
	data.erase(i);

	//At the very least this will work like a setwise remove, but if there's nothing 
	//left we treat it like a full clear, which means RELEASE

//to check on M204 - this seems klugey but I can't clearly see a better way

	if (data.size() == 0)
		RequestRepositionCursors(11);
	else
		RequestRepositionCursors(12);
}

//***************************************************************************************
SortRecordSet* BitMappedRecordSet::Sort(SortRecordsSpecification& sortspec)
{
	if (sortspec.record_count_limit == -1) //all requested
		sortspec.record_count_limit = Count();

	int num_requested_fields = sortspec.fields.size();

	//Make an initial check on field names for existence and visibility
	int num_keys = 0;
	int num_each_keys = 0;
	std::vector<int> each_key_fixs;

	for (int fix = 0; fix < num_requested_fields; fix++) {
		SortRecordsFieldSpec& specfld = sortspec.fields[fix];

		FieldAttributes atts = Context()->GetFieldAtts(specfld.name);

		if (atts.IsInvisible()) {
			//Use the same text as record operations
			std::string msg("Run-time fieldname (");
			msg.append(specfld.name);
			msg.append("): requested operation requires visible field(s)");

			throw Exception(DML_INVALID_INVIS_FUNC, msg);
		}

		//Also decide on the order for key fields where they requested default order
		if (specfld.key_type == SORT_DEFAULT_TYPE) {
			if (atts.IsFloat())
				specfld.key_type = SORT_NUMERIC;
			else
				specfld.key_type = SORT_CHARACTER;
		}

		if (specfld.key_type != SORT_NONE) {
			specfld.kix = num_keys;
			num_keys++;

			if (specfld.sort_by_each) {
				specfld.ekix = num_each_keys;
				num_each_keys++;
				each_key_fixs.push_back(fix);
			}
		}
	}

	if (num_keys == 0)
		throw Exception(DML_BAD_SORT_SPEC, "Sort requires at least one sort key field");
	if (num_keys > 100) //64k would be current limit because of int16 
		throw Exception(DML_BAD_SORT_SPEC, "More than 100 sort keys specified");

	SortRecordSet* sortset = Context()->CreateSortRecordSet(sortspec.sort_keys_only);
	Context()->DBAPI()->IncStatSORTS();

	//This will save a lot of vector expansions as we build the set
	sortset->ReserveMemory(sortspec.record_count_limit);

	try {
		SingleDatabaseFileContext* home = NULL;
		std::map<FieldID, int> specfidtable;

		//Loop on the base set and collect the appropriate number of records
		RecordSetCursorHandle rsch(this);
		for (int ct = 0; 
			rsch.CanEnterLoop() && ct < sortspec.record_count_limit; 
			rsch.Advance(1), ct++) 
		{
			Record* rec = rsch.AccessCurrentRealRecord();
			SingleDatabaseFileContext* sfc = rec->HomeFileContext();
			int recnum = rec->RecNum();

			//I think this is the meaning of STRECDS.  Note that as currently coded RECDS
			//will also have been incremented by the record set cursor Advance() above.
			//It is done for each record inside this loop, because the home file may vary.
			//Also note that when SORT BY EACH is in effect the sort algorithm will process
			//more records.
			sfc->GetDBFile()->IncStatSTRECDS(sfc->DBAPI());

			//When we move to a new group member the field IDs will be different
			if (sfc != home) {
				home = sfc;

				//So rebuild the local partial ID table (used for performance reasons)
				specfidtable.clear();
				for (int fix = 0; fix < num_requested_fields; fix++) {
					PhysicalFieldInfo* pfi = DatabaseFileFieldManager::GetAndValidatePFI
						(rec->HomeFileContext(), sortspec.fields[fix].name, false, false, false);

					//No probs if a group member is missing the field - it will not appear
					if (pfi == NULL)
						continue;

					specfidtable[pfi->id] = fix;
					sortspec.fields[fix].fid = pfi->id;
				}
			}

			//Empty sort record - tell it the real physical record number and home file
			SortRecord* sortrec = new SortRecord(home, recnum);
			sortrec->InitializeKeys(num_keys);

			//Temporary holding area for any EACH keys - dealt with separately at the end
			std::vector<std::vector<FieldValue> > each_key_vals(num_each_keys);

			try {
				FieldID fid;
				FieldValue fval;
				std::vector<int> collected(num_requested_fields, 0);

				//Do a PAI-style loop on FV pairs in the record
				int fvpix = 0;

				//V3.0. Altered the signature.  Note that we collect full BLOB values for the
				//sort record (after all we may want to sort by them).
				//while (rec->GetNextFVPair_ID(fid, fval, fvpix)) { 
				while (rec->GetNextFVPair_ID(&fid, &fval, NULL, fvpix)) {
				
					bool is_key = false;

//					std::string tempval = fval.ExtractString();

					//Do we want this field?  Look up in the field ID map built earlier.
					std::map<FieldID, int>::const_iterator fi = specfidtable.find(fid);
					if (fi == specfidtable.end()) {
						if (!sortspec.collect_all_fields)
							continue;
					}
					else {
						int fix = fi->second;

						collected[fix]++;
						SortRecordsFieldSpec& specfld = sortspec.fields[fix];

						//Have we already collected enough occs of it?
						if (!sortspec.collect_all_fields 
							&& collected[fix] > 1 
							&& !specfld.collect_all_occs)
								continue;

						//Is it a sort key or just for viewing?
						if (specfld.key_type != SORT_NONE) {

							//EACH key
							if (specfld.sort_by_each) {
								is_key = true;
								each_key_vals[specfld.ekix].push_back(fval);
							}

							//Normal key - only the first occurrence is used for sorting
							else if (collected[fix] == 1) {
								is_key = true;
								sortrec->AppendKey(specfld, fid, fval);
							}
						}
					}

					if (!is_key)
						sortrec->AppendNonKey(fid, fval);
				}

				//Deal with EACH keys at the end of the record
				if (num_each_keys > 0) 
					sortset->GenerateAndAppendEachKeyRotations
								(sortrec, &sortspec, &each_key_fixs, &each_key_vals);
				else
					sortset->AppendRec(sortrec);
			}
			catch (...) {
				delete sortrec;
				throw;
			}
		}

		sortset->PerformSort();
	}
	catch (...) {
		//V3.0.  Oops!  Direct deletion leaves hanging pointer in the set.
		//delete sortset;
		Context()->DestroyRecordSet(sortset);
		throw;
	}

	return sortset;
}

//***************************************************************************************
//V2.25.  Can be handy for diagnostics.
FindEnqueueType BitMappedRecordSet::LockType()
{
	//This function does not make complete sense because if there are no file sets we
	//can't look for a record lock - there won't be one.  Even though the user might have 
	//requested a SHR lock and found zero records.
	if (data.size() == 0)
		return FD_LOCK_NONE;
	BitMappedFileRecordSet* set0 = data.begin()->second;
	return set0->LockType();
}

//***************************************************************************************
//V3.0.  See comments in RecordSet.
void BitMappedRecordSet::GetRecordNumberArray_D(int* dest, int getmaxrecs)
{
	if (data.size() == 0)
		return;

	//Group contexts are invalid here, so the first member context is always right
	FileRecordSet* first_member = data.begin()->second;

	first_member->GetRecordNumberArray(dest, getmaxrecs);
}


/*
***************************************************************************************
I wanted to put as much of the overhead onto the deleter as possible and give this
set every chance to avoid having to do the extra EBP work, but decided to just flag
the flag in the end.  The reason is that we already have one extra lock to handle 
dirty delete (stops new sets being created and destroyed while the notify takes place)
and if we were going to do the processing below we would also then have to place a
lock on bmsets even as they were being modified normally (since this code uses the
internal structure of the set.  That's becoming too much extra locking for my taste
just to handle dirty delete.
At the end of the day dirty delete is a rarely used facility and it's just as well to
keep it simple anyway.
What I have done is put a check in the Foundset code so that only unlocked sets have
the flag on.  Lists and sorted sets are always unlocked so they must have it on.

See related comments in Record::CheckEBM()
***************************************************************************************
void BitMappedRecordSet::NotifyOfDirtyDelete(BitMappedRecordSet* goners)
{
	if (affected_by_dirty_delete)
		return;

	std::map<int, BitMappedFileRecordSet*>::iterator i;

	for (i = data.begin(); i != data.end(); i++) {
		std::map<int, BitMappedFileRecordSet*>::iterator i2 = goners->data.find(i->first);
		if (i2 == goners->data.end())
			continue;

		if (i->second->AnyIntersection(i2->second)) {
			affected_by_dirty_delete = true;
			break;
		}
	}
}
*/






//***************************************************************************************
//***************************************************************************************
//***************************************************************************************
//Cursor for lists and found sets
//***************************************************************************************
//***************************************************************************************
//***************************************************************************************
BitMappedRecordSetCursor::BitMappedRecordSetCursor(BitMappedRecordSet* s)
: RecordSetCursor(s)
{
	SetAtEnd();
}

//***************************************************************************************
bool BitMappedRecordSetCursor::RegisterValidSetPosition(bool advanced)
{
	int recnum = AbsRecNumFromRelRecNum(relrecnum_ix, segset_bookmark->second->SegNum());
	SingleDatabaseFileContext* sfc = fileset_bookmark->second->HomeFileContext();

	assert(sfc);
	SetLARInfo(recnum, sfc);

	if (advanced)
		sfc->GetDBFile()->IncStatRECDS(sfc->DBAPI());

	needs_reposition = false;
	return advanced;
}

//***************************************************************************************
void BitMappedRecordSetCursor::GotoFirst()
{
	SetAtEnd();

	//Since empty sets are not created, or are eliminated as they occur during removal
	//of records from lists, we know the first file set, if there is one, has a record.
	fileset_bookmark = BMSet()->data.begin();
	if (fileset_bookmark == BMSet()->data.end())
		return;

	GetInitialIxValues(true);
	RegisterValidSetPosition(true);
}

//***************************************************************************************
void BitMappedRecordSetCursor::GotoLast()
{
	SetAtEnd();

	if (BMSet()->data.empty())
		return;

	//Since there is at least one element, this is sound
	fileset_bookmark = BMSet()->data.end();
	fileset_bookmark--;

	GetInitialIxValues(false);
	RegisterValidSetPosition(true);
}

//***************************************************************************************
void BitMappedRecordSetCursor::GetInitialIxValues(bool first)
{
	fileset_ix = fileset_bookmark->first;

	if (first)
		segset_bookmark = fileset_bookmark->second->Data()->begin();
	else {
		//Since we know there is at least one element, this is sound
		segset_bookmark = fileset_bookmark->second->Data()->end();
		segset_bookmark--;
	}

	segset_ix = segset_bookmark->first;

	GetInitialRecNumIx(first);
}

//***************************************************************************************
void BitMappedRecordSetCursor::GetInitialRecNumIx(bool first)
{
	SegmentRecordSet_BitMap* bms = segset_bookmark->second->CastToBitMap();
	if (bms) {
		relrecnum_ix = USHRT_MAX;
		if (first)
			bms->FindNextRelRec(relrecnum_ix);
		else
			bms->FindPrevRelRec(relrecnum_ix);
	}
	else {
		SegmentRecordSet_SingleRecord* sing = segset_bookmark->second->CastToSingle();
		relrecnum_ix = sing->RelRecNum();
	}
}

//***************************************************************************************
//Normally we use iterators to do the loop, but since adding/removing map entries will 
//invalidate the iterators we have to re-derive them from their keys at these moments.
//***************************************************************************************
bool BitMappedRecordSetCursor::RecoverPositionWithAdvance()
{
	if (!needs_reposition) 
		return false;

	ClearSetPositionInfo();

	//Locate file where the cursor was last, or next after it
	fileset_bookmark = BMSet()->data.lower_bound(fileset_ix);

	//Was last file in set
	if (fileset_bookmark == BMSet()->data.end()) {
		SetAtEnd();
		return false;
	}
	
	//File gone but there is another
	if (fileset_bookmark->first > fileset_ix) {
		GetInitialIxValues(true);
		return RegisterValidSetPosition(true);
	}

	//--------------
	//File still there - locate seg where cursor was last, or next after it
	segset_bookmark = fileset_bookmark->second->Data()->lower_bound(segset_ix);

	//No more segs in file
	if (segset_bookmark == fileset_bookmark->second->Data()->end()) {

		//Try next file
		fileset_bookmark++;
		if (fileset_bookmark == BMSet()->data.end()) {
			SetAtEnd();
			return false;
		}

		GetInitialIxValues(true);
		return RegisterValidSetPosition(true);
	}

	//Seg gone but there is another in same file
	if (segset_bookmark->first > segset_ix) {
		segset_ix = segset_bookmark->first;
		GetInitialRecNumIx(true);
		return RegisterValidSetPosition(true);
	}

	//--------------
	//Seg still there - locate the record where the cursor was last
	bool fully_repositioned_ok;
	unsigned short seg_single_rec;
	SegmentRecordSet_BitMap* seg_bitmap = segset_bookmark->second->CastToBitMap();
	if (seg_bitmap) 
		fully_repositioned_ok = seg_bitmap->ContainsRelRecNum(relrecnum_ix);
	else {
		seg_single_rec = segset_bookmark->second->CastToSingle()->RelRecNum();
		fully_repositioned_ok = (seg_single_rec == relrecnum_ix);
	}
	
	//The record we were at before is still in the set (hopefully most common case)
	if (fully_repositioned_ok)
		return RegisterValidSetPosition(false);

	//The record has gone: the convention is that the cursor is automatically positioned
	//at the next record in a *forward* direction.  This reflects the fact that cursors
	//in DPT have no intrinsic direction - forward here is arbitrary but most natural,
	//and consistent with the above file/seg repositioning.

	//V1.2 25/7/06
	//It is possible that the current record has been removed but one of the records 
	//after it in the segment remains as a singleton.  There would be no way to alter 
	//the FindNextRelRec function for the singlerec set variant to handle this, so make 
	//an explicit test here.
	if (!seg_bitmap && seg_single_rec > relrecnum_ix) {
		relrecnum_ix = seg_single_rec;
		return RegisterValidSetPosition(true);
	}

	else if (Advance_1(true))
		return RegisterValidSetPosition(true);

	//We must have been at the last record in the set when it was removed
	return false;
}

//***************************************************************************************
void BitMappedRecordSetCursor::Advance(int n)
{
	//V2.06 Jul 07.  Since there are several return points below.
	AdvanceMain(n);
	PostAdvance(n);
}

void BitMappedRecordSetCursor::AdvanceMain(int n)
{
	bool advanced_during_recover = RecoverPositionWithAdvance();
	if (advanced_during_recover) {
		if (n == 1)
			return; //all done
		else if (n > 0)
			n--;
	}

	if (!CanEnterLoop())
		return;

	//Must check if this increases RECDS on M204s - see also sorted set equivalent.
	if (n == 0)
		return;

	bool forward = (n > 0);
	int iters = (forward) ? n : -n;

	//No other way to do it really than one record at a time with this data structure
	for (int x = 0; x < iters; x++) {
		if (!Advance_1(forward)) {
			ClearSetPositionInfo();
			return;
		}
	}

	//Assuming only 1 RECDS increment if skip value is > 1
	RegisterValidSetPosition(true);
}

//***************************************************************************************
bool BitMappedRecordSetCursor::Advance_1(bool forward)
{
	//First try for the next/previous bit in the current file and segment
	bool more_recs;
	if (forward)
		more_recs = segset_bookmark->second->FindNextRelRec(relrecnum_ix);
	else
		more_recs = segset_bookmark->second->FindPrevRelRec(relrecnum_ix);

	if (more_recs)
		return true;

	//-------------
	//No more records in that segment
	bool more_segs;
	if (forward) {
		segset_bookmark++;
		more_segs = (segset_bookmark != fileset_bookmark->second->Data()->end());
	}
	else {
		//NB.  Can't use rend() with a forward iterator, even a bidirectional one
		more_segs = (segset_bookmark != fileset_bookmark->second->Data()->begin());
		if (more_segs)
			segset_bookmark--;
	}

	if (more_segs) {
		segset_ix = segset_bookmark->first;
		GetInitialRecNumIx(forward);
		return true;
	}

	//--------------------
	//No more records in that file
	bool more_files;
	if (forward) {
		fileset_bookmark++;
		more_files = (fileset_bookmark != BMSet()->data.end());
	}
	else {
		//See iterator comment above
		more_files = (fileset_bookmark != BMSet()->data.begin());
		if (more_files)
			fileset_bookmark--;
	}

	if (more_files) {
		GetInitialIxValues(forward);
		return true;
	}

	//--------------------
	//No more files, so that's the end of the whole set
	SetAtEnd();
	return false;
}

//***************************************************************************************
RecordSetCursor* BitMappedRecordSetCursor::CreateClone()
{
	RecordSetCursor* clone = BMSet()->OpenCursor();

	//In most cases the clone could start processing at the same place
	ExportPosition(clone);

	return clone;
}

//***************************************************************************************
void BitMappedRecordSetCursor::ExportPosition(RecordSetCursor* clone)
{
	BitMappedRecordSetCursor* cast = static_cast<BitMappedRecordSetCursor*>(clone);

	cast->fileset_ix = fileset_ix;
	cast->segset_ix = segset_ix;
	cast->relrecnum_ix = relrecnum_ix;

	cast->RequestReposition(12);

}

//***************************************************************************************
void BitMappedRecordSetCursor::TBSReposition(std::map<short, SegmentRecordSet*>* frsdata)
{
	//This saves rescanning the first bitmap after dropping a segment, as would have
	//been done by the alternative which was to just GotoFirst() again.
	segset_bookmark = frsdata->begin();
	segset_ix = segset_bookmark->first;
}	

} //close namespace


