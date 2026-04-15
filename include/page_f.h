/****************************************************************************************
The file control table.  A very heavily-used and important page.

There's only ever one of these per file.
****************************************************************************************/

#ifndef BB_PAGE_F
#define BB_PAGE_F

#include "pagebase.h"

namespace dpt {

//Data area layout
const short DBP_F_LASTOSNAME	= DBPAGE_DATA   +  0; // For warning message if OS-renamed
const short DBP_F_FREEBASE		= DBPAGE_DATA   + 12; // Cater for expanding extent info
const short DBP_F_FREETOP		= DBPAGE_DATA   + 14; // Cater for broadcast msg at other end

//File wide parms
const short DBP_F_FIL_EYE		= DBPAGE_DATA   + 16; // "FIL:" eyecatcher
const short DBP_F_FISTAT		= DBP_F_FIL_EYE +  4; // Bit settings - broken, full etc.
const short DBP_F_FIFLAGS		= DBP_F_FIL_EYE +  5; // More bits - reason codes for FISTAT
const short DBP_F_FICREATE		= DBP_F_FIL_EYE +  6; // 'A', 'B' etc.
const short DBP_F_FILEORG		= DBP_F_FIL_EYE +  7; // Bit settings - RRN etc.
const short DBP_F_SEQOPT		= DBP_F_FIL_EYE +  8; // Buffer control parameter
const short DBP_F_PAD1			= DBP_F_FIL_EYE +  9; // (3 bytes for alignment)
const short DBP_F_LAST_TXNTIME	= DBP_F_FIL_EYE + 12; // Logical update time
const short DBP_F_CPTIME		= DBP_F_FIL_EYE + 16; // Last checkpoint if updated since then
const short DBP_F_RBACK_CPTIME	= DBP_F_FIL_EYE + 20; // Time of the ckp we rolled back to
const short DBP_F_FIWHEN		= DBP_F_FIL_EYE + 24; // When FISTAT was reset (for $VIEW)
const short DBP_F_FIWHO			= DBP_F_FIL_EYE + 28; // Who did it (for $VIEW)
const short DBP_F_DU_NUMLEN		= DBP_F_FIL_EYE + 36; // Deferred updates - num length
const short DBP_F_DU_FORMAT		= DBP_F_FIL_EYE + 38; // DU format (V2.10 - Dec 07).

//Field attribute parms
const short DBP_F_FLD_EYE		= DBP_F_FIL_EYE + 64; // "FLD:" eyecatcher
const short DBP_F_ATRPG			= DBP_F_FLD_EYE +  4; // # attribute pages
const short DBP_F_ATRFLD		= DBP_F_FLD_EYE +  8; // # fields

//Data area parms
const short DBP_F_DAT_EYE		= DBP_F_FLD_EYE + 32; // "DAT:" eyecatcher
const short DBP_F_BSIZE			= DBP_F_DAT_EYE +  4; // Size of table B
const short DBP_F_BRECPPG		= DBP_F_DAT_EYE +  8; // Records per page
const short DBP_F_BRESERVE		= DBP_F_DAT_EYE + 12; // Bytes reserved for record expansion
const short DBP_F_BREUSE		= DBP_F_DAT_EYE + 16; // Required % > BRESERVE  to reuse
const short DBP_F_BHIGHPG		= DBP_F_DAT_EYE + 20; // Table B HWM
const short DBP_F_BQLEN			= DBP_F_DAT_EYE + 24; // # pages in the table B reuse queue
const short DBP_F_BREUSED		= DBP_F_DAT_EYE + 28; // (8) # pages reused so far
const short DBP_F_MSTRADD		= DBP_F_DAT_EYE + 36; // (8) Data records created so far
const short DBP_F_MSTRDEL		= DBP_F_DAT_EYE + 44; // (8) Data records deleted so far
const short DBP_F_EXTNADD		= DBP_F_DAT_EYE + 52; // (8) # Data extensions created so far
const short DBP_F_EXTNDEL		= DBP_F_DAT_EYE + 60; // (8) # data extensions deleted so far

//Index area parms
const short DBP_F_IDX_EYE		= DBP_F_DAT_EYE + 96; // "IDX:" eyecatcher
const short DBP_F_DACTIVE		= DBP_F_IDX_EYE +  4; // IL type 2 page active
const short DBP_F_DRESERVE		= DBP_F_IDX_EYE +  8; // (2) Bytes/page rsvd for IL expansion
const short DBP_F_DPGSRES		= DBP_F_IDX_EYE + 10; // (2) Pages rsvd for TBO
const short DBP_F_ILACTIVE		= DBP_F_IDX_EYE + 12; // Custom parm - much like DACTIVE
const short DBP_F_ILMRQ_HEAD	= DBP_F_IDX_EYE + 16; // ILMR reuse queue (stack) start
const short DBP_F_LISTQ_HEAD	= DBP_F_IDX_EYE + 20; // List page reuse queue (stack) start
const short DBP_F_EACTIVE		= DBP_F_IDX_EYE + 24; // Like DACTIVE and ILACTIVE           //V3.0
const short DBP_F_BLOBQ_HEAD	= DBP_F_IDX_EYE + 28; // BLOB page reuse queue (stack) start //V3.0

//General Table D "heap" parms
const short DBP_F_DHP_EYE		= DBP_F_IDX_EYE + 64; // "DHP:" eyecatcher
const short DBP_F_DSIZE			= DBP_F_DHP_EYE +  4; // Size of table D
const short DBP_F_DHIGHPG		= DBP_F_DHP_EYE +  8; // HWM
const short DBP_F_DPGSUSED		= DBP_F_DHP_EYE + 12; // Pages currently in use
const short DBP_F_DPGS_A		= DBP_F_DHP_EYE + 16; //	- subtype (same as ATRPG)
const short DBP_F_DPGS_E		= DBP_F_DHP_EYE + 20; //	- subtype
const short DBP_F_DPGS_I		= DBP_F_DHP_EYE + 24; //	- subtype
const short DBP_F_DPGS_M		= DBP_F_DHP_EYE + 28; //	- subtype
const short DBP_F_DPGS_P		= DBP_F_DHP_EYE + 32; //	- subtype
const short DBP_F_DPGS_T		= DBP_F_DHP_EYE + 36; //	- subtype
const short DBP_F_DPGS_V		= DBP_F_DHP_EYE + 40; //	- subtype
const short DBP_F_DPGS_X		= DBP_F_DHP_EYE + 44; //	- subtype
const short DBP_F_DPGS_L		= DBP_F_DHP_EYE + 48; //	- subtype (V3.0 BLOB pages)

//Logical page map
const short DBP_F_LPM_EYE		= DBP_F_DHP_EYE + 64; // "LPM:" eyecatcher
const short DBP_F_FATT_HEAD		= DBP_F_LPM_EYE +  4; // 1st page of field attribute chain
const short DBP_F_EBMIX0		= DBP_F_LPM_EYE +  8; // 1st page of EBM index
const short DBP_F_EBMIX1		= DBP_F_LPM_EYE + 12; // 2nd page of EBM index
const short DBP_F_EBMIX2		= DBP_F_LPM_EYE + 16; // 3rd page of EBM index
const short DBP_F_EBMIX3		= DBP_F_LPM_EYE + 20; // 4th page of EBM index (most possible)
const short DBP_F_BRQ_HEAD		= DBP_F_LPM_EYE + 24; // Table B reuse queue (stack) start
const short DBP_F_DRQ_HEAD		= DBP_F_LPM_EYE + 28; // Table B reuse queue (stack) start
const short DBP_F_B_D_EXTENTS	= DBP_F_LPM_EYE + 32; // Alternating extents of tables B/D

class BufferedFileInterface;
class FCTPage_F;
class FCTPage_B;
class FCTPage_D;
class FCTPage_E;
class FCTPage_A;

//**************************************************************************************
class FCTPage : public DatabaseFilePage {

