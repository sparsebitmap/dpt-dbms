//***************************************************************************************
//Found sets, record lists and sorted record sets
//***************************************************************************************

#if !defined(BB_API_RECSET)
#define BB_API_RECSET

#include "cursor.h"
#include "record.h"

namespace dpt {

class RecordSetCursor;
class APIDatabaseFileContext;

class APIRecordSetCursor : public APICursor {
public:
	RecordSetCursor* target;
	APIRecordSetCursor(RecordSetCursor*);
	APIRecordSetCursor(const APIRecordSetCursor&);
	//-----------------------------------------------------------------------------------

	//Quicker than going to the table B page if recnum/file is all that's required
	int LastAdvancedRecNum();
	APIDatabaseFileContext LastAdvancedFileContext();

	//Separate functions to allow a common interface for sorted and unsorted sets.
	APIReadableRecord AccessCurrentRecordForRead();
	APIRecord AccessCurrentRecordForReadWrite() {return AccessCurrentRealRecord();}
	APIRecord AccessCurrentRealRecord();

	//These are used by the UL REMEMBER statement and may perhaps be generally useful.
	APIRecordSetCursor CreateClone();
	void ExportPosition(APIRecordSetCursor& dest);
};

//*****************************************
class RecordSet;

class APIRecordSet {
public:
	RecordSet* target;
	APIRecordSet(RecordSet* t) : target(t) {}
	APIRecordSet(const APIRecordSet& t) : target(t.target) {}
	//-----------------------------------------------------------------------------------
	
	//Releases all storage and locks, but doesn't delete.  Like RELEASE RECORDS in UL.
	void Clear();

	int Count() const;

	APIRecordSetCursor OpenCursor();
	void CloseCursor(const APIRecordSetCursor&);

	//V3.0.  Can make scrolling GUIs easier to build (cursors are not ideal with large sets
	//which allow random-access scrolling).
	int* GetRecordNumberArray(int* dest = NULL, int getmaxrecs = -1);
	APIRecord AccessRandomRecord(int recnum);
};

} //close namespace

#endif
