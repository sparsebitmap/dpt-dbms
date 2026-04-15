#include "stdafx.h"

#include "sortspec.h"

//Utils
//Diagnostics
#include "except.h"
#include "msg_db.h"

namespace dpt {

//*************************************************************************************
void SortRecordsSpecification::AddKey
(const std::string& fieldname, const SortDirection dir, const SortType type, const bool each)
{
	//Get a vector iterator for the field
	std::vector<SortRecordsFieldSpec>::iterator i;
	for (i = fields.begin(); i != fields.end(); i++) {
		if (fieldname == i->name)
			break;
	}

	//Field not present at all yet
	if (i == fields.end()) {
		//NB. sort by each means we have to collect each too
		fields.push_back(SortRecordsFieldSpec(fieldname, type, dir, each, each));
		return;
	}

	//Already present as a key field
	if (i->key_type != SORT_NONE)
		throw Exception(DML_BAD_SORT_SPEC, "Duplicate sort key");

	bool nonkey_all_occs = i->collect_all_occs;

	//Already present as a non-key field, so make it a key.  It will often be the case
	//that the caller defines non-key items first and then one or more keys afterwards.
	//This is what the UL compiler does in fact, because the non-keys are known at compile
	//time but the keys only at run time.  Since the order of keys in the spec determines
	//their priority, we remove the non-key item and then append it again as a key.  This
	//does not affect the order of fv pairs appearing in the sorted records (that depends
	//on the order in the original record).  vector::erase is normally something to be
	//avoided but it's nice and simple here, and is a minor overhead compared to the sort,
	//especially when you consider this will often not apply.
	fields.erase(i);
	AddKey(fieldname, dir, type, each);

	//V3.0.  Nancy Stevens' bug here.  Preserve the "all occs" status of the non-key.
	fields.back().collect_all_occs = nonkey_all_occs;
}

//*************************************************************************************
void SortRecordsSpecification::AddData(const std::string& fieldname, const bool all_occs)
{
	//Once these options are set we don't have to collect any non-key fields
	if (collect_all_fields || sort_keys_only)
		return;

	int ix = -1;
	for (size_t n = 0; n < fields.size(); n++) {
		if (fieldname == fields[n].name) {
			ix = n;
			break;
		}
	}

	//Field did not exist before
	if (ix == -1) {
		fields.push_back(SortRecordsFieldSpec(fieldname, SORT_NONE, false, false, all_occs));
	}

	//It did exist before, either as a key or non-key
	else {
		//We can change our mind from single-occ to all but not back - this saves the
		//UL compier having to query this flag.
		if (all_occs)
			fields[ix].collect_all_occs = true;
	}
}

//*************************************************************************************
void SortRecordsSpecification::ClearNonKeys()
{
	std::vector<SortRecordsFieldSpec> temp;

	for (size_t n = 0; n < fields.size(); n++)
		if (fields[n].key_type != SORT_NONE)
			temp.push_back(fields[n]);

	fields = temp;
}

//*************************************************************************************
void SortRecordsSpecification::SetOptionCollectAllFields()
{
	if (collect_all_fields) 
		return; 
	collect_all_fields = true;

	if (!sort_keys_only)
		ClearNonKeys();
}

//*************************************************************************************
void SortRecordsSpecification::SetOptionSortKeysOnly()
{
	if (sort_keys_only) 
		return; 
	sort_keys_only = true;

	if (!collect_all_fields)
		ClearNonKeys();
}

} //close namespace


