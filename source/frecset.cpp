
#include "stdafx.h"

#include "frecset.h"

//Utils
#include "bitmap3.h"
//API Tiers
#include "dbf_rlt.h"
#include "pagebitmap.h"
#include "page_v.h" //#include "page_V.h" : V2.24 case is less interchangeable on *NIX - Roger M.
//Diagnostics
#include "msg_db.h"

namespace dpt {

//***************************************************************************************
bool SegmentRecordSet::AnyIntersection(const SegmentRecordSet* s2) const 
{
	if (s2 == this)
		return true;

	//We can apply some intelligence here to avoid bitmap AND work
	const SegmentRecordSet_SingleRecord* sing = CCastToSingle();
	const SegmentRecordSet_SingleRecord* sing2 = s2->CCastToSingle();
	
	//Both single records - easy
	if (sing && sing2)
		return sing->IsSameRelRecNum(sing2);

	const SegmentRecordSet_BitMap* bmap = CCastToBitMap();
	const SegmentRecordSet_BitMap* bmap2 = s2->CCastToBitMap();

	//Both bitmaps
	if (bmap && bmap2)
		return bmap->AnyIntersection(bmap2);

	//One of each type
	if (bmap)
		return bmap->ContainsRelRecNum(sing2->RelRecNum());
	else
		return bmap2->ContainsRelRecNum(sing->RelRecNum());
}


//***************************************************************************************
//V2.14 Jan 09.  See comments in header file.  This BitOr function alone is supported 
//for array-style segment sets, because it gets used in loads.  Actually ORing two 
//array lists in the general situation is tricky as both sets must be sorted and then
//the merge code does not have a trivial cost either.
//***************************************************************************************
SegmentRecordSet* SegmentRecordSet::BitOr
(SegmentRecordSet* s1, const SegmentRecordSet* s2)
{
	if (s1 == s2)
		return s1;

	SegmentRecordSet_SingleRecord* sing1 = s1->CastToSingle();
	const SegmentRecordSet_SingleRecord* sing2 = s2->CCastToSingle();

	//May be nothing to do in this simple case
	if (sing1 && sing2) {
		if (sing1->RelRecNum() == sing2->RelRecNum())
			return s1;
	}

	SegmentRecordSet_BitMap* bmap1 = s1->CastToBitMap();
	const SegmentRecordSet_BitMap* bmap2 = s2->CCastToBitMap();

	//Promote s1 to bitmap if not already.
	if (!bmap1) {
		bmap1 = new SegmentRecordSet_BitMap(s1->segnum);

		//Copy in the single existing record if it was a single record set
		if (sing1) {
			bmap1->Data()->Set(sing1->RelRecNum());
			delete sing1;
		}

		//V2.14 Jan 09. Or the records from the array if it was an array set.
		else {
			bmap1->RecOr(s1->CastToArray());
			delete s1->CastToArray();
		}
	}

	//Then OR on the records in set 2
	if (sing2)
		bmap1->Data()->Set(sing2->RelRecNum());

	else if (bmap2)
		*(bmap1->Data()) |= *(bmap2->CData());

	//V2.14 Jan 09.
	else
		bmap1->RecOr(s2->CCastToArray());

	return bmap1;
}

//***************************************************************************************
SegmentRecordSet* SegmentRecordSet::BitAnd_S
(SegmentRecordSet* s, const SegmentRecordSet* s2, bool rhs_not)
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
		return s;
	}

	SegmentRecordSet_SingleRecord*			sing = s->CastToSingle();
	SegmentRecordSet_BitMap*				bmap = s->CastToBitMap();
	const SegmentRecordSet_SingleRecord*	sing2 = s2->CCastToSingle();
	const SegmentRecordSet_BitMap*			bmap2 = s2->CCastToBitMap();

	//Case 1: The LHS is a singleton
	if (sing) {

		bool lhsrec_covered;

		//1a. RHS is singleton too
		if (sing2)
			lhsrec_covered = (sing->RelRecNum() == sing2->RelRecNum());

		//1b. RHS is multiple
		else
			lhsrec_covered = bmap2->CData()->Test(sing->RelRecNum());

		//Old record survives if (AND same record, or AND NOT different record)
		if (lhsrec_covered != rhs_not)
			return s;		//survived
		else {
			delete s;
			return NULL;
		}
	}

	bool needs_demote = false;

	//--------------------------------
	//Case 2: LHS is a bitmap
	util::BitMap* bits = bmap->Data();
	unsigned short relrec;

	//2a. RHS is a singleton
	if (sing2) {
		relrec = sing2->RelRecNum();
		bool rhsrec_present = bits->Test(relrec);

		if (rhsrec_present) {

			//The LHS will be getting smaller somehow
			if (rhs_not)
				bits->Reset(relrec);	//remove one
			else {
				bits->ResetAll();		//remove all
				bits->Set(relrec);		//except one
				needs_demote = true;	//therefore definitely demote below
			}
		}
		else {
			if (rhs_not)
				return s;				//rec not present - remove unnecessary
			else {
				delete s;				//no intersection - nothing would be left
				return NULL;
			}
		}
	}

	//2b. Both bitmaps - use machine bitwise operations
	else {
		const util::BitMap* bits2 = bmap2->CData();

		if (rhs_not)
			*bits /= *bits2;
		else
			*bits &= *bits2;
	}

	//Now demote to a single version if appropriate.  The locating func is used instead of 
	//Count() so that we can find out the record number if there's only one left.  This
	//should be faster in any case - no need to scan the whole bitmap.
	if (!needs_demote) {
		relrec = USHRT_MAX;

		if (!bmap->FindNextRelRec(relrec)) {
			delete bmap; //something above has made it empty
			return NULL;
		}

		//If there's another bit set in there, it survives as a bitmap
		if (!bmap->FindNextRelRec(relrec))
			needs_demote = true;
	}

	if (!needs_demote)
		return bmap;

	//The first recnum was the only one left - demote to single type
	sing = new SegmentRecordSet_SingleRecord(bmap->SegNum(), relrec);
	delete bmap;
	return sing;
}



