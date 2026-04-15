
#include "stdafx.h"

#include "dbf_index.h"

//Utils
//API tiers
#include "cfr.h" //#include "CFR.h" : V2.24 case is less interchangeable on *NIX - Roger M.
#include "dbctxt.h"
#include "dbfile.h"
#include "valset.h"
#include "valdirect.h"
//Diagnostics
#include "except.h"
#include "msg_db.h"

namespace dpt {

//****************************************************************************************
//Value set work
//****************************************************************************************
unsigned int DatabaseFileIndexManager::FindOrCountValues
(SingleDatabaseFileContext* sfc, ValueSet* valset, const FindValuesSpecification& spec)
{
	DirectValueCursor dvc(sfc, spec);

	if (valset) {
		if (dvc.GetFieldAtts()->IsOrdNum())
			valset->InitializeSetFromBtree(SORT_NUMERIC);
		else
			valset->InitializeSetFromBtree(SORT_CHARACTER);
	}

	//If the set is currently empty we can save a merge below
	bool needs_merge = true;
	std::vector<FieldValue> temparray;
	if (valset)
		if (valset->Count() == 0)
			needs_merge = false;

	//Save repeated lock/unlock (but it will happen periodically - see DVC code)
	CFRSentry s;
	dvc.PreLockIndex(&s);

	unsigned int numvals = 0;
	for (dvc.GotoFirst(); dvc.CanEnterLoop(); dvc.Advance()) {

		numvals++;
		if (numvals == UINT_MAX)
			throw Exception(BUG_MISC, 
				"Bug: Too many values for current Find/CountValues algorithm");

		//If just counting, no need to get the actual value
		if (!valset)
			continue;

		FieldValue* pv;

		//Load the value set directly if possible to save a FieldValue copy.
		if (!needs_merge)
			pv = valset->NewBackMember();

		else {
			temparray.push_back(FieldValue());
			pv = &(temparray.back());
		}

		dvc.GetCurrentValue(*pv);
	}

	//Got all the values for this group member
	if (numvals == 0)
		return 0;

	//Note:  It is assumed that in the vast majority of cases the calling code will
	//want the values in the order they are in the indexes (i.e. character or numeric),
	//so we do a merge as each member's values appear, then if an FRV requires it we
	//sort the whole lot later.  A possible optimization on the final sort would be to 
	//re-sort each subset *before* merging, but it would greatly complicate the compiler 
	//and would in any case not guarantee a saving (what if all files have lots of shared
	//values - then we would end up re-sorting several similar large sets instead of
	//doing a feww merges and then sorting one somewhat larger set).
	if (needs_merge) {

		int premerge_count = valset->Count();

		valset->Merge(&temparray);

		//MRGVALS is a file and system level stat.  At system level it means the total 
		//number of values from all members, including dupes.
		if (valset->num_group_value_sets == 1)
			valset->premerge_sfc->GetDBFile()->AddToStatMRGVALS(sfc->DBAPI(), premerge_count);

		valset->num_group_value_sets++;
		sfc->GetDBFile()->AddToStatMRGVALS(sfc->DBAPI(), temparray.size());
	}
	else {
		valset->num_group_value_sets = 1;
		valset->premerge_sfc = sfc;
	}

	return numvals;
}



} //close namespace