	friend class FCTPage_F;
	friend class FCTPage_B;
	friend class FCTPage_D;
	friend class FCTPage_E;
	friend class FCTPage_A;

	//General info
	unsigned short&  MapFreeBase()	{return MapUInt16(DBP_F_FREEBASE);}
	unsigned short&  MapFreeTop()	{return MapUInt16(DBP_F_FREETOP);}

	char*  MapLastOSFileName()		{return MapPChar(DBP_F_LASTOSNAME);}

	//File-wide parms
	unsigned char&  MapFistat()			{return MapUInt8 (DBP_F_FISTAT);}
	unsigned char&	MapFiflags()		{return MapUInt8 (DBP_F_FIFLAGS);}
	unsigned char&  MapFicreate()		{return MapUInt8 (DBP_F_FICREATE);}
	unsigned char&  MapFileorg()		{return MapUInt8 (DBP_F_FILEORG);}
	unsigned char&	MapSeqopt()			{return MapUInt8 (DBP_F_SEQOPT);}
	time_t*			MapLastTxnTime()	{return MapTime  (DBP_F_LAST_TXNTIME);}
	time_t*			MapLastCPTime()		{return MapTime  (DBP_F_CPTIME);}
	time_t*			MapRBCPTime()		{return MapTime  (DBP_F_RBACK_CPTIME);}
	time_t*			MapFiwhen()			{return MapTime  (DBP_F_FIWHEN);}
	char*			MapFiwho()			{return MapPChar (DBP_F_FIWHO);}
	short&			MapDULen()			{return MapInt16 (DBP_F_DU_NUMLEN);}
	short&			MapDUFormat()		{return MapInt16 (DBP_F_DU_FORMAT);}

