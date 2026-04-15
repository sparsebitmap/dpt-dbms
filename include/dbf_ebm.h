/*******************************************************************************************
A sub-class largely just to keep this stuff from clogging up the DatabaseFile code.
This class deals with the existence bit map.
*******************************************************************************************/
#if !defined(BB_DBF_EBM)
#define BB_DBF_EBM

#include "atom.h"
#include "atomback.h"
#include "page_e.h" //#include "page_E.h" : V2.24 case is less interchangeable on *NIX - Roger M.

namespace dpt {

class DatabaseFile;
class DatabaseServices;
class BitMappedFileRecordSet;
class SingleDatabaseFileContext;

//*****************************************************************************************
class DatabaseFileEBMManager {
	DatabaseFile* file;

	int ChunkNumFromSegNum(short segnum) {return segnum / DBP_E_NUMSLOTS;}
	int RelSegNumFromAbsSegNum(short segnum) {return segnum % DBP_E_NUMSLOTS;}

	BufferPageHandle GetSegmentBitMapPage(DatabaseServices*, short);
	void ExtendEBMToCoverSet(DatabaseServices* d, BitMappedFileRecordSet*);

public:
	DatabaseFileEBMManager(DatabaseFile* f) : file(f) {}

	//Used during store and delete, and their backouts
	void Atom_FlagSingleRecordExistence(DatabaseServices*, int, bool, bool = false);

	//Used during dirty delete, and its backout
	int Atom_FlagRecordSetExistence
		(DatabaseServices*, BitMappedFileRecordSet*, bool, BitMappedFileRecordSet** = NULL, bool = false);

	bool DoesPrimaryRecordExist(DatabaseServices*, int);
	BitMappedFileRecordSet* MaskOffNonexistentInSet(DatabaseServices*, BitMappedFileRecordSet*);
	BitMappedFileRecordSet* CreateWholeFileSet(DatabaseServices*, 
		SingleDatabaseFileContext*, BitMappedFileRecordSet* = NULL);

	//V3.0
	void ExistizeLoadedRecords(DatabaseServices* d, BitMappedFileRecordSet* loaded_set) {
		Atom_FlagRecordSetExistence(d, loaded_set, true, NULL, true);}
};






//**************************************************************************************
class AtomicUpdate_ExistizeStoredRecord : public AtomicUpdate {
	int recnum;

public:
	AtomicUpdate_ExistizeStoredRecord(SingleDatabaseFileContext* c, int r) 
		: AtomicUpdate(c), recnum(r) {}

	AtomicBackout* CreateCompensatingUpdate();
	void Perform();
};

//*********TBO*********
class AtomicBackout_DeExistizeStoredRecord : public AtomicBackout {
	int recnum;

public:
	AtomicBackout_DeExistizeStoredRecord(SingleDatabaseFileContext* c, int r) 
		: AtomicBackout(c), recnum(r) {}

	void Perform();
};






//**************************************************************************************
class AtomicUpdate_DeExistizeDeletedRecord : public AtomicUpdate {
	int recnum;

public:
	AtomicUpdate_DeExistizeDeletedRecord(SingleDatabaseFileContext* c, int r) 
		: AtomicUpdate(c), recnum(r) {}

	AtomicBackout* CreateCompensatingUpdate();
	void Perform();
};

//*********TBO*********
class AtomicBackout_ExistizeDeletedRecord : public AtomicBackout {
	int recnum;

public:
	AtomicBackout_ExistizeDeletedRecord(SingleDatabaseFileContext* c, int r) 
		: AtomicBackout(c), recnum(r) {}

	void Perform();
};






//**************************************************************************************
class AtomicUpdate_DirtyDeleteRecords : public AtomicUpdate {
	BitMappedFileRecordSet* bits_to_turn_off;

public:
	AtomicUpdate_DirtyDeleteRecords(SingleDatabaseFileContext* c, BitMappedFileRecordSet* s) 
		: AtomicUpdate(c), bits_to_turn_off(s) {}

	AtomicBackout* CreateCompensatingUpdate();
	void Perform();
};

//*********TBO*********
class AtomicBackout_DeDirtyDeleteRecords : public AtomicBackout {
	BitMappedFileRecordSet* bits_to_turn_back_on;

public:
	AtomicBackout_DeDirtyDeleteRecords(SingleDatabaseFileContext* c)
		: AtomicBackout(c), bits_to_turn_back_on(NULL) {}
	~AtomicBackout_DeDirtyDeleteRecords();

	void NoteBitsTurnedOff(BitMappedFileRecordSet* s) {bits_to_turn_back_on = s;}
	void Perform();
};


} //close namespace

#endif
