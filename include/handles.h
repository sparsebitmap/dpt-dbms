//*****************************************************************************************
//Helper classes for use with various API MROs.  e.g. The cursor handle classes make
//it neater to code C++ 'for' loops to access records/values/etc. set using a cursor.
//*****************************************************************************************

#if !defined(BB_HANDLES)
#define BB_HANDLES

#include <string>

namespace dpt {

//Forward decs
class SortRecord;
class SortRecordSet;
class RecordSet;
class RecordSetCursor;
class ReadableRecord;
class Record;
class SingleDatabaseFileContext;
class DatabaseFileContext;
class FieldAttributeCursor;
class FieldAttributes;
class DBCursor;

typedef short FieldID;

//**************************************************************************************
//Set handles help ensure correct release of the set.
//**************************************************************************************
class RecordSetHandle {
	RecordSet* set;
	DatabaseFileContext* context;

	friend class RecordSet;
	bool set_deleted;

public:
	RecordSetHandle(RecordSet* s);
	~RecordSetHandle() {DestroySet();}
	void DestroySet();

	//The new owner releases the set now.  This is handy for returning a set handle
	//from, say, a "find" function if returning the set itself is not convenient.
	void operator=(RecordSetHandle& rhs);
	RecordSetHandle(RecordSetHandle& rhs) {*this = rhs;}

	RecordSet* GetSet() {return set;}
};



//**************************************************************************************
//Cursor handles - handy for neater loops
//**************************************************************************************
class DBCursorHandle {
protected:
	friend class DBCursor;
	bool cursor_deleted;
	void NotifyCursor(DBCursor*);
public:
	DBCursorHandle() : cursor_deleted(false) {}
};

//*****************************************************
class RecordSetCursorHandle : public DBCursorHandle {
protected:
	RecordSet* set;
	RecordSetCursor* cursor;

	RecordSetCursorHandle(RecordSetCursor*);

public:
	RecordSetCursorHandle(RecordSet*, bool gotofirst = true);
	virtual ~RecordSetCursorHandle() {CloseCursor();}

	//Used by UL evaluator - not necessarily ideal for general callers
	RecordSetCursor* GetLiveCursor() {if (cursor_deleted) return NULL; return cursor;}
	static RecordSetCursorHandle* CreateCloneHandle(RecordSetCursor*);
	void ImportPosition(RecordSetCursor*);

	void CloseCursor();

	void Advance(int);
	bool Accessible();
	bool CanEnterLoop() {return Accessible();}
	void GotoFirst();
	void GotoLast();

	int LastAdvancedRecNum();
	SingleDatabaseFileContext* LastAdvancedFileContext();

	//Separate functions to clarify what's happening with sorted sets
	ReadableRecord* AccessCurrentRecordForRead();
	Record* AccessCurrentRecordForReadWrite();
	Record* AccessCurrentRealRecord() {return AccessCurrentRecordForReadWrite();}
};

//*****************************************************
class FieldAttributeCursorHandle : public DBCursorHandle {
	DatabaseFileContext* context;
	FieldAttributeCursor* cursor;
public:
	FieldAttributeCursorHandle(DatabaseFileContext*, bool gotofirst = true);
	~FieldAttributeCursorHandle() {CloseCursor();}
	void CloseCursor();

	void Advance(int);
	bool Accessible();
	bool CanEnterLoop() {return Accessible();}
	void GotoFirst();
	void GotoLast();

	int NumFields();

	const std::string* Name();
	const FieldAttributes* Atts();
	const FieldID* FID();
};

} //close namespace

#endif