	//Field attribute parms
	unsigned short& MapAtrpg()		{return MapUInt16(DBP_F_ATRPG);}
	unsigned short& MapAtrfld()		{return MapUInt16(DBP_F_ATRFLD);}

	//Data area attribute parms
	_int32& MapBsize()		{return MapInt32(DBP_F_BSIZE);}
	_int32& MapBhighpg()	{return MapInt32(DBP_F_BHIGHPG);}
	short& MapBrecppg()		{return MapInt16(DBP_F_BRECPPG);}
	short& MapBreserve()	{return MapInt16(DBP_F_BRESERVE);}
	char&  MapBreuse()		{return MapInt8 (DBP_F_BREUSE);}
	_int32& MapBqlen()		{return MapInt32(DBP_F_BQLEN);}

	_int64& MapBreused()	{return MapInt64(DBP_F_BREUSED);}
	_int64& MapMstradd()	{return MapInt64(DBP_F_MSTRADD);}
	_int64& MapMstrdel()	{return MapInt64(DBP_F_MSTRDEL);}
	_int64& MapExtnadd()	{return MapInt64(DBP_F_EXTNADD);}
	_int64& MapExtndel()	{return MapInt64(DBP_F_EXTNDEL);}

	//Index area attribute parms
	_int32& MapDactive()	{return MapInt32(DBP_F_DACTIVE);}
	short& MapDreserve()	{return MapInt16(DBP_F_DRESERVE);}
	short& MapDpgsres()		{return MapInt16(DBP_F_DPGSRES);}
	//Custom
	_int32& MapIlactive()	{return MapInt32(DBP_F_ILACTIVE);}
	_int32& MapILMRQHead()	{return MapInt32(DBP_F_ILMRQ_HEAD);}
	_int32& MapListQHead()	{return MapInt32(DBP_F_LISTQ_HEAD);}
	_int32& MapEactive()	{return MapInt32(DBP_F_EACTIVE);}    //V3.0
	_int32& MapBLOBQHead()	{return MapInt32(DBP_F_BLOBQ_HEAD);} //V3.0

	//Heap stats
	_int32& MapDsize()		{return MapInt32(DBP_F_DSIZE);}
	_int32& MapDhighpg()	{return MapInt32(DBP_F_DHIGHPG);}
	_int32& MapDpgsused()	{return MapInt32(DBP_F_DPGSUSED);}
	_int32& MapDpgsA()		{return MapInt32(DBP_F_DPGS_A);}
	_int32& MapDpgsE()		{return MapInt32(DBP_F_DPGS_E);}
	_int32& MapDpgsI()		{return MapInt32(DBP_F_DPGS_I);}
	_int32& MapDpgsM()		{return MapInt32(DBP_F_DPGS_M);}
	_int32& MapDpgsP()		{return MapInt32(DBP_F_DPGS_P);}
	_int32& MapDpgsT()		{return MapInt32(DBP_F_DPGS_T);}
	_int32& MapDpgsV()		{return MapInt32(DBP_F_DPGS_V);}
	_int32& MapDpgsX()		{return MapInt32(DBP_F_DPGS_X);}
	_int32& MapDpgsL()		{return MapInt32(DBP_F_DPGS_L);} //V3.0