//***************************************************************************************
//***************************************************************************************
//***************************************************************************************
SegmentRecordSet_BitMap::SegmentRecordSet_BitMap(short s) 
: SegmentRecordSet(s), data(new util::BitMap(DBPAGE_SEGMENT_SIZE)), own_data(true) 
{}

//***************************************************************************************
SegmentRecordSet_BitMap::SegmentRecordSet_BitMap(short s, BitMapFilePage& p) 
: SegmentRecordSet(s), filepage(&p), own_data(false)
{
	data = &(p.Data());
}

//***************************************************************************************
//V2.14 Jan 09.  Used during one step loads.
SegmentRecordSet_BitMap::SegmentRecordSet_BitMap(short s, util::BitMap* adoptee) 
: SegmentRecordSet(s), data(adoptee), own_data(true)
{}

//***************************************************************************************
SegmentRecordSet* SegmentRecordSet_BitMap::MakeCopy(bool donate) const
{
	SegmentRecordSet_BitMap* newsegset;

	//V2.14 Jan 09.  Bypass the copy if we know the RHS is no longer needed.
	if (donate) {
		newsegset = new SegmentRecordSet_BitMap(segnum, data);
		own_data = false;
	}
	else {
		newsegset = new SegmentRecordSet_BitMap(segnum);
		newsegset->data->CopyBitsFrom(*data);
	}

	return newsegset;
}


//***************************************************************************************
SegmentRecordSet_BitMap::~SegmentRecordSet_BitMap() 
{
	if (own_data)
		delete data;
}

//***************************************************************************************
unsigned short SegmentRecordSet_BitMap::Count() const
{
	if (own_data)
		return data->Count();
	else
		return filepage->SetCount(); //don't know if this gets used - ANALYZE maybe
}

//***************************************************************************************
bool SegmentRecordSet_BitMap::AnyIntersection(const SegmentRecordSet_BitMap* s) const
{
	if (s == this)
		return true;

	return data->AnyIntersection(*(s->data));
}

//***************************************************************************************
bool SegmentRecordSet_BitMap::ContainsRelRecNum(unsigned short relrecnum) const
{
	return data->Test(relrecnum);
}

//***************************************************************************************
bool SegmentRecordSet_BitMap::FindNextRelRec(unsigned short& relrec) const
{
	unsigned short nextbit;

	if (relrec == USHRT_MAX)
		nextbit = 0;
	else {
		nextbit = relrec + 1;
		if (nextbit >= DBPAGE_SEGMENT_SIZE)
			return false; //bitmap func would throw
	}

	//Convert parm: [ushort<->uint] so that the general bitmap func can be used directly
	unsigned int irelrec = relrec;
	if (!data->FindNext(irelrec, nextbit))
		return false;

	relrec = irelrec;
	return true;
}

