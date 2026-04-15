
#include "stdafx.h"

#include "dbapi\record.h"
#include "record.h"

namespace dpt {

APIRecord::APIRecord(Record* t) : APIReadableRecord(t), target(t) {}
APIRecord::APIRecord(const APIRecord& t) : APIReadableRecord(t), target(t.target) {}

//***************************************
int APIRecord::AddField(const std::string& fname, const APIFieldValue& val)
{
	return target->AddField(fname, *(val.target));
}

int APIRecord::InsertField
(const std::string& fname, const APIFieldValue& val, const int newocc)
{
	return target->InsertField(fname, *(val.target), newocc);
}


//***************************************
int APIRecord::ChangeField
(const std::string& fname, const APIFieldValue& newval, const int occ, APIFieldValue* povapi)
{
	FieldValue* pov = NULL;
	if (povapi)
		pov = povapi->target;

	return target->ChangeField(fname, *(newval.target), occ, pov);
}

int APIRecord::ChangeFieldByValue
(const std::string& fname, const APIFieldValue& newval, const APIFieldValue& oldval)
{
	return target->ChangeFieldByValue(fname, *(newval.target), *(oldval.target));
}


//***************************************
int APIRecord::DeleteField(const std::string& fname, const int occ, APIFieldValue* povapi)
{
	FieldValue* pov = NULL;
	if (povapi)
		pov = povapi->target;

	return target->DeleteField(fname, occ, pov);
}

int APIRecord::DeleteFieldByValue(const std::string& fname, const APIFieldValue& oldval)
{
	return target->DeleteFieldByValue(fname, *(oldval.target));
}


//***************************************
int APIRecord::DeleteEachOccurrence(const std::string& fname)
{
	return target->DeleteEachOccurrence(fname);
}


//***************************************
void APIRecord::Delete()
{
	target->Delete();
}


//***************************************
std::string APIRecord::ShowPhysicalInformation()
{
	return target->ShowPhysicalInformation();
}

//***************************************
bool APIRecord::GetNextFVPairAndOrBLOBDescriptor
(std::string& fname, APIFieldValue* pval, APIFieldValue* pdesc, int& fvpix) const
{
	return target->GetNextFVPairAndOrBLOBDescriptor(fname, pval->target, pdesc->target, fvpix);
}

//***************************************
bool APIRecord::GetBLOBDescriptor(const std::string& fname, APIFieldValue& fval, int occ) const
{
	return GetBLOBDescriptor(fname, fval, occ);
}

} //close namespace


