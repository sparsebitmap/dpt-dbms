/*******************************************************************************************
A sub-class largely just to keep this stuff from clogging up the DatabaseFile code.
Management of physical table B pages - nearly always called by the stuff in dbf_data.
*******************************************************************************************/
#if !defined(BB_DBF_TABLEB)
#define BB_DBF_TABLEB

#include <string>
#include "buffhandle.h"
#include "infostructs.h"

namespace dpt {

class DatabaseServices;
class DatabaseFile;
class DatabaseFileDataManager;
class RecordDataAccessor;
class LabelDefinitionWithBufferPage;
class RecordDataPage;
class FieldValue;
class SingleDatabaseFileContext;

//**************************************************************************************
class DatabaseFileTableBManager {
	DatabaseFile* file;

	int cached_brecppg;
	int cached_breserve;
	int cached_breuse;
	bool cached_rrn_flag;

	friend class DatabaseFileDataManager;

	//V3.0 In a load we allocate extensions knowing more than just the next field value
//	int AllocateNewRecordSlot(DatabaseServices*, bool, const FieldValue*);
//	int AllocatePrimaryRecordExtent(DatabaseServices* d, bool b) {
//										return AllocateNewRecordSlot(d, b, NULL);}
	int AllocateNewRecordSlot(DatabaseServices*, bool, bool, short);
	int AllocatePrimaryRecordExtent(DatabaseServices* d, bool b) {
										return AllocateNewRecordSlot(d, b, false, 0);}
	int AllocateExtensionRecordExtent(DatabaseServices* d, bool b, bool e, short el) {
										return AllocateNewRecordSlot(d, b, e, el);}
	int AllocateExtensionRecordExtent(DatabaseServices* d, bool b, const FieldValue* v);

	void DeletePrimaryExtent(DatabaseServices*, int);
	void RestoreDeletedPrimaryExtent(DatabaseServices*, int);
	void DeleteExtensionRecordExtent(DatabaseServices*, RecordDataPage*, int, int);

	int BPageNumFromRecNum(int recnum) {return recnum / cached_brecppg;}
	short BPageSlotFromRecNum(int recnum) {return recnum % cached_brecppg;}
	int BaseRecNumOnPage(int pagenum) {return pagenum * cached_brecppg;}

	//For the REQUEST PAGE statement
	friend class LabelDefinitionWithBufferPage;
	friend class RecordDataAccessor;
	BufferPageHandle GetTableBPage(DatabaseServices*, int, bool = false);

public:
	DatabaseFileTableBManager(DatabaseFile* f) : file(f) {}

	void CacheParms();
	std::string ViewParm(SingleDatabaseFileContext*, const std::string&);
	void ResetParm(SingleDatabaseFileContext*, const std::string&, int);

	void Dump(DatabaseServices*, BB_OPDEVICE*, bool, bool, int = -1, int = -1);
	void ValidateBsize(DatabaseServices*, _int64, _int64 = -1);
};

} //close namespace

#endif
