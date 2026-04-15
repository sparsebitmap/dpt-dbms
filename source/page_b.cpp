
#include "stdafx.h"

#include "page_b.h" //#include "page_B.h" : V2.24 case is less interchangeable on *NIX - Roger M.

//Utils
//API Tiers
#include "fieldval.h"
#include "fieldinfo.h"
#include "update.h"
#include "dbserv.h"
//Diagnostics
#include "msg_db.h"
#include "except.h"

namespace dpt {

//**************************************************************************************
RecordDataPage::RecordDataPage(BufferPageHandle& bh, short brecppg, bool rrn) 
: SlotAndRecordPage('B', bh, brecppg) 
{
	//Only do initial formatting if it's a fresh page
	if (brecppg != -1)
		MapInt8(DBP_B_RRNCHAR) = (rrn) ? 'R' : 'E';
}




//**************************************************************************************
//**************************************************************************************
//Parm 2 is passed in just to save repeated sequential scans of the slot area on
//pages where there are lots of deleted slots.
//**************************************************************************************
bool RecordDataPage::PageAdvanceField(short& pagecursor, short next_record_offset) 
{
	if (pagecursor >= next_record_offset)
		throw Exception(DB_STRUCTURE_BUG, "Bug: advancing by field beyond end of record");

	if (PageFieldIsFloat(pagecursor))
		pagecursor += 10;							//fid + 8 bytes data
	else
		pagecursor += (3 + MapUInt8(pagecursor+2));	//fid + 1 byte slen + n bytes sdata 

	if (pagecursor > next_record_offset)
		throw Exception(DB_STRUCTURE_BUG, "Bug: corrupt record data");

	//No more fields - we are now positioned at the point where INSERT will work
	if (pagecursor == next_record_offset)
		return false;

	//We are at a position inside the record - we are positioned at an FV pair
	return true;
}

//**************************************************************************************
void RecordDataPage::PageGetFieldValue(short pagepos, FieldValue& result) 
{
	if (PageFieldIsFloat(pagepos))
		//Values are rounded on the way in, so we can cast directly to rounded here
		result = MapRoundedDouble(pagepos+2);

	else {
		unsigned _int8 slen = MapUInt8(pagepos+2);
		result.AssignData(MapPChar(pagepos+3), slen);

		//V3.0.
		if (PageFieldIsBLOB(pagepos)) {
		
			if (slen != FV_BLOBDESC_LEN)
				throw Exception(DB_STRUCTURE_BUG, "Bug: BLOB descriptor is corrupt");

			result.SetBLOBDescriptorFlag();
		}
	}
}






//**************************************************************************************
//**************************************************************************************
short RecordDataPage::AllocateSlotWithoutReuse
//(int baserec, int breserve, const FieldValue* extending_val) //V3.0
(int baserec, int breserve, bool extending, short extending_len) 
{
	//This function is called in the first attempt to allocate a slot, and only
	//takes slots in sequential order, regardless of deletions.
	if (MapNextFreeSlot() == MapNumSlots())
		return -1;

	//When initially storing records we don't know how much space will be required, 
	//and that is the purpose of BRESERVE.  However, when creating extension records
	//we know the size of the value that is to be put on the extension, so there's
	//no point allocating an extent with less than that size.
	short bytes_needed = 4 + breserve;

	//V3.0.  In a load we can fill large chunks not just single fields
	//if (extending_val) {
	//	if (extending_val->CurrentlyNumeric())
	//		bytes_needed += 10;
	//	else
	//		bytes_needed += (3 + extending_val->StrLen());
	//}
	if (extending)
		bytes_needed += extending_len;

	//Some space is reserved for expansion and can't be used for newly-stored records
	if (NumFreeBytes() < bytes_needed)
		return -1;

	short use_slot = MapNextFreeSlot();

	//We check this before making any changes so that it can be a benign update error
	int newrecnum = baserec;
	newrecnum += use_slot;

	//This check is for M204 compatibility.  In theory INT_MAX records are possible
	//on Baby204 but by default we respect the M204 limit of 16 million.
	if (newrecnum > dbapi->GetParmMAXRECNO())
//	if (newrecnum >= INT_MAX)
		return -1;

	MakeDirty();

	//No need to do any splicing here since this is now the top record on the page
	//(but see the RRN version below).
	short newrecoffset = MapFreeBase();
	MapFreeBase() += 4;

	MapRecordOffset(use_slot, true) = newrecoffset;
	MapExtensionRecordNumber(use_slot) = -1;

	MapFreeSlots()--;
	MapNextFreeSlot()++;

	//The slot number is used as a key in the functions below
	return use_slot;
}

//**************************************************************************************
short RecordDataPage::AllocateSlotWithReuse(int baserec, int breserve) 
{
	//All reusable slots may have been reused already
	if (MapFreeSlots() == 0)
		return -1;

	//The deleted record(s) may have been too small to make useful room
	if (NumFreeBytes() < breserve + 4)
		return -1;

	//There is a slot somewhere - scan along for it
	short slotnum;
	for (slotnum = 0 ; ; slotnum++) {

		if (slotnum == MapNumSlots())
			return -1;

		if (MapRecordOffset(slotnum, true) != -1)
			continue;

		//Check the constraints log before we make the update
		if (!UpdateUnit::FindConstrainedRecNum(baserec + slotnum))
			break;
	}
	
	//Locate the start of the next valid record with number higher than this one
	short insert_point = MapNextRecordOffset(slotnum);

	MakeDirty();

	//May as well splice a -1 straight in as the extension record number
	int ern = -1;
	Splice(slotnum, insert_point, &ern, 4);

	MapRecordOffset(slotnum, true) = insert_point;
	MapFreeSlots()--;

	return slotnum;
}

//**************************************************************************************
void RecordDataPage::UndeleteSlot(short slotnum) 
{
	short& recoffset = MapRecordOffset(slotnum, true);
	if (recoffset != -1)
		throw Exception(DB_STRUCTURE_BUG, "Bug: undeleting valid record slot");

	//We need 4 bytes for the record - it's just possible this will be unavailable
	if (NumFreeBytes() < 4)
		throw Exception(TXN_BACKOUT_ERROR, "Table B page full trying to undelete slot");

	//Locate the start of the next valid record with number higher than this one
	short insert_point = MapNextRecordOffset(slotnum);

	MakeDirty();

	//May as well splice a -1 straight in as the extension record number
	int ern = -1;
	Splice(slotnum, insert_point, &ern, 4);

	recoffset = insert_point;
	MapFreeSlots()--;
}






//**************************************************************************************
//**************************************************************************************
//**************************************************************************************
//Public field functions
//**************************************************************************************
//**************************************************************************************
//**************************************************************************************

//**************************************************************************************
//The purpose of this function is simply to cut down on the number of times the 
//page/record offset translation has to be done in the common case of searching for 
//particular field IDs on a record.
//**************************************************************************************
bool RecordDataPage::SlotAdvanceField
(short slotnum, short& reccursor, PhysicalFieldInfo* pfi) 
{
	//Are we looking for a specific field id?  This will be most common case.
	short findfid = (pfi) ? pfi->id : -1;

	//Map to page-relative once up front (other recs can't move as we have DIRECT now)
	short rec_pos_on_page = MapRecordOffset(slotnum);
	short next_rec_pos_on_page = MapNextRecordOffset(slotnum);

	//First call - start past the extension pointer
	if (reccursor < 4) {

		//* * * NB. See also assumption of pos 4 elsewhere in this file, and also in 
		//RecordDataAccessor::InsertField().  Probably other places.
		reccursor = 4;

		//Record is empty so we're already at the end
		if (rec_pos_on_page + reccursor == next_rec_pos_on_page)
			return false;

		//Any field
		if (!pfi)
			return true;

		//First field is the desired field
		if (PageGetFieldCode(rec_pos_on_page + 4) == findfid)
			return true;
	}

	//Advance at least one field
	short pagecursor = reccursor + rec_pos_on_page;
	bool noteor;
	do {
		noteor = PageAdvanceField(pagecursor, next_rec_pos_on_page);
	} 
	while (findfid != -1 && noteor && PageGetFieldCode(pagecursor) != findfid);

	//Map back to record-relative offset so we can use it in later calls (e.g. FEO loops).
	//Note that the cursor will point at the start of the next record if a field was
	//not found.  Thus we will be positioned ready for ADD field.
	reccursor = pagecursor - rec_pos_on_page;
	return noteor;
}

//**************************************************************************************
//Similar, but used when inserting data and fields have to be moved to extensions
//**************************************************************************************
bool RecordDataPage::SlotLocateLastAnyField(short slotnum, short& reccursor) 
{
	short rec_pos_on_page = MapRecordOffset(slotnum);
	short next_rec_pos_on_page = MapNextRecordOffset(slotnum);

	//Got to scan the whole extent for this
	short pagecursor = rec_pos_on_page + 4;

	//No fields on the record
	if (pagecursor == next_rec_pos_on_page)
		return false;

	short lastfieldpos = 4;
	while (PageAdvanceField(pagecursor, next_rec_pos_on_page))
		lastfieldpos = pagecursor;

	reccursor = lastfieldpos - rec_pos_on_page;
	return true;
}

//**************************************************************************************
//The following functions are called after the above have been used for positioning
//**************************************************************************************
int RecordDataPage::SlotInsertFVPair
(short slotnum, short recoffset, FieldID fid, const FieldValue& fval) 
{
	bool numeric_value = fval.CurrentlyNumeric();
	short fv_block_len = 2;

	if (numeric_value)
		fv_block_len += 8;
	else
		fv_block_len += 1 + fval.StrLen();

	//Not enough room
	if (fv_block_len > NumFreeBytes())
		return 0;

	short pageoffset = MapRecordOffset(slotnum) + recoffset;

	MakeDirty();

	//Splice in some blank data - we'll write over it using standard mappers next
	Splice(slotnum, pageoffset, NULL, fv_block_len);

	MapInt16(pageoffset) = fid;

	if (numeric_value) {
		MakeNumericPageCode(MapInt16(pageoffset)); //see comments elsewhere
		MapRoundedDouble(pageoffset+2) = *(fval.RDData());
	}
	else {
		//V3.0.  Like numerics above, BLOBs have a bit set on table B for ease of read access
		if (fval.IsBLOBDescriptor())
			MakeBLOBPageCode(MapInt16(pageoffset));

		int slen = fval.StrLen();
		MapUInt8(pageoffset+2) = slen;
		memcpy(MapPChar(pageoffset+3), fval.StrChars(), slen);
	}

	return fv_block_len;
}

//**************************************************************************************
bool RecordDataPage::SlotDeleteFVPair(short slotnum, short offset, FieldID* nextfid) 
{
	short rec_pos_on_page = MapRecordOffset(slotnum);
	short delpos = rec_pos_on_page + offset;

	short fvlen = 2;
	if (PageFieldIsFloat(delpos))
		fvlen += 8;
	else
		fvlen += (1 + MapUInt8(delpos+2));

	MakeDirty();

	Splice(slotnum, delpos, NULL, -fvlen);

	//Is the extent now empty?
	short next_rec_pos_on_page = MapNextRecordOffset(slotnum);
	if (next_rec_pos_on_page == rec_pos_on_page + 4)
		return true;

	//V2.19 June 2009.  For neater processing of the DELETE FIELD command
	if (nextfid && next_rec_pos_on_page > delpos)
		*nextfid = PageGetFieldCode(delpos);

	return false;
}

//**************************************************************************************
void RecordDataPage::SlotGetFVPair
(short slotnum, short offset, FieldID* pfid, FieldValue* pval) 
{
	short rec_pos_on_page = MapRecordOffset(slotnum);
	short pagepos = rec_pos_on_page + offset;

	if (pval)
		PageGetFieldValue(pagepos, *pval);

	if (pfid)
		*pfid = PageGetFieldCode(pagepos);
}

//**************************************************************************************
int RecordDataPage::SlotGetExtensionPtr(short slotnum) 
{
	short rec_pos_on_page = MapRecordOffset(slotnum);
	return MapInt32(rec_pos_on_page);
}

//**************************************************************************************
void RecordDataPage::SlotSetExtensionPtr(short slotnum, int ern) 
{
	short rec_pos_on_page = MapRecordOffset(slotnum);

	MakeDirty();

	MapInt32(rec_pos_on_page) = ern;
}

//**************************************************************************************
//V3.0.  Whole records are written at once in a fast load.
char* RecordDataPage::SlotPrepareSpaceForLoadData(short slotnum, short extent_len) 
{
	short pageoffset = MapRecordOffset(slotnum) + 4;

	MakeDirty();

	//Move other data up.  There would only be any more data if we were loading into
	//a RRN file, otherwise it will always be the top record.
	Splice(slotnum, pageoffset, NULL, extent_len);

	return &(MapInt8(pageoffset));
}

} //close namespace
