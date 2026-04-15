/****************************************************************************************
This class is a base class shared by all the database page mapper classes, which 
collectively form our interface to the data stored in database files.  In all cases 
these classes are pure interfaces, having no data of their own and just reporting on,
and changing the data stored in the page of memory to which they are pointed.
****************************************************************************************/

#ifndef BB_PAGEBASE
#define BB_PAGEBASE

#include <time.h>
#include "rawpage.h"
#include "buffhandle.h"
#include <string>
#include <vector>

namespace dpt {

class BufferPage;
class BufferedFileInterface;
class ZapPageQop;
class RoundedDouble;

//**************************************************************************************
const short DBPAGE_HEADER		= 0;

const short DBPAGE_SYSHEADER	= 0;
const short DBPAGE_PAGETYPE		= DBPAGE_SYSHEADER + 0;
const short DBPAGE_PAGESUBTYPE	= DBPAGE_SYSHEADER + 1;
const short DBPAGE_DREUSE_PTR	= DBPAGE_SYSHEADER + 2;  //used by all but F & B

const short DBPAGE_PAGEHEADER	= 16;
const short DBPAGE_DATA			= 32;

const short DBPAGE_USABLE_SPACE	= DBPAGE_SIZE - DBPAGE_DATA;	//8160
const short DBPAGE_BITMAP_BYTES	= DBPAGE_USABLE_SPACE;          //8160
const int DBPAGE_BITMAP_SIZE	= DBPAGE_BITMAP_BYTES * 8;		//65280
const int DBPAGE_SEGMENT_SIZE	= DBPAGE_BITMAP_SIZE;			//65280

//**************************************************************************************
inline short SegNumFromAbsRecNum(int recnum) 
{
	int isegnum = recnum / DBPAGE_SEGMENT_SIZE;
	return isegnum;
}

inline unsigned short RelRecNumFromAbsRecNum(int absrecnum, short segnum = -1) 
{
	if (segnum == -1)
		segnum = SegNumFromAbsRecNum(absrecnum);

	int seg_base_rec = segnum;
	seg_base_rec *= DBPAGE_SEGMENT_SIZE;

	int irelrecnum = absrecnum - seg_base_rec;
	return irelrecnum;
}

inline int AbsRecNumFromRelRecNum(unsigned short relrecnum, short segnum) 
{
	int seg_base_rec = segnum;
	seg_base_rec *= DBPAGE_SEGMENT_SIZE;

	int absrecnum = seg_base_rec;
	absrecnum += relrecnum;

	return absrecnum;
}

//**************************************************************************************
class DatabaseFilePage {
	RawPageData* data;
	BufferPage* buff;

	char* Cx(short i = 0) {
//#ifdef _DEBUG
//		char* debug_pch = data->CharData() + i;
//		unsigned char  debug_i8 	= *(reinterpret_cast<unsigned char*>(debug_pch));
//				 char  debug_ui8	= *(reinterpret_cast<         char*>(debug_pch));
//				 
//		if (i < (DBPAGE_SIZE - 1)) {
//		unsigned short debug_i16	= *(reinterpret_cast<unsigned short*>(debug_pch));
//				 short debug_ui16	= *(reinterpret_cast<         short*>(debug_pch));	
//		if (i < (DBPAGE_SIZE - 3)) {
//		unsigned _int32 debug_i32	= *(reinterpret_cast<unsigned _int32*>(debug_pch));
//				 _int32 debug_ui32	= *(reinterpret_cast<         _int32*>(debug_pch));	
//		if (i < (DBPAGE_SIZE - 7)) {
//		unsigned _int64 debug_i64	= *(reinterpret_cast<unsigned _int64*>(debug_pch));
//				 _int64 debug_ui64	= *(reinterpret_cast<         _int64*>(debug_pch));
//		}}}
//#endif
		return data->CharData() + i;}

	void Construct(DatabaseServices*, char, BufferPage*, bool);
	void Construct(char, RawPageData*, bool);

protected:
	DatabaseServices* dbapi;