	//LPM pointers
	_int32& MapFattHead()		{return MapInt32(DBP_F_FATT_HEAD);}
	_int32& MapEBMIX0()			{return MapInt32(DBP_F_EBMIX0);}
	_int32& MapEBMIX1()			{return MapInt32(DBP_F_EBMIX1);}
	_int32& MapEBMIX2()			{return MapInt32(DBP_F_EBMIX2);}
	_int32& MapEBMIX3()			{return MapInt32(DBP_F_EBMIX3);}
	_int32& MapBReuseQHead()	{return MapInt32(DBP_F_BRQ_HEAD);}
	_int32& MapDReuseQHead()	{return MapInt32(DBP_F_DRQ_HEAD);}

	BufferPageHandle GetAbsolutePageHandleFromRelative
		(DatabaseServices*, BufferedFileInterface*, int, bool, bool);

	void InitializeValues(int);
	std::string FormatOSFileName(const std::string&);

	FCTPage(DatabaseServices* d, BufferPage* bp) : DatabaseFilePage(d, 'F', bp) {};
	FCTPage(RawPageData*, int, int, int, int, int, int, int, int, const std::string&);
	
	RawPageData* RawPage() {return RawPage_S();}
};



//******************************************************************************
//Interface classes 
//******************************************************************************
class FCTPage_F : public FCTPage  {
	friend class DatabaseFile;

	FCTPage_F(DatabaseServices* d, BufferPage* bp) : FCTPage(d, bp) {};
	FCTPage_F(RawPageData* rp, int i1, int i2, int i3, int i4, 
				int i5, int i6, int i7, int i8, const std::string& s)
		: FCTPage(rp, i1, i2, i3, i4, i5, i6, i7, i8, s) {}

	void InitializeValues(int atrpg) {FCTPage::InitializeValues(atrpg);}
	std::string FormatOSFileName(const std::string& s) {return FCTPage::FormatOSFileName(s);}

	bool Increase(int, bool);
	void ShowTableExtents(std::vector<int>*);
	void EyeCheck();
	bool MultiBExtents() {return (MapInt32(DBP_F_B_D_EXTENTS + 12) != -1);}
	bool MultiDExtents() {return (MapInt32(DBP_F_B_D_EXTENTS + 16) != -1);}

	std::string GetBroadcastMessage();
	void SetBroadcastMessage(const std::string&);

	std::string GetLastOSFileName() {return MapLastOSFileName();}
	void SetLastOSFileName(const std::string& s);

	unsigned char GetFistat() {return MapFistat();}
	unsigned char GetFiflags() {return MapFiflags();}
	unsigned char GetFicreate() {return MapFicreate();}
	unsigned char GetFileorg() {return MapFileorg();}
	unsigned char GetSeqopt() {return MapSeqopt();}
	void SetSeqopt(unsigned char i) {MakeDirty(); MapSeqopt() = i;}
	short GetDULen() {return MapDULen();}
	short GetDUFormat() {return MapDUFormat();}
	void SetDULen(short s) {MakeDirty(); MapDULen() = s;}
	void SetDUFormat(short s) {MakeDirty(); MapDUFormat() = s;}

	int GetBsize() {return MapBsize();}
	int GetDsize() {return MapDsize();}

	void OrFistat(unsigned char i, bool direct = false) {
		if (!direct) MakeDirty(); MapFistat() |= i;} //recovered is written directly
	void NandFistat(unsigned char i) {MakeDirty(); MapFistat() &= ~i;}
	void ManualResetFistat(unsigned char i);

	void OrFiflags(unsigned char i) {MakeDirty(); MapFiflags() |= i;}
	void NandFiflags(unsigned char i, bool direct = false) {
		if (!direct) MakeDirty(); MapFiflags() &= ~i;} //page-unbroken is written directly

