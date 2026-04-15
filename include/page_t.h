/****************************************************************************************
B-tree - all node types now.
****************************************************************************************/

#ifndef BB_PAGE_T
#define BB_PAGE_T

#include "pagebase.h"
#include "fieldval.h"

namespace dpt {

const short DBP_T_NUMERIC_FLAG	= DBPAGE_PAGEHEADER;

const short DBP_T_BLFREE		= DBPAGE_DATA + 0; //holds an area-relative number
const short DBP_T_PCKLEN		= DBPAGE_DATA + 2;
const short DBP_T_NUMVALS		= DBPAGE_DATA + 4;
//Leaf only, but kept for all node types to avoid unnecessary amounts of indirection
const short DBP_T_LEFTSIB		= DBPAGE_DATA + 6;
const short DBP_T_RIGHTSIB		= DBPAGE_DATA + 10;
const short DBP_T_TSTAMP		= DBPAGE_DATA + 14;

const short DBP_T_AREA_1_EYE	= DBPAGE_DATA + 32;
const short DBP_T_AREA_1_DATA	= DBPAGE_DATA + 36;
const short DBP_T_TREE_LOVALUE	= DBP_T_AREA_1_DATA + 0; //length 8

const short DBP_T_AREA_2_EYE	= DBPAGE_DATA + 64;
const short DBP_T_AREA_2_DATA	= DBPAGE_DATA + 68;
 
class BTreePage : public DatabaseFilePage {

	//This is local to save having to include dbserv.h here or derive this class
	//from Parameterized just for this.
	static short DBP_T_DEBUG_FILLER;

	//These are cached at construction time to save lots of indirection during use
	bool is_root;
	bool is_branch;
	bool is_leaf;
	bool tree_type_numeric;
	short bl_data_area_pos;
	inline void CacheFlags();

	//------------------------
	//General functions
	char& MapNumericFlag() {return MapInt8(DBP_T_NUMERIC_FLAG);}

	short& MapPCKLen() {return MapInt16(DBP_T_PCKLEN);}
	//Careful - returns null if there is no prefix to avoid addressing outside page
	char* PCKChars() {int i = MapPCKLen(); return (i==0) ? NULL : MapPChar(DBPAGE_SIZE-i);} 
	short& MapNumVals() {return MapInt16(DBP_T_NUMVALS);}

	_int32& MapLeftSibling() {return MapInt32(DBP_T_LEFTSIB);}
	_int32& MapRightSibling() {return MapInt32(DBP_T_RIGHTSIB);}
	_int32& MapTStamp() {return MapInt32(DBP_T_TSTAMP);}

	void WriteEyeCatchers();

	friend class BTreeAPI;
	friend class BTreeExtract;
	friend class BTreeAPI_Load;
	friend class DatabaseFileIndexManager; //for analyze3
	BTreePage(BufferPageHandle& bh);
	BTreePage(BufferPageHandle& bh, bool freshvalnum, char freshnodetype);

	//------------------------
	//Root functions
	void ConvertRootToPureNode(BTreePage&);
	short H_AreaSize() {return bl_data_area_pos - DBP_T_AREA_1_DATA;}
	void H_SetLowestValue(const FieldValue&);
	void H_GetLowestValue(FieldValue&);

	//------------------------
	//Branch or leaf area functions
	short BL_AreaSize() {return DBPAGE_SIZE - DBP_T_DEBUG_FILLER - bl_data_area_pos;}
	short BL_PagePos(short rp) {return bl_data_area_pos + rp;}

	unsigned char& BL_MapStrLen(short area_offset) {return MapUInt8(BL_PagePos(area_offset));}
	char* BL_MapPChar(short area_offset) {return MapPChar(BL_PagePos(area_offset));}

	short& BL_MapAreaFreeBase() {return MapInt16(DBP_T_BLFREE);}
	bool BL_PositionIsWithinData(short offset) {
		return (offset < BL_MapAreaFreeBase() && offset >= 0);}

	short BL_FreeBytes() {return (BL_AreaSize() - BL_MapAreaFreeBase() - MapPCKLen());}

	struct BL_EntryInfo {
		FieldValue value;
		int idata;
		short sdata;
	};
	short BL_DataItemSize() {if (is_leaf) return 6; else return 4;}
	bool BL_EntryHasSData() {return is_leaf;}

	void BL_GetData(short offset, int& i, short* s) {
		i = MapInt32(BL_PagePos(offset));
		if (!BL_EntryHasSData())
			return;
		*s = MapInt16(BL_PagePos(offset)+4);}

	void BL_WriteValue(short&, const FieldValue&);
	void BL_WriteData(short& offset, const int i, const short s = -1) {
		MapInt32(BL_PagePos(offset)) = i; offset += 4;
		if (!BL_EntryHasSData())
			return;
		MapInt16(BL_PagePos(offset)) = s; offset += 2;}
	void BL_WriteData_NoMove(short temp, const int i, const short s) {BL_WriteData(temp, i, s);}

	void BL_ExtractAllInfo(std::vector<BL_EntryInfo>*, 
							std::vector<BL_EntryInfo>* = NULL, int* = NULL);
	void BL_LoadAllInfo(const std::vector<BL_EntryInfo>*, short, 
							const FieldValue* = NULL, short* = NULL, short* = NULL);

	void BL_GetValue(short, FieldValue&);
	bool BL_LocateNextValue(short, short&, short&);
	bool BL_LocatePreviousValue(short, short&, short&);

	bool BL_InsertValue(const FieldValue&, short&, short&);
	bool BL_RemoveValue(short);
	void BL_DivideValues(BTreePage*, unsigned _int8, FieldValue&);

	static int BL_CalculatePCK(const std::vector<BL_EntryInfo>*);

	//------------------------
	//Branch area functions
	bool B_LocateValueLE(const FieldValue&, short&, short&);
	void B_SetChildPageNum(short area_offset, int i) {MapInt32(BL_PagePos(area_offset)) = i;}

	//------------------------
	//Leaf area functions
	int L_GetRightSibling() {return MapRightSibling();}
	int L_GetLeftSibling() {return MapLeftSibling();}
	void L_SetRightSibling(int p) {MakeDirty(); MapRightSibling() = p;}
	void L_SetLeftSibling(int p) {MakeDirty(); MapLeftSibling() = p;}

	int L_GetTStamp() {return MapTStamp();}
	void L_IncTStamp() {MapTStamp()++;}

	bool L_LocateValue(const FieldValue&, short&, short&);

	//Diagnostics
	std::string BL_FormatDumpDisplayString(short, int);

public:
	static void SetNodeFiller(short s) {DBP_T_DEBUG_FILLER = s;}
};
	
} //close namespace

#endif
