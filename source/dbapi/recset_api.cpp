
#include "stdafx.h"

#include "dbapi\recset.h"
#include "dbapi\dbctxt.h"
#include "recset.h"
#include "dbctxt.h"

namespace dpt {

APIRecordSetCursor::APIRecordSetCursor(RecordSetCursor* t) 
: APICursor(t), target(t) {}
APIRecordSetCursor::APIRecordSetCursor(const APIRecordSetCursor& t) 
: APICursor(t), target(t.target) {}

//************************************************
int APIRecordSetCursor::LastAdvancedRecNum()
{
	return target->LastAdvancedRecNum();
}

APIDatabaseFileContext APIRecordSetCursor::LastAdvancedFileContext()
{
//	api = target->LastAdvancedFileContext();
	return APIDatabaseFileContext(target->LastAdvancedFileContext());
}

//************************************************
APIReadableRecord APIRecordSetCursor::AccessCurrentRecordForRead()
{
	return APIReadableRecord(target->AccessCurrentRecordForRead());
}

APIRecord APIRecordSetCursor::AccessCurrentRealRecord()
{
	return APIRecord(target->AccessCurrentRealRecord());
}

//************************************************
APIRecordSetCursor APIRecordSetCursor::CreateClone()
{
	return APIRecordSetCursor(target->CreateClone());
}

void APIRecordSetCursor::ExportPosition(APIRecordSetCursor& dest)
{
	target->ExportPosition(dest.target);
}


//***************************************************************************************
void APIRecordSet::Clear()
{
	target->Clear();
}

int APIRecordSet::Count() const
{
	return target->Count();
}

//************************************************
APIRecordSetCursor APIRecordSet::OpenCursor()
{
	//Can't see why they would ever want to not goto first when opening
	bool gotofirst = true;
	return APIRecordSetCursor(target->OpenCursor(gotofirst));
}

void APIRecordSet::CloseCursor(const APIRecordSetCursor& c)
{
	target->CloseCursor(c.target);
}

//************************************************
int* APIRecordSet::GetRecordNumberArray(int* dest, int getmaxrecs)
{
	return target->GetRecordNumberArray(dest, getmaxrecs);
}

APIRecord APIRecordSet::AccessRandomRecord(int recnum)
{
	return APIRecord(target->AccessRandomRecord(recnum));
}

} //close namespace