	time_t GetLastTxnTime() {return *(MapLastTxnTime());}
	time_t GetLastCPTime() {return *(MapLastCPTime());}
	time_t GetRollbackCPTime() {return *(MapRBCPTime());}

	void SetLastTransactionTime() {MakeDirty(); time(MapLastTxnTime());}
	void SetLastCPTime(time_t t) {MakeDirty(); *(MapLastCPTime()) = t;}
	void SetRollbackCPTime(time_t t) {*(MapRBCPTime()) = t;}

	time_t GetFiwhen() {return *(MapFiwhen());}
	std::string GetFiwho() {char c[9]; memcpy(c, MapFiwho(), 8); c[8] = 0; return c;};

	bool InitializeBLOBControlFieldsIfNeeded(); //V3.0
};



//******************************************************************************
//******************************************************************************
class FCTPage_B : public FCTPage  {
	friend class DatabaseFileTableBManager;

	FCTPage_B(DatabaseServices* d, BufferPage* bp) : FCTPage(d, bp) {};

	int GetBhighpg()	{return MapBhighpg();}
	int GetBrecppg()	{return MapBrecppg();}
	int GetBreserve()	{return MapBreserve();}
	int GetBreuse()		{return MapBreuse();}
	int GetBqlen()		{return MapBqlen();}

	void IncBhighpg() {MakeDirty(); MapBhighpg()++;}

	void SetBreserve(int i) {MakeDirty(); MapBreserve() = i;}
	void SetBreuse(int i) {MakeDirty(); MapBreuse() = i;}
	int GetBReuseQHead() {return MapInt32(DBP_F_BRQ_HEAD);}
	void SetBReuseQHead(int newhead, bool lengthen) {
		MakeDirty(); MapInt32(DBP_F_BRQ_HEAD) = newhead; MapBqlen() += (lengthen) ? 1 : -1;} 
	void IncBreused() {MakeDirty(); MapBreused()++;}

	_int64 GetBreused()	{return MapBreused();}
	_int64 GetMstradd()	{return MapMstradd();}
	_int64 GetMstrdel()	{return MapMstrdel();}
	_int64 GetExtnadd()	{return MapExtnadd();}
	_int64 GetExtndel()	{return MapExtndel();}

	void IncMstradd() {MakeDirty(); MapMstradd()++;}
	void IncExtnadd() {MakeDirty(); MapExtnadd()++;}
	void IncMstrdel() {MakeDirty(); MapMstrdel()++;}
	void IncExtndel() {MakeDirty(); MapExtndel()++;}
	void DecMstrdel() {MakeDirty(); MapMstrdel()--;}

	BufferPageHandle GetAbsoluteBPageHandle
		(DatabaseServices* dbapi, BufferedFileInterface* fileapi, int tablepage, bool fresh) 
	{
		return GetAbsolutePageHandleFromRelative(dbapi, fileapi, tablepage, fresh, false);
	}
};



//******************************************************************************
//******************************************************************************
class FCTPage_D : public FCTPage {
	friend class DatabaseFileTableDManager;

	FCTPage_D(DatabaseServices* d, BufferPage* bp) : FCTPage(d, bp) {};

	int GetDReuseQHead() {return MapInt32(DBP_F_DRQ_HEAD);}

	int GetDsize()		{return MapDsize();}
	int GetDpgsres()	{return MapDpgsres();}
	int GetDhighpg()	{return MapDhighpg();}
	int GetDpgsused()	{return MapDpgsused();}
	int GetDreserve()	{return MapDreserve();}

	int GetDactive()	{return MapDactive();}
	int GetIlactive()	{return MapIlactive();}
	int GetILMRQHead()	{return MapILMRQHead();}
	int GetListQHead()	{return MapListQHead();}
	int GetEactive()	{return MapEactive();}    //V3.0
	int GetBLOBQHead()	{return MapBLOBQHead();}  //V3.0

	int GetDpgsA()		{return MapDpgsA();}
	int GetDpgsE()		{return MapDpgsE();}
	int GetDpgsI()		{return MapDpgsI();}
	int GetDpgsL()		{return MapDpgsL();} //V3.0
	int GetDpgsM()		{return MapDpgsM();}
	int GetDpgsP()		{return MapDpgsP();}
	int GetDpgsT()		{return MapDpgsT();}
	int GetDpgsV()		{return MapDpgsV();}
	int GetDpgsX()		{return MapDpgsX();}

