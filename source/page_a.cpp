
#include "stdafx.h"


#include "page_a.h" //#include "page_A.h" : V2.24 case is less interchangeable on *NIX - Roger M.
#include "fieldinfo.h"
#include "except.h"
#include "msg_db.h"

namespace dpt {

//A certain amount is reserved for renames to longer names and redefines to ordered.
//This is not entirely robust, but OK for a first cut.
const short FieldAttributePage::RESERVED_PAGE_SPACE = 500;

//**************************************************************************************
FieldAttributePage::FieldAttributePage(BufferPageHandle& bh, int start_field_id) 
: DatabaseFilePage('A', bh, (start_field_id != -1)) 
{
	if (start_field_id == -1)
		return;

	MapNextFieldID() = start_field_id;
	MapChainPtr() = -1;
	MapFreeBase() = DBP_A_ATT_AREA;
}

//**************************************************************************************
int FieldAttributePage::AppendFieldAtts
(const std::string& fname, const FieldAttributes& fatts)
{
	//Is there room?
	short required_space = 1 + fname.length() + 1;
	if (fatts.IsOrdered())
		required_space += 5;

	if (required_space > NumFreeBytes() - RESERVED_PAGE_SPACE)
		return -1;

	MakeDirty();

	//Start storing the info at the start of free space
	short cursor = MapFreeBase();

	//Use the next field ID
	int fid = MapNextFieldID();
	MapNextFieldID()++;
	MapInt16(cursor) = fid;
	cursor += 2;

	//Using a length byte here as with M204 instead of a C string like I used to.  This
	//is because sometimes you want to just scan for a particular field ID and the length
	//byte makes this a tad more efficient than loop/testing for zero.
//	strcpy(MapPChar(cursor), fname.c_str());
//	cursor += fname.length() + 1;
	unsigned _int8 namelen = fname.length();
	MapUInt8(cursor) = namelen;
	cursor++;
	memcpy(MapPChar(cursor), fname.c_str(), namelen);
	cursor += namelen;

	//The first byte of attributes is always stored
	MapUInt8(cursor) = fatts.Flags();
	cursor++;

	//Only store the index info if appropriate
	if (fatts.IsOrdered()) {
		MapUInt8(cursor) = fatts.Splitpct();
		cursor++;

		//Initially no btree but reserve the root ptr space now
		MapInt32(cursor) = -1;
		cursor += 4;
	}

	short newattpos = MapFreeBase();
	MapFreeBase() = cursor;
	return newattpos;
}

//**************************************************************************************
PhysicalFieldInfo* FieldAttributePage::GetFieldInfo(short& cursor)
{
	//Start at the first field on the page or wherever the caller left off last time
	if (cursor <= 0)
		cursor = DBP_A_ATT_AREA;

	//No (more) fields on the page
	if (cursor == MapFreeBase())
		return NULL;

	//Field ID
	FieldID fid = MapInt16(cursor);
	cursor += 2;

	//Field name (see comments above re. length byte now)
//	std::string fname = MapPChar(cursor);
	unsigned _int8 namelen = MapUInt8(cursor);
	cursor++;
	std::string fname(MapPChar(cursor), namelen);
	cursor += namelen;

	//Atts - there is always a flags byte
	FieldAttributes atts;
	atts.flags = MapUInt8(cursor);
	cursor++;

	//Maybe extra info for indexed fields
	int root = -1;
	if (atts.IsOrdered()) {
		atts.SetSplitPct(MapUInt8(cursor));
		cursor++;

		root = MapInt32(cursor);
		cursor += 4;
	}

	atts.ValidityCheck(true);

	return new PhysicalFieldInfo(fname, atts, fid, root);
}

//**************************************************************************************
short FieldAttributePage::LocateField(const std::string& infn)
{
	int fidoffset = DBP_A_ATT_AREA;

	while (fidoffset < MapFreeBase()) {

		int offset = fidoffset;
		
		//Field ID
		offset += 2;

		//Field name (see comments above re. length byte now)
		//std::string fname = MapPChar(offset);
		unsigned _int8 namelen = MapUInt8(offset);
		offset++;
		std::string fname(MapPChar(offset), namelen);
		offset += namelen;

		if (fname == infn)
			return fidoffset;

		//Atts - there is always a flags byte
		FieldAttributes atts;
		atts.flags = MapUInt8(offset);
		offset++;

		//Maybe extra info for indexed fields
		if (atts.IsOrdered())
			offset += 5;

		//Now we're at the next field
		fidoffset = offset;
	}

	//Field not present on the page
	return -1;
}

//**************************************************************************************
void FieldAttributePage::VerifyFieldMissing(const std::string& newfname)
{
	if (LocateField(newfname) != -1)
		throw Exception(DBA_FIELD_ALREADY_EXISTS, 
			std::string("Field already exists: ").append(newfname));
}

//**************************************************************************************
bool FieldAttributePage::UpdateBTreeRootPage(short& offset, PhysicalFieldInfo* pfi)
{
	//This func will be called until the required field is found, so start at the first
	if (offset <= 0)
		offset = DBP_A_ATT_AREA;

	//No (more) fields on the page
	if (offset == MapFreeBase()) {
		offset = -1;
		return false;
	}

	//Field ID
	FieldID fid = MapInt16(offset);
	offset += 2;

	//Field name - no need to know it here, just skip past
	unsigned _int8 namelen = MapUInt8(offset);
	offset++;
	offset += namelen;

	//Atts
	FieldAttributes atts;
	atts.flags = MapUInt8(offset);
	offset++;

	//Indexed field info - if missing we know it's not our field
	if (!atts.IsOrdered())
		return false;

	//Just skip past splitpct and root page for now
	offset++;
	offset += 4;

	//See soft initialize below
	if (!pfi) {
		MapInt32(offset-4) = -1;
		return true;
	}

	//Normal use
	if (fid != pfi->id)
		//Desired field not reached yet
		return false;
	else {
		//Found it
		MakeDirty();
		MapInt32(offset-4) = pfi->btree_root;
		return true;
	}
}

//**************************************************************************************
void FieldAttributePage::InitializeDynamicFieldInfo()
{
	MakeDirty();

	//Currently the only item of info altered after initial DEFINE is the btree root page
	short offset = 0;
	for (;;) {
		UpdateBTreeRootPage(offset, NULL);
		if (offset == -1)
			break;
	}
}

//**************************************************************************************
void FieldAttributePage::SetSplitpct(short fidoffset, unsigned char splitpct)
{
	MakeDirty();

	short offset = fidoffset;

	//Skip fid and name
	offset += 2;
	unsigned _int8 namelen = MapUInt8(offset);
	offset++;
	offset += namelen;
	
	offset++; //atts

	MapUInt8(offset) = splitpct;
}

//**************************************************************************************
void FieldAttributePage::SetAttributeByte(short fidoffset, unsigned char attbyte)
{
	MakeDirty();

	short offset = fidoffset;

	//Skip fid and name
	offset += 2;
	unsigned _int8 namelen = MapUInt8(offset);
	offset++;
	offset += namelen;
	
	MapUInt8(offset) = attbyte;
}

//**************************************************************************************
bool FieldAttributePage::ChangeFieldName(short fidoffset, const std::string& newfname)
{
	short offset = fidoffset;

	offset += 2;
	unsigned _int8 namelen = MapUInt8(offset);

	//Allow the full page area to be used - cf. define above.  This could be tightened
	//up but I'm just not in the mood.  We could shuffle all the fields along the fatt
	//page chain, but that would be more complicated than it sounds.
	short namelen_change = newfname.length() - namelen;
	if (namelen_change > NumFreeBytes())
		return false;
	
	short nameoffset = offset;

	//Go to the end of the field name
	offset++;
	offset += namelen;

	MakeDirty();

	if (namelen_change != 0) {

		//Shuffle up or down any data above this field on the page.  Actually it will
		//always be at least 1 byte - i.e. the atts of the field being renamed.
		short bytes_to_move = MapFreeBase() - offset;
		//if (bytes_to_move > 0)
			memmove(MapPChar(offset + namelen_change), MapPChar(offset), bytes_to_move);

		MapFreeBase() += namelen_change;

		//Blank any revealed area as usual
		if (namelen_change < 0)
			memset(MapPChar(MapFreeBase()), 0, -namelen_change);
	}

	MapUInt8(nameoffset) = newfname.length();
	memcpy(MapPChar(nameoffset+1), newfname.c_str(), newfname.length());
	return true;
}

//**************************************************************************************
//V2.19 June 09.
void FieldAttributePage::DeleteFieldEntry(short base)
{
	short cursor = base;

	//How long is the entry?
	cursor += 2;                      //FID
	cursor += MapUInt8(cursor);       //name
	cursor += 1;                      //name length byte

	FieldAttributes atts;
	atts.flags = MapUInt8(cursor);
	cursor += 1;                      //att byte

	if (atts.IsOrdered())
		cursor += 5;                  //index info

	int len = cursor - base;

	MakeDirty();

	//Shuffle down any data above the deleted field.
	short bytes_to_move = MapFreeBase() - cursor;
	if (bytes_to_move > 0)
		memmove(MapPChar(base), MapPChar(cursor), bytes_to_move);

	MapFreeBase() -= len;

	//Blank the revealed area
	memset(MapPChar(MapFreeBase()), 0, len);
}

//**************************************************************************************
//V2.19 June 09.  Redefine from ordered to non-ordered
void FieldAttributePage::RemoveIndexInfoBlock(short base)
{
	short cursor = base;

	cursor += 2;                      //FID
	cursor += MapUInt8(cursor);       //name
	cursor += 1;                      //name length byte
	cursor += 1;                      //att byte

	MakeDirty();

	short bytes_to_move = MapFreeBase() - cursor - 5;
	memmove(MapPChar(cursor), MapPChar(cursor + 5), bytes_to_move);

	MapFreeBase() -= 5;

	memset(MapPChar(MapFreeBase()), 0, 5);
}

//**************************************************************************************
//V2.19 June 2009.  Redefine from non-ordered to ordered
void FieldAttributePage::InsertIndexInfoBlock(short base, PhysicalFieldInfo* pfi)
{
	short cursor = base;

	cursor += 2;                      //FID
	cursor += MapUInt8(cursor);       //name
	cursor += 1;                      //name length byte
	cursor += 1;                      //att byte

	//We left space for expansion in Append... above, to keep things simple here
	if (NumFreeBytes() - RESERVED_PAGE_SPACE < 5)
		throw Exception(DB_INSUFFICIENT_SPACE, 
			"Field attribute page is full - operation is not possible");

	MakeDirty();

	short bytes_to_move = MapFreeBase() - cursor;
	memmove(MapPChar(cursor + 5), MapPChar(cursor), bytes_to_move);

	MapFreeBase() += 5;

	MapUInt8(cursor) = 50;
	MapInt32(cursor+1) = pfi->btree_root;
}

} //close namespace
