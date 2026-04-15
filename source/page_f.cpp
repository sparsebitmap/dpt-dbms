
#include "stdafx.h"

#include "page_f.h" //#include "page_F.h" : V2.24 case is less interchangeable on *NIX - Roger M.

//Utils
//API Tiers
#include "page_e.h" //#include "page_E.h" : V2.24 case is less interchangeable on *NIX - Roger M.
#include "dbserv.h"
#include "core.h"
//Diagnostics
#include "msg_db.h"
#include "except.h"

namespace dpt {

//************************************************************************************
//Create time
//************************************************************************************
FCTPage::FCTPage(
RawPageData* rp, 
int bsize, int brecppg, int breserve, int breuse, 
int dsize, int dreserve, int dpgsres, 
int fileorg, const std::string& path) 
: DatabaseFilePage('F', rp, true) 
{
	//Eyecatchers
	WriteChars(DBP_F_FIL_EYE, "FIL:", 4);
	WriteChars(DBP_F_FLD_EYE, "FLD:", 4);
	WriteChars(DBP_F_DAT_EYE, "DAT:", 4);
	WriteChars(DBP_F_IDX_EYE, "IDX:", 4);
	WriteChars(DBP_F_DHP_EYE, "DHP:", 4);
	WriteChars(DBP_F_LPM_EYE, "LPM:", 4);

	//Ficreate:
	//A = V0.1 - initial dev version
	//B = V0.2 - new parms added for indexing (ILACTIVE, IRQ_HEAD and LRQ_HEAD)
	MapFicreate()	= 'B';
	MapFistat()		= 1;

	//Supplied parms
	MapBsize() = bsize;
	MapBrecppg() = brecppg;
	MapBreserve() = breserve;
	MapBreuse() = breuse;
	MapDsize() = dsize;
	MapDreserve() = dreserve;
	MapDpgsres() = dpgsres;
	MapFileorg() = fileorg;

	strcpy(MapLastOSFileName(), FormatOSFileName(path).c_str());

	InitializeValues(0);
}

//************************************************************************************
//This stuff gets cleared on Initialize
//************************************************************************************
void FCTPage::InitializeValues(int atrpg)
{
	//File-wide values
	MapFiflags() = 0;
	*(MapRBCPTime()) = 0;
	MapSeqopt() = 0;
	MapDULen() = 0;
	MapDUFormat() = 0;

	//Data area
	MapBhighpg() = -1;
	MapBqlen() = 0;
	MapBreused() = 0;
	MapMstradd() = 0;
	MapMstrdel() = 0;
	MapExtnadd() = 0;
	MapExtndel() = 0;

	//V3.0 - BLOBS
	MapEactive() = -1;
	MapBLOBQHead() = -1;

	//Indexes
	MapDactive() = -1;
	MapIlactive() = -1;
	MapILMRQHead() = -1;
	MapListQHead() = -1;

	//Heap 
	MapDpgsused() = atrpg;
	MapDhighpg() = MapDpgsused() - 1;
	MapDpgsA() = atrpg;
	MapDpgsE() = 0;
	MapDpgsI() = 0;
	MapDpgsM() = 0;
	MapDpgsP() = 0;
	MapDpgsT() = 0;
	MapDpgsV() = 0;
	MapDpgsX() = 0;
	MapDpgsL() = 0; //V3.0

	//LPM
	MapEBMIX0() = -1;
	MapEBMIX1() = -1;
	MapEBMIX2() = -1;
	MapEBMIX3() = -1;
	MapBReuseQHead() = -1;
	MapDReuseQHead() = -1;

	//LPM and initial terminators
	MapInt32(DBP_F_B_D_EXTENTS) = 1; //table B starts after the FCT
	MapInt32(DBP_F_B_D_EXTENTS + 4) = MapBsize() + 1; //implied defrag during initialize
	MapInt32(DBP_F_B_D_EXTENTS + 8) = MapBsize() + MapDsize() + 1;
	MapInt32(DBP_F_B_D_EXTENTS + 12) = -1;
	MapInt32(DBP_F_B_D_EXTENTS + 16) = -1;
	MapFreeBase() = DBP_F_B_D_EXTENTS + 20;

	//Clear broadcast message by setting freetop to the end of the page
	MapFreeTop() = DBPAGE_SIZE;

	//----------------------------------------------
	//Initialize may or may not have collected the fatt chain at the start of table D.
	MapAtrpg() = atrpg;

	if (atrpg > 0)
		MapFattHead() = 0;
	else {
		MapFattHead() = -1;
		MapAtrfld() = 0;
	}
}

//************************************************************************************
std::string FCTPage::FormatOSFileName(const std::string& fullpath)
{
	//V2.04. Mar 07.  Must be const char*.
	const char* fname = strrchr(fullpath.c_str(), '\\') + 1;

	//We have 12 bytes allocated for this - not ideal but not bad.
	char buff[12];
	strncpy(buff, fname, 11);
	buff[11] = 0;
	
	return buff;
}

//************************************************************************************
BufferPageHandle FCTPage::GetAbsolutePageHandleFromRelative
(DatabaseServices* dbapi, BufferedFileInterface* buffapi, 
 int x_relpage_needed, bool fresh, bool tabled)
{
	if (x_relpage_needed < 0)
		throw Exception(DB_STRUCTURE_BUG, 
			std::string("File structure bug getting table ")
			.append( (tabled) ? "D page: " : "B page: ")
			.append("Negative table page number - FCT/GAPHFR"));

	//The algorithm for table D pages is just the same, except that we start by
	//bypassing the first table B extent.  In this code, x is the type of page we're
	//looking for, and y is the other type.
	int first_xchunk_ptr = (tabled) ? DBP_F_B_D_EXTENTS + 4 : DBP_F_B_D_EXTENTS;
	int prechunk_notx_pages = MapInt32(first_xchunk_ptr);
	int y_chunkbase = -1;

	//Scan extents (chunks) in pairs
	for (int ix = first_xchunk_ptr; ; ix += 8) {

		//Absolute pointer to the next extent of the desired type
		int x_chunkbase = MapInt32(ix);
		if (x_chunkbase == -1)
			throw Exception(DB_STRUCTURE_BUG, 
				std::string("File structure bug getting table ")
				.append( (tabled) ? "D page: " : "B page: ")
				.append("Page number too high - FCT/GAPHFR1"));

		//Discount non-table pages from previous time before getting next y_chunkbase
		if (y_chunkbase != -1) {
			int y_last_chunksize = x_chunkbase - y_chunkbase;
			prechunk_notx_pages += y_last_chunksize;
		}

		//How big's this extent?  There should be at least an extent pointer of the 
		//other type, even if no next extent yet.  Saves queriny OS file size itself.
		y_chunkbase = MapInt32(ix + 4);
		if (y_chunkbase == -1)
			throw Exception(DB_STRUCTURE_BUG, 
				std::string("File structure bug getting table ")
				.append( (tabled) ? "D page: " : "B page: ")
				.append("Page number too high - FCT/GAPHFR2"));

		int x_chunksize = y_chunkbase - x_chunkbase;

		//Is the requested page in the current extent?
		int x_relpage_from = x_chunkbase - prechunk_notx_pages;
		int x_chunkoffset_try = x_relpage_needed - x_relpage_from;

		//Yes - we've found what we want
		if (x_chunkoffset_try < x_chunksize) {
			int abspage = x_chunkbase + x_chunkoffset_try;
			return BufferPageHandle(dbapi, buffapi, abspage, fresh);
		}
	}
}







//************************************************************************************
//Functions accessed from the main class
//************************************************************************************
void FCTPage_F::EyeCheck()
{
	//This could easily be spoofed, but will prevent random data looking like a file.
	if (memcmp(MapPChar(DBP_F_FIL_EYE), "FIL:", 4)
	 || memcmp(MapPChar(DBP_F_FLD_EYE), "FLD:", 4)
	 || memcmp(MapPChar(DBP_F_DAT_EYE), "DAT:", 4)
	 || memcmp(MapPChar(DBP_F_IDX_EYE), "IDX:", 4)
	 || memcmp(MapPChar(DBP_F_DHP_EYE), "DHP:", 4)
	 || memcmp(MapPChar(DBP_F_LPM_EYE), "LPM:", 4))
		//V2.27 - better message
//		throw Exception(DB_BAD_FILE_CONTENTS, "The file contents are corrupt");
		throw Exception(DB_BAD_FILE_CONTENTS, "The file is not a DPT database file, or is corrupt (2)");
}

//************************************************************************************
bool FCTPage_F::Increase(int amount, bool tabled)
{
	//Start at the first extent pointer of this type
	int ptr = (tabled) ? DBP_F_B_D_EXTENTS + 4 : DBP_F_B_D_EXTENTS;

	//Locate the last extent pointer of this type
	while (MapInt32(ptr) != -1)
		ptr += 8;
	ptr -= 8;

	//Case 1 - the extent is already the last
	if (MapInt32(ptr+4) != -1)
		//Just move the pointer out
		MapInt32(ptr+4) += amount;

	//Case 2 - the other type of extent is the last
	else {

		//Is there any free space on the FCT for a new extent pointer?
		if (MapFreeBase() + 4 >= MapFreeTop())
			return false;

		//Yes - rejig the end of the LPM pointer area 
		MapInt32(ptr+4) = MapInt32(ptr) + amount;
		MapInt32(ptr+8) = -1;
		MapInt32(ptr+12) = -1;

		//And move the free space out 1 slot
		MapFreeBase() = MapFreeBase() + 4;
	}

	MakeDirty();
	if (tabled)
		MapDsize() = MapDsize() + amount;
	else
		MapBsize() = MapBsize() + amount;

	return true;
}

//************************************************************************************
void FCTPage_F::ShowTableExtents(std::vector<int>* result)
{
	result->clear();

	int ptr = DBP_F_B_D_EXTENTS + 4;	//table b always starts at page 1
	_int32 prev = 1;
	_int32 extent_startpage = MapInt32(ptr);
	_int32 tot = 0;

	while (extent_startpage != -1) {
		_int32 extent_size = extent_startpage - prev;
		tot += extent_size;

		result->push_back(extent_size);

		ptr += 4;
		prev = extent_startpage;
		extent_startpage = MapInt32(ptr);
	}

	//Calculate the last extent size
//	result->push_back(MapBsize() + MapDsize() - tot);
}

//************************************************************************************
std::string FCTPage_F::GetBroadcastMessage()
{
	std::string result;

	int msglen = DBPAGE_SIZE - MapFreeTop();
	if (msglen > 0) {
		char temp[DBPAGE_SIZE];
		memcpy(temp, MapPChar(MapFreeTop()), DBPAGE_SIZE - MapFreeTop());
		temp[msglen] = 0;
		result = temp;
	}

	return result;
}

//************************************************************************************
void FCTPage_F::SetBroadcastMessage(const std::string& msg)
{
	int newlen = msg.length();

	//Check there is room on the page
	if (MapFreeBase() + newlen >= DBPAGE_SIZE)
		throw Exception(DB_BAD_BROADCAST_MSG, 
			"The FCT hasn't enough room for the message "
			"- use a shorter message or reorg the file");

	MakeDirty(); 

	//Clear the old message for tidiness
	int oldlen = DBPAGE_SIZE - MapFreeTop();
	if (oldlen > newlen)
		SetChars(MapFreeTop(), 0, oldlen-newlen);

	//Set the new freetop pointer.  Note that this points to the start of the broadcast
	//message, which is actually *one past* the last free byte.
	MapFreeTop() = DBPAGE_SIZE - newlen;

	if (newlen > 0)
		WriteChars(MapFreeTop(), msg.data(), newlen);
}

//************************************************************************************
void FCTPage_F::SetLastOSFileName(const std::string& s)
{
	MakeDirty(); 
	strcpy(MapLastOSFileName(), s.c_str());
}

//************************************************************************************
void FCTPage_F::ManualResetFistat(unsigned char newval)
{
	MakeDirty();
	MapFistat() = newval;

	//These are maintained for $VIEW.  The user ID is used in lieu of a terminal ID.
	//An alternative would be IP address but in an API situation there would be none.
	strncpy(MapFiwho(), dbapi->Core()->GetUserID().c_str(), 8);
	time(MapFiwhen());
}

//************************************************************************************
//V3.0. This lets old files be used with BLOBs without needing to recreate them.
bool FCTPage_F::InitializeBLOBControlFieldsIfNeeded()
{
	//Luckily all database pages are zeroized when they're created, and this will
	//always be -1 or a positive number when it has valid information in it.
	if (MapEactive() != 0)
		return false;

	MakeDirty(); 

	MapEactive() = -1;
	MapBLOBQHead() = -1;
	MapDpgsL() = 0;

	return true;
}






//************************************************************************************
//Functions accessed from the EBM manager
//************************************************************************************
int FCTPage_E::GetEBMIndexPageNum(int ebmchunk)
{
	if (ebmchunk == 0)
		return MapEBMIX0();
	else if (ebmchunk == 1)
		return MapEBMIX1();
	else if (ebmchunk == 2)
		return MapEBMIX2();
	else if (ebmchunk == 3)
		return MapEBMIX3();
	else
		throw Exception(DB_STRUCTURE_BUG, "Invalid EBM chunk number");
}

//************************************************************************************
void FCTPage_E::SetEBMIndexPageNum(int ebmchunk, int pagenum)
{
	MakeDirty(); 

	if (ebmchunk == 0)
		MapEBMIX0() = pagenum;
	else if (ebmchunk == 1)
		MapEBMIX1() = pagenum;
	else if (ebmchunk == 2)
		MapEBMIX2() = pagenum;
	else if (ebmchunk == 3)
		MapEBMIX3() = pagenum;
	else
		throw Exception(DB_STRUCTURE_BUG, "Invalid EBM chunk number");
}






} //close namespace
