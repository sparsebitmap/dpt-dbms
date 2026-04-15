
#include "stdafx.h"

#include "page_t.h" //#include "page_T.h" : V2.24 case is less interchangeable on *NIX - Roger M.

//Utils
#include "parsing.h"
#include "dataconv.h"
//API Tiers
//Diagnostics
#include "assert.h"
#include "except.h"
#include "msg_db.h"

namespace dpt {

short BTreePage::DBP_T_DEBUG_FILLER	= 0;

//***********************************************************************************
inline void BTreePage::CacheFlags()
{ 
	switch (PageSubType_S()) {
	case 'L':
		is_root		= false;
		is_branch	= false;
		is_leaf		= true;

		bl_data_area_pos = DBP_T_AREA_1_DATA;
		break;

	case 'S':
		is_root		= true;
		is_branch	= false;
		is_leaf		= true;

		bl_data_area_pos = DBP_T_AREA_2_DATA;
		break;

	case 'R':
		is_root		= true;
		is_branch	= true;
		is_leaf		= false;

		bl_data_area_pos = DBP_T_AREA_2_DATA;
		break;

	default: //N
		is_root		= false;
		is_branch	= true;
		is_leaf		= false;

		bl_data_area_pos = DBP_T_AREA_1_DATA;
	}

	tree_type_numeric = (MapNumericFlag() == 'N');
}

//***********************************************************************************
BTreePage::BTreePage(BufferPageHandle& bh) 
: DatabaseFilePage('T', bh) 
{
	CacheFlags();
}

//***********************************************************************************
BTreePage::BTreePage(BufferPageHandle& bh, bool freshvalnum, char freshnodetype)
: DatabaseFilePage('T', bh, true)
{
	if (freshvalnum)
		MapNumericFlag() = 'N'; //ORD NUM
	else
		MapNumericFlag() = 'A'; //ORD CHAR

	PageSubType_S() = freshnodetype;

	CacheFlags();
	WriteEyeCatchers();

	MapPCKLen() = 0;
	BL_MapAreaFreeBase() = 0;

	MapRightSibling() = -1;
	MapLeftSibling() = -1;

	//If we started a version number at 1 there would be a risk of an existing leaf 
	//with a low version number being reused again as a leaf and versioning up to the
	//same number so as to confuse the caller.  By starting at the UTC time we can 
	//virtually eliminate that possibility.
	MapTStamp() = time(NULL);

	//V2.12. April 2008.  Buffer stats.
	bh.BuffAPINoteFreshFormattedPage('T');
}

//***********************************************************************************
void BTreePage::WriteEyeCatchers() 
{
	if (is_root) {
		memcpy(MapPChar(DBP_T_AREA_1_EYE), "HDR:", 4);
		memcpy(MapPChar(DBP_T_AREA_2_EYE), "DAT:", 4);
	}
	else {
		memcpy(MapPChar(DBP_T_AREA_1_EYE), "DAT:", 4);
	}
}






//***********************************************************************************
//***********************************************************************************
//Header area and root functions
//***********************************************************************************
//***********************************************************************************
void BTreePage::ConvertRootToPureNode(BTreePage& ptnewroot)
{
	//Copy all the header information from the old root to the new
//	ptnewroot.R_LoadHeaderInfo(this);

	//Convert into a branch or leaf page
	MakeDirty();

	short old_hdr_area_size = H_AreaSize();
	if (old_hdr_area_size > 0) {

		//Move down all the main data to overwrite the old header area.
		
		char* curbase = BL_MapPChar(0);			//current area base
		bl_data_area_pos = DBP_T_AREA_1_DATA;	//remap
		char* newbase = BL_MapPChar(0);			//remapped area base

		short datasize = BL_MapAreaFreeBase();
		memmove(newbase, curbase, datasize);

		//For readability in dumps clear the part revealed at the new free space pointer
		memset(BL_MapPChar(BL_MapAreaFreeBase()), 0, old_hdr_area_size);
	}

	//So that next time we map the page we'll know there's no header area
	if (is_branch)
		PageSubType_S() = 'N';
	else
		PageSubType_S() = 'L';

	is_root = false;
	WriteEyeCatchers();
}

//***********************************************************************************
void BTreePage::H_GetLowestValue(FieldValue& lv)
{
	if (tree_type_numeric)
		lv = MapRoundedDouble(DBP_T_TREE_LOVALUE);
	else
		lv = MapPChar(DBP_T_TREE_LOVALUE);
}

//***********************************************************************************
void BTreePage::H_SetLowestValue(const FieldValue& lv)
{
	//To include this optional processing we need to put calls in the following places:
		//Normal insert value (can triage by testing for leftsib == -1 and pos == 0)
		//Normal remove value (likewise)
		//Remove final value from leftmost node
	//In both the remove cases the need to chain to the right sibling 
	//for the new lovalue is relevant. 

	if (tree_type_numeric)
		MapRoundedDouble(DBP_T_TREE_LOVALUE) = lv.ExtractRoundedDouble().Data();
	else {

		//This is only used for LT tests in btree finds, so truncation is fine
		int numchars = lv.StrLen();
		if (numchars > 7)
			numchars = 7;
		char* dest = MapPChar(DBP_T_TREE_LOVALUE);

		memcpy(dest, lv.StrChars(), numchars);
		dest[numchars] = 0;
	}
}







//***********************************************************************************
//***********************************************************************************
//Branch or leaf area functions
//***********************************************************************************
//***********************************************************************************
bool BTreePage::BL_LocateNextValue
(short current_value_offset, short& next_value_offset, short& next_data_offset)
{
	//Lowest value requested
	if (!BL_PositionIsWithinData(current_value_offset))
		next_value_offset = 0;

	//Otherwise advance to the next value
	else {
		if (tree_type_numeric)
			next_value_offset = current_value_offset + 
				8 + BL_DataItemSize();
		else
			next_value_offset = current_value_offset + 
				1 + BL_MapStrLen(current_value_offset) + BL_DataItemSize();
	}

	//Positioned on a value now?
	if (!BL_PositionIsWithinData(next_value_offset))
		return false;

	if (tree_type_numeric)
		next_data_offset = next_value_offset + 8;
	else
		next_data_offset = next_value_offset + 1 + BL_MapStrLen(next_value_offset);

	return true;
}

//******************************************************************************
bool BTreePage::BL_LocatePreviousValue
(short current_value_offset, short& prev_value_offset, short& prev_data_offset)
{
	//Highest value requested
	if (!BL_PositionIsWithinData(current_value_offset))
		current_value_offset = BL_MapAreaFreeBase();

	//Empty page
	if (current_value_offset == 0)
		return false;

	//Move back one value
	if (tree_type_numeric) {
		prev_value_offset = current_value_offset - (8 + BL_DataItemSize());
		prev_data_offset = prev_value_offset + 8;
	}

	//With string values there is no way to work out what the previous value is 
	//without scanning the whole page again (since values aren't zero-terminated).
	else {
		short v = 0;
		short data_size = BL_DataItemSize();
		short vprev;
		while (v < current_value_offset) {
			vprev = v;
			v += BL_MapStrLen(v);
			v++;
			v += data_size;
		}
		prev_value_offset = vprev;
		prev_data_offset = prev_value_offset + 1 + BL_MapStrLen(prev_value_offset);
	}

	//Must be on a value if we get to here
	return true;
}

//******************************************************************************
void BTreePage::BL_GetValue(short offset, FieldValue& value)
{
	if (!BL_PositionIsWithinData(offset))
		throw Exception(DB_STRUCTURE_BUG, 
			"Bug: reading btree leaf/branch value in no man's land");

	short pagepos = BL_PagePos(offset);

	if (tree_type_numeric)
		value = MapRoundedDouble(pagepos);
	else
		value.AssignData(PCKChars(), MapPCKLen(), MapPChar(pagepos+1), MapUInt8(pagepos));
}

//******************************************************************************
void BTreePage::BL_ExtractAllInfo
(std::vector<BL_EntryInfo>* info1, std::vector<BL_EntryInfo>* info2, int* split)
{
	//We may be collecting all values into one vector (e.g. changing compression key)
	if (!split)
		info1->resize(MapNumVals());
	
	//or into 2 parts (during a page split)
	else {
		info1->resize(*split);
		info2->resize(MapNumVals() - *split);
	}

	short valoffset = -1;
	short dataoffset;
	int i = 0;

	//Start off loading the first container
	std::vector<BL_EntryInfo>* info = info1;

	while (BL_LocateNextValue(valoffset, valoffset, dataoffset)) {

		//Then switch to the second at the appropriate point
		if (split) if (i == *split) if (info == info1) {
			info = info2;
			i = 0;
		}

		BL_EntryInfo& entry = (*info)[i];
		BL_GetValue(valoffset, entry.value);
		BL_GetData(dataoffset, entry.idata, &entry.sdata);

		i++;
	}
}


//******************************************************************************
//******************************************************************************
bool BTreePage::BL_InsertValue
(const FieldValue& newval, short& value_offset, short& data_offset)
{
	//Insert first value
	if (value_offset == -1)
		value_offset = 0;

	short entry_physsize;
	int required_bytes;
	short cklen;
	short cklenchange;

	//Things are much simpler for numeric values
	if (tree_type_numeric) {
		entry_physsize = 8 + BL_DataItemSize();
		required_bytes = entry_physsize;
		cklen = 0;
		cklenchange = 0;
	}

	//...than for strings
	else {

		//New string fits existing prefix?  (Otherwise the prefix must be shortened).
		short newvallen = newval.StrLen();

		//Note that it is impossible to cause a longer prefix during insert, *except*
		//with the first value on the page, when it will be the entire string.
		if (MapNumVals() == 0)
			cklen = newvallen;
		else
			cklen = util::CountSharedChars
				(newval.StrChars(), newvallen, PCKChars(), MapPCKLen());

		entry_physsize = 1 + newvallen + BL_DataItemSize() - cklen;

		//Shorter compression key means all other values will have to be lengthened.
		cklenchange = cklen - MapPCKLen();

		required_bytes = entry_physsize;
		required_bytes -= (MapNumVals() * cklenchange);	//to alter other entries
		required_bytes += cklenchange;					//to alter stored ck string
	}

	//Not enough room - caller will have to split the node first
	if (required_bytes > BL_FreeBytes())
		return false;

	MakeDirty();

	//Rejig other values if prefix compression is changing.
	//To avoid many big memmoves the quickest way of doing this is to read all the
	//values off and recompress/reload them in one pass, as when doing a page split.
	if (cklenchange != 0) {
		std::vector<BL_EntryInfo> info;
		BL_ExtractAllInfo(&info);

		//May as well load the inserted value at the same time as we go (the originally
		//located offset will become invalid anyway).
		BL_LoadAllInfo(&info, cklen, &newval, &value_offset, &data_offset);
		return true;
	}

	//No CK change, so make a gap for the new value at the specified offset
	short bytes_to_move = BL_MapAreaFreeBase() - value_offset;
	if (bytes_to_move > 0)
		memmove(BL_MapPChar(value_offset + entry_physsize), 
				BL_MapPChar(value_offset), 
				bytes_to_move);

	BL_MapAreaFreeBase() += entry_physsize;

	short cursor = value_offset;
	BL_WriteValue(cursor, newval);
	data_offset = cursor;
	BL_WriteData(cursor, -1);

	return true;
}

//******************************************************************************
void BTreePage::BL_WriteValue(short& offset, const FieldValue& value)
{
	if (tree_type_numeric) {
		MapRoundedDouble(BL_PagePos(offset)) = value.ExtractRoundedDouble();
		offset += 8;
	}
	else {
		short cklen = MapPCKLen();
		unsigned _int8 physvallen = value.StrLen() - cklen;

		BL_MapStrLen(offset) = physvallen;
		offset++;

		if (physvallen > 0) {
			memcpy(BL_MapPChar(offset), value.StrChars() + cklen, physvallen);
			offset += physvallen;
		}
	}

	MapNumVals()++;
}

//******************************************************************************
bool BTreePage::BL_RemoveValue(short offset)
{
	MakeDirty();

	//No attempt to improve the prefix compression key - see tech docs for discussion.
	short entry_physsize;
	if (tree_type_numeric)
		entry_physsize = 8 + BL_DataItemSize();
	else
		entry_physsize = 1 + BL_MapStrLen(offset) + BL_DataItemSize();

	short bytes_to_move = BL_MapAreaFreeBase() - (offset + entry_physsize);
	if (bytes_to_move > 0)
		memmove(BL_MapPChar(offset), 
				BL_MapPChar(offset + entry_physsize), 
				bytes_to_move);

	BL_MapAreaFreeBase() -= entry_physsize;
	MapNumVals()--;

	//For readability in dumps
	memset(BL_MapPChar(BL_MapAreaFreeBase()), 0, entry_physsize);

	//Any values now left?
	return (MapNumVals() == 0);
}

//******************************************************************************
void BTreePage::BL_LoadAllInfo
(const std::vector<BL_EntryInfo>* info, short cklen, 
 const FieldValue* insval, short* insvaloffset, short* insdataoffset)
{
	MapPCKLen() = cklen;
	if (cklen > 0) {
		const FieldValue& v = (insval) ? *insval : (*info)[0].value;
		memcpy(PCKChars(), v.StrChars(), cklen);
	}

	MapNumVals() = 0;
	short cursor = 0;
	const FieldValue* value_to_be_inserted = insval;

	//Load the entries onto the page in one pass
	for (size_t x = 0; x < info->size(); x++) {
		const BL_EntryInfo& entry = (*info)[x];
		const FieldValue& thisval = entry.value;

		//New value to be inserted preceding an entry
		if (value_to_be_inserted) {
			if (value_to_be_inserted->Compare(thisval) < 0) {

				//The actual insertion offset will be required by the btree management code
				*insvaloffset = cursor;
				BL_WriteValue(cursor, *value_to_be_inserted);
				*insdataoffset = cursor;
				BL_WriteData(cursor, -1);

				value_to_be_inserted = NULL;
			}
		}

		BL_WriteValue(cursor, thisval);
		BL_WriteData(cursor, entry.idata, entry.sdata);
	}

	//New entry inserted yet?  It not put it at the end.
	if (value_to_be_inserted) {
		*insvaloffset = cursor;
		BL_WriteValue(cursor, *value_to_be_inserted);
		*insdataoffset = cursor;
		BL_WriteData(cursor, -1);
	}

	BL_MapAreaFreeBase() = cursor;
}

//******************************************************************************
void BTreePage::BL_DivideValues(BTreePage* newrightnode, 
	unsigned _int8 splitpct, FieldValue& last_value_on_left) 
{
	int splitleft = MapNumVals() * splitpct;
	splitleft /= 100;

	//Both nodes *must* have at least one value - see tech docs for discussion.
	if (splitleft == 0)
		splitleft++;
	if (splitleft == MapNumVals())
		splitleft--;

	//Collect the current info into 2 vectors
	std::vector<BL_EntryInfo> infoleft;
	std::vector<BL_EntryInfo> inforight;
	BL_ExtractAllInfo(&infoleft, &inforight, &splitleft);

	//Prefix compression keys
	int cklenleft = (tree_type_numeric) ? 0 : BL_CalculatePCK(&infoleft);
	int cklenright = (tree_type_numeric) ? 0 : BL_CalculatePCK(&inforight);

	//To help with suffix compression after the split
	last_value_on_left = infoleft[infoleft.size() - 1].value;

	//Partially reload left page (zeroize first for visual readability)
	MakeDirty();
	memset(BL_MapPChar(0), 0, BL_MapAreaFreeBase());
	BL_LoadAllInfo(&infoleft, cklenleft);

	//Rest of the info goes on the fresh right sibling
	newrightnode->BL_LoadAllInfo(&inforight, cklenright);
}

//******************************************************************************
int BTreePage::BL_CalculatePCK(const std::vector<BL_EntryInfo>* info)
{
	//Firstly assume the entire first string
	short len = (*info)[0].value.StrLen();
	const char* chars = (*info)[0].value.StrChars();
	
	//Then compare subsequent strings with the best key so far (quit when/if zero length)
	for (size_t x = 1; len > 0 && x < info->size(); x++) {
		const FieldValue& val = (*info)[x].value; 
		len = util::CountSharedChars(val.StrChars(), val.StrLen(), chars, len);
	}

	return len;
}








//***********************************************************************************
//***********************************************************************************
//Branch area functions
//***********************************************************************************
//***********************************************************************************
bool BTreePage::B_LocateValueLE
(const FieldValue& desired_value, short& value_offset, short& pagenum_offset)
{
	assert (is_branch);

	value_offset = -1;

	FieldValue curval;
	short curr_value_offset = -1;
	short best_value_offset = -1;
	short curr_pagenum_offset;
	short best_pagenum_offset;

	while (BL_LocateNextValue(curr_value_offset, curr_value_offset, curr_pagenum_offset)) {

		BL_GetValue(curr_value_offset, curval);
		int cmp = curval.Compare(desired_value);

		//Gone past the place where the desired value would have been
		if (cmp > 0)
			break;

		//Exactly found
		if (cmp == 0) {
			value_offset = curr_value_offset;
			pagenum_offset = curr_pagenum_offset;
			return true;
		}

		//Yet to reach value - still might be the right branch though - keep looking
		best_value_offset = curr_value_offset;
		best_pagenum_offset = curr_pagenum_offset;
	}

	//Gone too far - take previous branch as noted above.  NB. If we drop out of the 
	//loop above we've gone past the last value - this is fine - take the final branch.
	if (best_value_offset != -1) {
		value_offset = best_value_offset;
		pagenum_offset = best_pagenum_offset;
		return true;
	}

	//Desired value before first value on page (or page empty).
	//During a search it is OK to take the first branch even if it will lead us
	//to successive nodes where the value will definitely not be found.  This
	//simplifies the case where insertion will then follow, since we will always
	//be positioned at the very left of the tree ready to insert.
	value_offset = curr_value_offset;
	pagenum_offset = curr_pagenum_offset;
	return false;
}







//***********************************************************************************
//***********************************************************************************
//Leaf area functions
//***********************************************************************************
//***********************************************************************************
bool BTreePage::L_LocateValue
(const FieldValue& desired_value, short& value_offset, short& ilmr_offset)
{
	assert (is_leaf);

	FieldValue curval;
	value_offset = -1;

	while (BL_LocateNextValue(value_offset, value_offset, ilmr_offset)) {

		BL_GetValue(value_offset, curval);
		int cmp = curval.Compare(desired_value);

		//Gone past the place where the desired value would have been.  Importantly we 
		//are now positioned at the next.
		if (cmp > 0)
			return false;

		//Reached it
		if (cmp == 0)
			return true;
	}

	//All values on the page were less than the desired value.  We are now positioned
	//at the first free byte on the page.
	return false;
}








//***********************************************************************************
//***********************************************************************************
//Diagnostics
//***********************************************************************************
//***********************************************************************************
std::string BTreePage::BL_FormatDumpDisplayString(short value_offset, int oplen)
{
	FieldValue v;
	BL_GetValue(value_offset, v);

	std::string s;
	bool pseudo = false;

	if (v.CurrentlyNumeric()) {
		double d = v.ExtractRoundedDouble().Data();
		if (d == RangeCheckedDouble::MAXIMUM_NEGATIVE_VALUE) {
			s = "<Lo-val>";
			pseudo = true;
		}
		else if (d == RangeCheckedDouble::MAXIMUM_POSITIVE_VALUE) {
			s = "<Hi-val>";
			pseudo = true;
		}
	}
	else if (v.StrLen() == 0) {
		s = "<Null-string>";
		pseudo = true;
	}

	if (!pseudo) {
		int pcklen = MapPCKLen(); 
		if (pcklen == 0)
			s = v.ExtractString();

		else {
			int slen = v.StrLen();

			s.reserve(slen + 2);

			//Put brackets to indicate what is the prefix
			s.append(1, '(');
			s.append(v.StrChars(), pcklen);
			s.append(1, ')');
			s.append(v.StrChars() + pcklen, slen - pcklen);
		}
	}

	return util::PadOrElide(s, oplen);
}

} //close namespace
