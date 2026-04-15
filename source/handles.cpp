
#include "stdafx.h"

#include "handles.h"

//Utils
//API Tiers
#include "recset.h"
#include "sortset.h"
#include "dbctxt.h"
#include "fieldinfo.h"
//Diagnostics

namespace dpt {

//**************************************************************************************
RecordSetHandle::RecordSetHandle(RecordSet* s) 
: set(s), context(s->Context()), set_deleted(false)
{
	set->handle = this;
}

//**************************************************************************************
void RecordSetHandle::DestroySet()
{
	if (!set_deleted)
		context->DestroyRecordSet(set);
	set_deleted = true;
}

//**************************************************************************************
void RecordSetHandle::operator=(RecordSetHandle& rhs)
{
	set = rhs.set;
	context = rhs.context;
	set_deleted = rhs.set_deleted;

	rhs.set = NULL;
	rhs.context = NULL;
}





//**************************************************************************************
//**************************************************************************************
//Cursor handles
//**************************************************************************************
//**************************************************************************************
void DBCursorHandle::NotifyCursor(DBCursor* c) 
{
	c->handle = this;
}

//**************************************************************************************
RecordSetCursorHandle::RecordSetCursorHandle(RecordSet* s, bool gotofirst) 
: set(s), cursor(NULL) 
{
	cursor = set->OpenCursor(gotofirst);
	NotifyCursor(cursor);
}

//********************
RecordSetCursorHandle::RecordSetCursorHandle(RecordSetCursor* source)
: set(source->Set()), cursor(source)
{ /* only used by the cloner below */ }

//********************
RecordSetCursorHandle* RecordSetCursorHandle::CreateCloneHandle(RecordSetCursor* source)
{
	RecordSetCursorHandle* result = NULL;
	RecordSetCursor* c = source->CreateClone();
	try {
		result = new RecordSetCursorHandle(c);
		result->NotifyCursor(c);
		return result;
	}
	catch (...) {
		if (result)
			delete result;
		else
			c->Set()->CloseCursor(c);
		throw;
	}
}

//********************
void RecordSetCursorHandle::ImportPosition(RecordSetCursor* source)
{
	source->ExportPosition(cursor);
}

//********************
void RecordSetCursorHandle::CloseCursor()
{
	if (cursor && !cursor_deleted)
		set->CloseCursor(cursor);
	cursor_deleted = true;
}

//********************
void RecordSetCursorHandle::Advance(int n)
{
	if (cursor && !cursor_deleted)
		cursor->Advance(n);
}

//********************
bool RecordSetCursorHandle::Accessible()
{
	if (cursor && !cursor_deleted)
		return cursor->Accessible();
	return false;
}

//********************
void RecordSetCursorHandle::GotoFirst()
{
	if (cursor && !cursor_deleted)
		cursor->GotoFirst();
}

//********************
void RecordSetCursorHandle::GotoLast()
{
	if (cursor && !cursor_deleted)
		cursor->GotoLast();
}

//********************
ReadableRecord* RecordSetCursorHandle::AccessCurrentRecordForRead()
{
	if (cursor && !cursor_deleted)
		return cursor->AccessCurrentRecordForRead();
	return NULL;
}

//********************
Record* RecordSetCursorHandle::AccessCurrentRecordForReadWrite()
{
	if (cursor && !cursor_deleted)
		return cursor->AccessCurrentRecordForReadWrite();
	return NULL;
}

//********************
int RecordSetCursorHandle::LastAdvancedRecNum()
{
	if (cursor && !cursor_deleted)
		return cursor->LastAdvancedRecNum();
	return -1;
}

//********************
SingleDatabaseFileContext* RecordSetCursorHandle::LastAdvancedFileContext()
{
	if (cursor && !cursor_deleted)
		return cursor->LastAdvancedFileContext();
	return NULL;
}

//**************************************************************************************
//**************************************************************************************
FieldAttributeCursorHandle::FieldAttributeCursorHandle(DatabaseFileContext* c, bool gotofirst)
: context(c), cursor(NULL)
{
	if (cursor && !cursor_deleted) {
		context->CloseFieldAttCursor(cursor);
		cursor = NULL;
	}
	cursor = c->OpenFieldAttCursor(gotofirst);
	NotifyCursor(cursor);
}

//**************************************************************************************
void FieldAttributeCursorHandle::CloseCursor()
{
	if (cursor && !cursor_deleted)
		context->CloseFieldAttCursor(cursor);
	cursor_deleted = true;
}

//**************************************************************************************
int FieldAttributeCursorHandle::NumFields()
{
	if (cursor && !cursor_deleted)
		return cursor->NumFields();
	return 0;
}

//**************************************************************************************
void FieldAttributeCursorHandle::GotoFirst()
{
	if (cursor && !cursor_deleted)
		cursor->GotoFirst();
}

//**************************************************************************************
void FieldAttributeCursorHandle::GotoLast()
{
	if (cursor && !cursor_deleted)
		cursor->GotoLast();
}

//**************************************************************************************
void FieldAttributeCursorHandle::Advance(int n)
{
	if (cursor && !cursor_deleted)
		cursor->Advance(n);
}

//**************************************************************************************
bool FieldAttributeCursorHandle::Accessible()
{
	if (cursor && !cursor_deleted)
		return cursor->Accessible();
	else
		return false;
}

//**************************************************************************************
const std::string* FieldAttributeCursorHandle::Name()
{
	if (cursor && !cursor_deleted)
		return cursor->Name();
	else
		return NULL;
}

//**************************************************************************************
const FieldAttributes* FieldAttributeCursorHandle::Atts()
{
	if (cursor && !cursor_deleted)
		return cursor->Atts();
	else
		return NULL;
}

//**************************************************************************************
const FieldID* FieldAttributeCursorHandle::FID()
{
	if (cursor && !cursor_deleted)
		return cursor->FID();
	else
		return NULL;
}





} //close namespace
