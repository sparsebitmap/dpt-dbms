
#include "stdafx.h"

#include "dbapi\sortspec.h"
#include "sortspec.h"

namespace dpt {

APISortRecordsSpecification::APISortRecordsSpecification(int r, bool k)
: target(new SortRecordsSpecification(r, k))
{
	target->refcount = 1;
}

APISortRecordsSpecification::APISortRecordsSpecification
(const APISortRecordsSpecification& from)
{
	target = from.target;
	if (target)
		target->refcount++;
}

APISortRecordsSpecification::~APISortRecordsSpecification()
{
	if (target) {
		target->refcount--;
		if (target->refcount == 0)
			delete target;
	}
}

//*********************************************
void APISortRecordsSpecification::AddKeyField
(const std::string& fieldname, const SortDirection dir, const SortType type, const bool each)
{
	target->AddKey(fieldname, dir, type, each);
}

void APISortRecordsSpecification::AddDataField
(const std::string& fieldname, const bool collect_all_occs)
{
	target->AddData(fieldname, collect_all_occs);
}


void APISortRecordsSpecification::SetOptionSortKeysOnly()
{
	target->SetOptionSortKeysOnly();
}

void APISortRecordsSpecification::SetOptionCollectAllFields()
{
	target->SetOptionCollectAllFields();
}



} //close namespace


