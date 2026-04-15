
//Physical data access at the record level.  This class functions somewhat like a cursor
//on the physical record, and is how occurrence loops achieve the sequential scanning
//of a record without holding a lock across unpredictable waits.

#if !defined(BB_RECDATA)
#define BB_RECDATA

#include <string>
#include <vector>
#include "buffhandle.h"

namespace dpt {

class DatabaseServices;
class DatabaseFile;
class DatabaseFileTableDManager;
class DatabaseFileContext;
class Record;
struct PhysicalFieldInfo;
class FieldValue;
class RecordDataAccessor;
class RecordCopy;
typedef short FieldID;

//**************************************************************************************
class DataCursorInfo {
	FieldID fid;
	short recoffset;
	int occ;
	int recnum;
	int primary;

public:
	DataCursorInfo(int p) : primary(p) {ResetPos();}

	void ResetOcc()		{fid = -1; occ = 0;}
	void ResetPos()		{ResetOcc(); recnum = primary; recoffset = 0;}

	void ForceRescan() {ResetPos();}

	FieldID	Fid()		{return fid;}
	int		Occ()		{return occ;}
	int		RecNum()	{return recnum;}
	short	RecOffset() {return recoffset;}

	void	SetOcc(FieldID f, int o, int r, short ro) {
							fid = f; occ = o; recnum = r; recoffset = ro;}
	void	SetPos(int r, short ro) {
							ResetOcc(); recnum = r; recoffset = ro;}
	//V2.14 Feb 09.  {bug with the calling code}
//	void	MutateOcc(int o) {occ = o;}
};

//**************************************************************************************
class RecordDataAccessor {
	friend class Record;
	friend class DatabaseFileDataManager;
	friend class DatabaseFileIndexManager;

	Record* record;
	mutable DataCursorInfo data_cursor;
	DatabaseServices* deleter;
	bool noregister;

	mutable int cached_buffer_pagenum;
	mutable BufferPageHandle buffcache;
	mutable int pai_cursor_fvpix;

	void CachePageForRec(int) const;
	bool CursorAdvanceFieldOccurrence(PhysicalFieldInfo*) const;
	bool LocateFieldOccurrenceByNumber(PhysicalFieldInfo*, const int, int* = NULL) const;
	bool LocateFieldOccurrenceByValue(PhysicalFieldInfo*, const FieldValue&, int*) const;

	//V3.0 Extra parms for BLOB handling.
	//void RetrieveFVPair(FieldID*, FieldValue*) const;
	//void DeleteFVPair(bool = false, bool = false, FieldID* = NULL); 
	//void InsertFVPair_S(FieldID, const FieldValue&, int, short, int, bool); 
	void RetrieveFVPair(FieldID*, FieldValue*, bool = true, FieldValue* = NULL, int* = NULL) const;
	void DeleteFVPair(PhysicalFieldInfo*, bool = false, bool = false, FieldID* = NULL);
	void InsertFVPair_S(FieldID, bool, const FieldValue&, int, short, int, bool);
	void InsertFVPair_SRecurse(FieldID, const FieldValue&, int, short, int, bool);

	bool IxPrepFVPairNowMissing(PhysicalFieldInfo*, const FieldValue*);

	//So fastload can use it
	static int StoreBLOB(DatabaseServices*, DatabaseFileTableDManager*, 
		const FieldValue&, FieldValue&, const FieldValue**, bool&);

	//-------------------------
	RecordDataAccessor(int r, bool n) : record(NULL), data_cursor(r), deleter(NULL),
			noregister(n), cached_buffer_pagenum(-1), pai_cursor_fvpix(INT_MAX) {}
	void DelayedConstruct(Record*);
	~RecordDataAccessor();

	void ForceRescan(RecordDataAccessor* causer, bool deletion) {
		//After insert and change, the cursor on this thread is positioned fine
		if (deletion || causer != this)
			data_cursor.ForceRescan();}
	void MarkDeleted(DatabaseServices* d) {deleter = d;}
	void ThrowUnlockedRecordDeleted(bool) const;
	void UnlockedRecordDeletionCheck(bool u) const {if (deleter) ThrowUnlockedRecordDeleted(u);}

	//-----------------------
	//Field reading functions
	int CountOccurrences(PhysicalFieldInfo*) const;
	bool GetFieldValue(PhysicalFieldInfo*, int, FieldValue&, bool = true) const;

	bool GetNextFVPair(FieldID*, FieldValue*, FieldValue*, int&) const;
	void CopyAllInformation(RecordCopy&) const;

	//Field updating functions
	int InsertField(PhysicalFieldInfo*, const FieldValue&, int, bool*, bool = false);

	int ChangeField(bool, PhysicalFieldInfo*, const FieldValue&, 
					const int, const FieldValue*, int*, FieldValue*, bool*, bool*);

	int DeleteField(bool, PhysicalFieldInfo*, 
					const int, const FieldValue*, FieldValue*, bool*);

	//DBA functions
	void QuickDeleteEachOccurrence(PhysicalFieldInfo*);
	void QuickChangeEachOccurrenceFormat(PhysicalFieldInfo*, bool);
	void AppendVisiblizedValue(PhysicalFieldInfo*, const FieldValue&);
	void GetPageDataPointer(int*, const char**, short*);

	std::string ShowPhysicalInformation();

	//V3.0.  See comment in Record::Delete()
	void PreUpdatePeek() const;
};

} //close namespace

#endif
