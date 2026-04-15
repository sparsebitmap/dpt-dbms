
#include "stdafx.h"

#include "recset.h"

//Utils
#include "record.h"
#include "handles.h"
//API Tiers
#include "dbctxt.h"
//Diagnostics
#include "except.h"
#include "msg_file.h"

namespace dpt {

//***************************************************************************************
RecordSet::~RecordSet()
{
	if (handle)
		handle->set_deleted = true;

	if (random_access_record)
		delete random_access_record;
}

//************************************************
//V3.0
int* RecordSet::GetRecordNumberArray(int* dest, int getmaxrecs)
{
	if (context->CastToGroup())
		throw Exception(CONTEXT_SINGLE_FILE_ONLY, "GetRecordNumberArray() is invalid in group context");

	bool allocated_here = false;

	if (dest == NULL) {
		allocated_here = true;

		if (getmaxrecs == -1)
			getmaxrecs = Count();
	
		dest = new int[getmaxrecs];
	}

	try {
		if (getmaxrecs != 0)
			GetRecordNumberArray_D(dest, getmaxrecs);
		return dest;
	}
	catch (...) {
		if (allocated_here)
			delete[] dest;
		throw;
	}
}

//************************************************
//V3.0
Record* RecordSet::AccessRandomRecord(int recnum)
{
	if (context->CastToGroup()) {
		throw Exception(CONTEXT_SINGLE_FILE_ONLY, 
			"AccessRandomRecord() is invalid in group context (but you could try a FD_SINGLEREC find)");
	}

	//The salient parts of the cursor function below
	bool ddflag = AffectedByDirtyDelete();

	if (random_access_record) {
		delete random_access_record;
		random_access_record = NULL;
	}

	random_access_record = new Record(context->CastToSingle(), recnum, false, ddflag, context);
	return random_access_record;
}

//***************************************************************************************
//The creation of a record MROs is left until this function is called, and doesn't 
//happen as the cursor is just moved around.
//***************************************************************************************
Record* RecordSetCursor::AccessCurrentRealRecord_B(bool noregister)
{
	int nextrn = LastAdvancedRecNum();
	SingleDatabaseFileContext* nextctxt = LastAdvancedFileContext();

	//The cursor has gone past the end of the set, or the set has been released.
	if (!Accessible())
		return NULL;

	bool ddflag = Set()->AffectedByDirtyDelete();

	//Accessing the same record twice in a row is something I imagine either API coders
	//(or even me writing the UL evaluator) will do inadvertently, and there's no reason
	//not to make a special case for it - just return the same record MRO.
	if (prevrealmro) {
		if (nextrn == prevrealmro->RecNum() 
			&& nextctxt == prevrealmro->HomeFileContext()
			&& !ddflag)
				return prevrealmro;
	}

	ForgetPrevRealRec();

	//V2.10: May 07.  Found a new safe use for the noregister flag!  In DBA operations
	//(e.g. REDEFINE, UNLOAD) we know there is either a record lock or CFR_DIRECT EXCL lock.
	prevrealmro = new Record(nextctxt, nextrn, noregister, ddflag, Set()->Context());
//	prevrealmro = new Record(nextctxt, nextrn, false, ddflag, Set()->Context());

	return prevrealmro;
}

//***************************************************************************************
void RecordSetCursor::ForgetPrevRealRec()
{
	if (prevrealmro) {
		delete prevrealmro;
		prevrealmro = NULL;
	}
}

} //close namespace