	//Seems to make smaller code with no trailing default parms
	DatabaseFilePage(DatabaseServices* d, char c, BufferPage* b, bool f) {Construct(d, c, b, f);}
	DatabaseFilePage(DatabaseServices* d, char c, BufferPage* b) {Construct(d, c, b, false);}
	DatabaseFilePage(char c, BufferPageHandle& h, bool f) {Construct(h.dbapi, c, h.buff, f);}
	DatabaseFilePage(char c, BufferPageHandle& h) {Construct(h.dbapi, c, h.buff, false);}
	DatabaseFilePage(char c, RawPageData* r, bool f) {Construct(c, r, f);}
	DatabaseFilePage(char c, RawPageData* r) {Construct(c, r, false);}

	//Casting functions used everywhere
	friend class DatabaseFilePagePart;
	unsigned char&  MapUInt8 (short i) {return *(reinterpret_cast<unsigned char* >(Cx(i)));}
			 char&  MapInt8  (short i) {return *(reinterpret_cast<         char* >(Cx(i)));}
	unsigned short& MapUInt16(short i) {return *(reinterpret_cast<unsigned short*>(Cx(i)));}
			 short& MapInt16 (short i) {return *(reinterpret_cast<         short*>(Cx(i)));}
	unsigned _int32& MapUInt32(short i) {return *(reinterpret_cast<unsigned _int32*>(Cx(i)));}
			 _int32& MapInt32 (short i) {return *(reinterpret_cast<         _int32*>(Cx(i)));}
	unsigned _int64& MapUInt64(short i) {return *(reinterpret_cast<unsigned _int64*>(Cx(i)));}
			 _int64& MapInt64 (short i) {return *(reinterpret_cast<         _int64*>(Cx(i)));}

	//No need to round float values on the way out, so cast straight into a rounded double
	RoundedDouble& MapRoundedDouble(short i) {
								return *(reinterpret_cast<RoundedDouble*>(Cx(i)));}
//	//On the way in all interfaces are already rounded, so can go str8 to double on disk
//	double& MapDouble(short i) {return *(reinterpret_cast<double*>(Cx(i)));}

	char* MapPChar(short i) {return Cx(i);}
	time_t* MapTime(short i) {return reinterpret_cast<time_t*>(Cx(i));}

	void WriteChars(short i, const char* s, short n) {memcpy(Cx(i), s, n);}
	void SetChars(short i, const char c, short n) {memset(Cx(i), c, n);}

	//The derived classes can publish pass-throughs to these if appropriate.  This class
	//does not use virtual functions so as not to screw up its mapping with a vtable.
	RawPageData* RawPage_S() {return data;}
	char& PageType_S() {return *(Cx(DBPAGE_PAGETYPE));}
	char& PageSubType_S() {return *(Cx(DBPAGE_PAGESUBTYPE));}
	char* RealData_S() {return Cx(DBPAGE_DATA);}

	void MakeDirty_S();
	void MakeDirty() {MakeDirty_S();}

//	void LateXXXConstruct_S(char c, BufferPageHandle& h, bool f = false) {
//												Construct(h.dbapi, c, h.buff, f);}
//	void LateXXXConstruct_S(char c, RawPageData* r, bool f = false) {
//												Construct(c, r, f);}
};

//**************************************************************************************
class GenericPage : public DatabaseFilePage {

	friend class BufferPage;
//	void SetTimeStamp() {time(MapTime(DBPAGE_TIME_STAMP));} //already dirty

public:
	GenericPage(BufferPageHandle& bh) : DatabaseFilePage('*', bh) {}
	GenericPage(RawPageData* rp, bool f) : DatabaseFilePage('*', rp, f) {}
	GenericPage(RawPageData* rp) : DatabaseFilePage('*', rp, false) {}

	void CopyData(char* pc, short len) {memcpy(pc, RawPage_S(), len);}

	//Used mainly for blanking pages during initialize, but also the ZAP PAGE statement
	void Overwrite(RawPageData* r) {MakeDirty(); memcpy(RawPage_S(), r, DBPAGE_SIZE);}
	void Overwrite(GenericPage& p) {Overwrite(p.RawPage_S());}

	char PageType() {return PageType_S();}
	char PageSubType() {return PageSubType_S();}
};

	
} //close namespace

#endif
