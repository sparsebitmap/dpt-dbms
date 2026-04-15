
#include "stdafx.h"

#include "dbapi\reccopy.h"
#include "reccopy.h"

namespace dpt {

APIRecordCopy::APIRecordCopy()
: APIReadableRecord(NULL)
{
	target = new RecordCopy();
	target->refcount = 1;
	APIReadableRecord::target = target;
}

APIRecordCopy::APIRecordCopy(RecordCopy* t)
: APIReadableRecord(t)
{
	//Fresh after construction by the infrastructure
	target = t;
	if (target)
		target->refcount = 1;
}

APIRecordCopy::APIRecordCopy(const APIRecordCopy& from)
: APIReadableRecord(from)
{
	//Assignment or or copy constructor
	target = from.target;
	if (target)
		target->refcount++;
}

APIRecordCopy::~APIRecordCopy()
{
	if (target) {
		target->refcount--;
		if (target->refcount == 0)
			delete target;
	}
}

//**************************************************
void APIRecordCopy::Clear()
{
	target->Clear();
}

int APIRecordCopy::NumFVPairs() const
{
	return target->NumFVPairs();
}


//***************************************************************************************
//V2.18 May 09.  New class.
APIStoreRecordTemplate::APIStoreRecordTemplate()
: APIRecordCopy(new StoreRecordTemplate())
{}

//**************************************************
void APIStoreRecordTemplate::Append(const std::string& fname, const APIFieldValue& val)
{
	static_cast<StoreRecordTemplate*>(target)->Append(fname, *(val.target));
}

//***************************************
void APIStoreRecordTemplate::ClearFieldNames(int leave_number)
{
	static_cast<StoreRecordTemplate*>(target)->ClearFieldNames(leave_number);
}

void APIStoreRecordTemplate::AppendFieldName(const std::string& s)
{
	static_cast<StoreRecordTemplate*>(target)->AppendFieldName(s);
}

void APIStoreRecordTemplate::SetFieldName(int fix, const std::string& name)
{
	static_cast<StoreRecordTemplate*>(target)->SetFieldName(fix, name);
}


//***************************************
void APIStoreRecordTemplate::ClearFieldValues(int leave_number)
{
	static_cast<StoreRecordTemplate*>(target)->ClearFieldValues(leave_number);
}

void APIStoreRecordTemplate::AppendFieldValue(const APIFieldValue& val)
{
	static_cast<StoreRecordTemplate*>(target)->AppendFieldValue(*(val.target));
}

void APIStoreRecordTemplate::SetFieldValue(int fix, const APIFieldValue& val)
{
	static_cast<StoreRecordTemplate*>(target)->SetFieldValue(fix, *(val.target));
}

void APIStoreRecordTemplate::Clear()
{
	static_cast<StoreRecordTemplate*>(target)->Clear();
}

} //close namespace


