/****************************************************************************************
Table B data page
****************************************************************************************/

#ifndef BB_PAGE_B
#define BB_PAGE_B

#include "pageslotrec.h"

namespace dpt {

const short DBP_B_RRNCHAR = DBPAGE_PAGEHEADER; //duplicate fileorg bit for convenience

class FieldValue;
struct PhysicalFieldInfo;
typedef short FieldID;

//***************************************************************************************
class RecordDataPage : public SlotAndRecordPage {

	friend class DatabaseFileTableBManager;
	friend class DatabaseFileDataManager;
	friend class RecordDataAccessor;

	bool RRN() {return (MapInt8(DBP_B_RRNCHAR) == 'R');}

	_int32& MapExtensionRecordNumber(short s) {return MapInt32(MapRecordOffset(s));}

	RecordDataPage(BufferPageHandle& bh) : SlotAndRecordPage('B', bh) {} 
	RecordDataPage(BufferPageHandle&, short, bool);

	//Functions using page-relative offsets
	bool PageAdvanceField(short&, short);
	void PageGetFieldValue(short, FieldValue&);
	FieldID PageGetFieldCode(short p) {return RealFieldCode(MapInt16(p));}
	bool PageFieldIsFloat(short p) {return RealFieldIsFloat(MapInt16(p));}
	bool PageFieldIsBLOB(short p) {return RealFieldIsBLOB(MapInt16(p));}

	//-----------------------------------------------------------------
	//Interface functions
//	short AllocateSlotWithoutReuse(int, int, const FieldValue*);
	short AllocateSlotWithoutReuse(int, int, bool, short); //V3.0
	short AllocateSlotWithReuse(int, int);
	short EmptyRecordLength() {return 4;}
	void UndeleteSlot(short);
	void RecordExistsCheck();

	bool SlotAdvanceField(short, short&, PhysicalFieldInfo*);
	bool SlotLocateLastAnyField(short, short&);

	int SlotInsertFVPair(short, short, FieldID, const FieldValue&);
	bool SlotDeleteFVPair(short, short, FieldID* = NULL);
	void SlotGetFVPair(short, short, FieldID*, FieldValue*);

	int SlotGetExtensionPtr(short);
	void SlotSetExtensionPtr(short, int);

	//V3.0
	char* SlotPrepareSpaceForLoadData(short, short);

public:
	//V3.0
	static FieldID RealFieldCode(short pagecode) {return pagecode & 0x0fff;}
	//X'1000' means numeric
	static bool RealFieldIsFloat(short pagecode) {return pagecode & 0x1000 ? true : false;}
	static void MakeNumericPageCode(short& realcode) {realcode |= 0x1000;}
	//X'2000' means BLOB
	static bool RealFieldIsBLOB(short pagecode) {return pagecode & 0x2000 ? true : false;}
	static void MakeBLOBPageCode(short& realcode) {realcode |= 0x2000;}
};
	
} //close namespace

#endif