	void IncDhighpg() {MakeDirty(); MapDhighpg()++;}

	void IncDpgsused() {MakeDirty(); MapDpgsused()++;}
	void IncDpgsA() {MakeDirty(); MapDpgsA()++;}
	void IncDpgsE() {MakeDirty(); MapDpgsE()++;}
	void IncDpgsI() {MakeDirty(); MapDpgsI()++;}
	void IncDpgsL() {MakeDirty(); MapDpgsL()++;} //V3.0
	void IncDpgsM() {MakeDirty(); MapDpgsM()++;}
	void IncDpgsP() {MakeDirty(); MapDpgsP()++;}
	void IncDpgsT() {MakeDirty(); MapDpgsT()++;}
	void IncDpgsV() {MakeDirty(); MapDpgsV()++;}
	void IncDpgsX() {MakeDirty(); MapDpgsX()++;}

	void DecDpgsused() {MakeDirty(); MapDpgsused()--;}
	void DecDpgsA() {MakeDirty(); MapDpgsA()--;}
	void DecDpgsE() {MakeDirty(); MapDpgsE()--;}
	void DecDpgsI() {MakeDirty(); MapDpgsI()--;}
	void DecDpgsL() {MakeDirty(); MapDpgsL()--;} //V3.0
	void DecDpgsM() {MakeDirty(); MapDpgsM()--;}
	void DecDpgsP() {MakeDirty(); MapDpgsP()--;}
	void DecDpgsT() {MakeDirty(); MapDpgsT()--;}
	void DecDpgsV() {MakeDirty(); MapDpgsV()--;}
	void DecDpgsX() {MakeDirty(); MapDpgsX()--;}

	void SetDReuseQHead(int i) {MakeDirty(); MapInt32(DBP_F_DRQ_HEAD) = i;}

	void SetDpgsres(int i) {MakeDirty(); MapDpgsres() = i;}
	void SetDreserve(int i) {MakeDirty(); MapDreserve() = i;}

	void SetDactive(int i)	{MakeDirty(); MapDactive() = i;}
	void SetIlactive(int i)	{MakeDirty(); MapIlactive() = i;}
	void SetILMRQHead(int i) {MakeDirty(); MapILMRQHead() = i;}
	void SetListQHead(int i) {MakeDirty(); MapListQHead() = i;}
	void SetEactive(int i)	{MakeDirty(); MapEactive() = i;}    //V3.0
	void SetBLOBQHead(int i) {MakeDirty(); MapBLOBQHead() = i;} //V3.0

	BufferPageHandle GetAbsoluteDPageHandle
		(DatabaseServices* dbapi, BufferedFileInterface* fileapi, int tablepage, bool fresh) 
	{
		return GetAbsolutePageHandleFromRelative(dbapi, fileapi, tablepage, fresh, true);
	}
};



//******************************************************************************
//******************************************************************************
class FCTPage_E : public FCTPage {
	friend class DatabaseFileEBMManager;

	FCTPage_E(DatabaseServices* d, BufferPage* bp) : FCTPage(d, bp) {};

	int GetEBMIndexPageNum(int ch);
	void SetEBMIndexPageNum(int ch, int p);

	void ChangeMstrdel(int n) {MakeDirty(); MapMstrdel()+=n;} //for dirty delete
};



//******************************************************************************
//******************************************************************************
class FCTPage_A : public FCTPage {
	friend class DatabaseFileFieldManager;

	FCTPage_A(DatabaseServices* d, BufferPage* bp) : FCTPage(d, bp) {};

	int GetFattHead()	{return MapFattHead();}
	int GetAtrpg()		{return MapAtrpg();}
	int GetAtrfld()		{return MapAtrfld();}

	void SetFattHead(int p)	{MakeDirty(); MapFattHead() = p;}
	void IncAtrpg()		{MakeDirty(); MapAtrpg()++;}
	void IncAtrfld()	{MakeDirty(); MapAtrfld()++;}
};

} //close namespace

#endif
