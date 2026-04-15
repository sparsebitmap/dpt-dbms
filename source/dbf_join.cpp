
#include "stdafx.h"
#include "dbf_join.h"

//Utils
//API tiers
//Diagnostics

namespace dpt {

ValueSetToBTreeJoinOperation::ValueSetToBTreeJoinOperation(ValueSet* vi) 
: input_set(vi), 
precalc_done(false)
{}

//********************************************************************************
void ValueSetToBTreeJoinOperation::PreCalculate()
{
	if (precalc_done)
			return;

	//blah

	precalc_done = true;
}

//********************************************************************************
ValueSet* ValueSetToBTreeJoinOperation::Perform()
{
	if (!precalc_done)
		PreCalculate();

	if (merge_chosen)
		return PerformMerge();
	else
		return PerformLoop();
}

//********************************************************************************
ValueSet* ValueSetToBTreeJoinOperation::PerformMerge()
{
	return NULL;
}

//********************************************************************************
ValueSet* ValueSetToBTreeJoinOperation::PerformLoop()
{
	return NULL;
}

} //close namespace