//***************************************************************************************
bool SegmentRecordSet_BitMap::FindPrevRelRec(unsigned short& relrec) const
{
	unsigned short prevbit;
	if (relrec >= DBPAGE_SEGMENT_SIZE)
		prevbit = DBPAGE_SEGMENT_SIZE - 1;
	else {
		if (relrec == 0)
			return false;
		prevbit = relrec - 1;
	}
	
	//Convert parm: [short<->int] so that the general bitmap func can be used directly
	unsigned int irelrec = relrec;
	if (!data->FindPrev(irelrec, prevbit))
		return false;

	relrec = irelrec;
	return true;
}

//***************************************************************************************
void SegmentRecordSet_BitMap::RecOr(const SegmentRecordSet_Array* arrayset)
{
	for (int x = arrayset->CArray().NumEntries() - 1; x >= 0; x--)
		data->Set(arrayset->CArray().GetEntry(x));
}





//***************************************************************************************
SegmentRecordSet_Array::SegmentRecordSet_Array
(const short s, unsigned short initial_num_entries) 
: SegmentRecordSet(s)
{
	array.InitializeAndReserve(initial_num_entries);
}

SegmentRecordSet_Array::SegmentRecordSet_Array
(const short s, unsigned short* mem, void* heap) 
: SegmentRecordSet(s), array(mem, heap, true)
{}

//****************
SegmentRecordSet* SegmentRecordSet_Array::MakeCopy(bool donate) const 
{
	SegmentRecordSet_Array* newsegset;

	//Bypass the copy if we know the RHS is no longer needed.
	if (donate) {
		newsegset = new SegmentRecordSet_Array(segnum, array.AdoptMemFrom(), array.Heap());
	}
	else {
		unsigned short myentrycount = array.NumEntries();
		newsegset = new SegmentRecordSet_Array(segnum, myentrycount);
		newsegset->array.CopyEntriesFrom((void*)array.CData(), myentrycount);
	}

	return newsegset;
}



//***************************************************************************************
//***************************************************************************************
//***************************************************************************************
void FileRecordSet::DestroySetData()
{
	std::map<short, SegmentRecordSet*>::iterator i;
	for (i = data.begin(); i != data.end(); i++)
		delete i->second;

	data.clear();
}

//***************************************************************************************
bool FileRecordSet::AnyIntersection(const FileRecordSet* s2) const
{
	if (s2 == this)
		return true;

	//Check overlaps only where the segments match
	std::map<short, SegmentRecordSet*>::const_iterator i;
	for (i = data.begin(); i != data.end(); i++) {

		std::map<short, SegmentRecordSet*>::const_iterator i2;
		i2 = s2->data.find(i->first);

		if (i2 == s2->data.end())
			continue;

		//There is a matching segment
		if (i->second->AnyIntersection(i2->second))
			return true;
	}

	return false;
}

//***************************************************************************************
//Jul 09, in prep for V3.0.  Used in fast unload.
SegmentRecordSet* FileRecordSet::GetSegmentSubSet(short segnum) const
{
	std::map<short, SegmentRecordSet*>::const_iterator i = data.find(segnum);
	if (i == data.end())
		return NULL;
	else
		return i->second;
}

//***************************************************************************************
FindEnqueueType FileRecordSet::LockType() 
{
	if (lock == NULL)
		return FD_LOCK_NONE;
	if (lock->IsExcl())
		return FD_LOCK_EXCL;
	return FD_LOCK_SHR;
}

//***************************************************************************************
//V3.0.  See RecordSet.
void FileRecordSet::GetRecordNumberArray(int* dest, int getmaxrecs)
{
	//The caller should have prepared an array with exactly the right number of elements
	std::map<short, SegmentRecordSet*>::iterator i;
	for (i = data.begin(); i != data.end(); i++) {

		SegmentRecordSet* segset = i->second;

		SegmentRecordSet_SingleRecord* singset = segset->CastToSingle();
		if (singset) {
			*dest = singset->AbsRecNum();
			dest++;
		}
		else {
			short segnum = i->first;
			util::BitMap* bitmap = segset->CastToBitMap()->Data();

			for (unsigned int bit = 0; bit < (unsigned int)DBPAGE_BITMAP_SIZE; bit++) {
				if (!bitmap->FindNext(bit, bit))
					break;

				if (getmaxrecs == 0)
					return;
				else
					getmaxrecs--;

				*dest = AbsRecNumFromRelRecNum(bit, segnum);
				dest++;
			}
		}
	}
}



} //close namespace


