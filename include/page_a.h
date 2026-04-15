/****************************************************************************************
Field attributes pages 

Remember that these live in table D - there is no table A.
****************************************************************************************/

#ifndef BB_PAGE_A
#define BB_PAGE_A

#include "pagebase.h"
#include <string>

namespace dpt {

class DatabaseFileFieldManager;
class FieldAttributes;
struct PhysicalFieldInfo;

const short DBP_A_CHAIN_PTR		= DBPAGE_DATA + 0;
const short DBP_A_NEXT_FIELD_ID	= DBPAGE_DATA + 4;
const short DBP_A_FREEBASE		= DBPAGE_DATA + 6;

const short DBP_A_ATT_AREA	= DBPAGE_DATA + 16;

class FieldAttributePage : public DatabaseFilePage {
	friend class DatabaseFileFieldManager;

	static const short RESERVED_PAGE_SPACE;

	_int32& MapChainPtr() {return MapInt32(DBP_A_CHAIN_PTR);}
	short& MapNextFieldID() {return MapInt16(DBP_A_NEXT_FIELD_ID);}
	short& MapFreeBase() {return MapInt16(DBP_A_FREEBASE);}
	short NumFreeBytes() {return DBPAGE_SIZE - MapFreeBase();}

	FieldAttributePage(BufferPageHandle& bh) : DatabaseFilePage('A', bh, false) {} 
	FieldAttributePage(BufferPageHandle& bh, int);

	void VerifyFieldMissing(const std::string&);

	//Getters
	int GetChainPtr() {return MapChainPtr();}
	int GetNextFieldID() {return MapNextFieldID();}
	PhysicalFieldInfo* GetFieldInfo(short&);
	short LocateField(const std::string&);

	//Setters
	void SetChainPtr(int n) {MakeDirty(); MapChainPtr() = n;}
	int AppendFieldAtts(const std::string&, const FieldAttributes&);
	void InitializeDynamicFieldInfo(); //for soft initialize
	void SetSplitpct(short, unsigned char);
	void SetAttributeByte(short, unsigned char);
	bool ChangeFieldName(short, const std::string&);

	void DeleteFieldEntry(short);
	void RemoveIndexInfoBlock(short);
	void InsertIndexInfoBlock(short, PhysicalFieldInfo*);

	//Getter/setter
	bool UpdateBTreeRootPage(short&, PhysicalFieldInfo*);
};
	
} //close namespace

#endif
