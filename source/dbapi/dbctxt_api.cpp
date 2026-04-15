
#include "stdafx.h"

#include "dbapi\dbctxt.h"
#include "dbapi\reccopy.h"

#include "dbctxt.h"
#include "valdirect.h"
#include "findspec.h"
#include "reccopy.h"

namespace dpt {

//*******************************************
std::string APIDatabaseFileContext::GetShortName() const
{
	return target->GetShortName();
}

std::string APIDatabaseFileContext::GetFullName() const
{
	return target->GetFullName();
}

std::string APIDatabaseFileContext::GetFullFilePath() const
{
	return target->GetFullFilePath();
}

bool APIDatabaseFileContext::IsGroupContext() const
{
	return (target->CastToGroup()) ? true : false;
}

//*******************************************
void APIDatabaseFileContext::Initialize(bool leave_fields)
{
	target->Initialize(leave_fields);
}

void APIDatabaseFileContext::Increase(int qty, bool tabled)
{
	target->Increase(qty, tabled);
}

void APIDatabaseFileContext::ShowTableExtents(std::vector<int>* result)
{
	target->ShowTableExtents(result);
}

unsigned APIDatabaseFileContext::ApplyDeferredUpdates(int forgiveness, int* hiword)
{
	_int64 i64 = target->ApplyDeferredUpdates(forgiveness);

	unsigned int result = i64;

	//Allow API defined with no 64 bit integers
	if (hiword) {
		i64 >>= 32;
		*hiword = i64;
	}

	return result;
}

//*******************************************
//V3.0
void APIDatabaseFileContext::Unload
(const FastUnloadOptions& opts, const BitMappedRecordSet* baseset, 
 std::vector<std::string>* fnames, const std::string& dir)
{
	target->Unload(opts, baseset, fnames, dir);
}

//*******************************************
void APIDatabaseFileContext::Load
(const FastLoadOptions& opts, int eyeball, 
 util::LineOutput* eyeball_altdest, const std::string& dir)
{
	target->Load(opts, eyeball, eyeball_altdest, dir);
}



//*******************************************
void APIDatabaseFileContext::DefineField
(const std::string& fname, const APIFieldAttributes& atts)
{
	//V3.0
//	target->DefineField(fname, atts.IsFloat(), atts.IsInvisible(), atts.IsUpdateAtEnd(), 
//		atts.IsOrdered(), atts.IsOrdNum(), atts.Splitpct(), atts.IsNoMerge());
	target->DefineField(fname, atts.IsFloat(), atts.IsInvisible(), atts.IsUpdateAtEnd(), 
		atts.IsOrdered(), atts.IsOrdNum(), atts.Splitpct(), atts.IsNoMerge(), atts.IsBLOB());
}

void APIDatabaseFileContext::RedefineField
(const std::string& fname, const APIFieldAttributes& newatts)
{
	target->RedefineField(fname, *(newatts.target));
}

void APIDatabaseFileContext::RenameField(const std::string& from, const std::string& to)
{
	target->RenameField(from, to);
}

void APIDatabaseFileContext::DeleteField(const std::string& fname)
{
	target->DeleteField(fname);
}


//*******************************************
APIFieldAttributes APIDatabaseFileContext::GetFieldAtts(const std::string& fname)
{
	FieldAttributes temp = target->GetFieldAtts(fname);
	return APIFieldAttributes(&temp);
}

APIFieldAttributeCursor APIDatabaseFileContext::OpenFieldAttCursor()
{
	//As per record set
	bool gotofirst = true;
	return APIFieldAttributeCursor(target->OpenFieldAttCursor(gotofirst));
}

void APIDatabaseFileContext::CloseFieldAttCursor(const APIFieldAttributeCursor& c)
{
	target->CloseFieldAttCursor(c.target);
}

//*******************************************
int APIDatabaseFileContext::StoreRecord(const APIStoreRecordTemplate& r)
{
	StoreRecordTemplate* temp = static_cast<StoreRecordTemplate*>(r.target);
	return target->StoreRecord(*temp);
}

//*************************************************************************************
//Finds
//*************************************************************************************
//V2.13 Dec 08. These to handle the set of explicit functions rather than the old single
//one with default parameters.
APIFoundSet APIDatabaseFileContext::FindRecords()
{
	return APIFoundSet(target->FindRecords());
}

APIFoundSet APIDatabaseFileContext::FindRecords
(const APIFindSpecification& spec)
{
	return APIFoundSet(target->FindRecords(*spec.target));
}

APIFoundSet APIDatabaseFileContext::FindRecords
(const APIFindSpecification& spec, FindEnqueueType lk)
{
	return APIFoundSet(target->FindRecords(*spec.target, lk));
}

APIFoundSet APIDatabaseFileContext::FindRecords
(const APIFindSpecification& spec, const APIBitMappedRecordSet& referback)
{
	return APIFoundSet(target->FindRecords(*spec.target, referback.target));
}

APIFoundSet APIDatabaseFileContext::FindRecords
(const APIFindSpecification& spec, FindEnqueueType lk, const APIBitMappedRecordSet& referback)
{
	return APIFoundSet(target->FindRecords(spec.target, lk, referback.target));
}

APIFoundSet APIDatabaseFileContext::FindRecords
(const std::string& fname, FindOperator optr, const APIFieldValue& fval)
{
	FindSpecification spec(fname, optr, *(fval.target));
	return APIFoundSet(target->FindRecords(spec));
}

//*************************************************************************************
APIRecordList APIDatabaseFileContext::CreateRecordList()
{
	return APIRecordList(target->CreateRecordList());
}


//*******************************************
void APIDatabaseFileContext::DestroyRecordSet(const APIRecordSet& s)
{
	target->DestroyRecordSet(s.target);
}

void APIDatabaseFileContext::DestroyAllRecordSets()
{
	target->DestroyAllRecordSets();
}



//*******************************************
APIValueSet APIDatabaseFileContext::FindValues(const APIFindValuesSpecification& fspec)
{
	return APIValueSet(target->FindValues(*(fspec.target)));
}

unsigned int APIDatabaseFileContext::CountValues(const APIFindValuesSpecification& fspec)
{
	return target->CountValues(*(fspec.target));
}

void APIDatabaseFileContext::FileRecordsUnder
(const APIBitMappedRecordSet& set, const std::string& fname, const APIFieldValue& val)
{
	target->FileRecordsUnder(set.target, fname, *(val.target));
}

APIValueSet APIDatabaseFileContext::CreateEmptyValueSet()
{
	return APIValueSet(target->CreateEmptyValueSet());
}

void APIDatabaseFileContext::DestroyValueSet(const APIValueSet& s)
{
	target->DestroyValueSet(s.target);
}

void APIDatabaseFileContext::DestroyAllValueSets()
{
	target->DestroyAllValueSets();
}


//*******************************************
void InitializeDVC(DatabaseFileContext* f, DirectValueCursor* c, CursorDirection dir)
{
	try {
		c->SetDirection(dir);
		c->GotoFirst();
	}
	catch (...) {
		f->CloseDirectValueCursor(c);
		throw;
	}
}

APIDirectValueCursor APIDatabaseFileContext::OpenDirectValueCursor
(const APIFindValuesSpecification& fspec, CursorDirection dir)
{
	//For some reason the creation doesn't start the loop, so do it now
	DirectValueCursor* c = target->OpenDirectValueCursor(*(fspec.target));
	InitializeDVC(target, c, dir);
	return APIDirectValueCursor(c);
}

void APIDatabaseFileContext::CloseDirectValueCursor(const APIDirectValueCursor& c)
{
	target->CloseDirectValueCursor(c.target);
}

//*******************************************
void APIDatabaseFileContext::DirtyDeleteRecords(const APIBitMappedRecordSet& s)
{
	target->DirtyDeleteRecords(s.target);
}

} //close namespace


