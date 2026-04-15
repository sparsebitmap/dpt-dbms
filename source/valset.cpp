
#include "stdafx.h"

#include "valset.h"

#include "dbctxt.h"
#include "dbserv.h"

#include "except.h"
#include "msg_db.h"

#include <algorithm>

namespace dpt {

//***************************************************************************************
void ValueSet::Sort(SortType requested_sorttype, SortDirection requested_sortdir)
{
	//This is meaningless with value sets, as they don't have field info as such
	if (requested_sorttype == SORT_DEFAULT_TYPE || requested_sorttype == SORT_NONE)
		return;

	//Work can be saved in some cases if it's already sorted
	if (sorted) {
		if (requested_sorttype == current_sorttype) {
			if (requested_sortdir != current_sortdir) {
				ReverseOrder();
				current_sortdir = requested_sortdir;
			}
			return;
		}
	}

	//* * * To check on M204 * * *
	//The following convention causes somewhat unexpected results when you sort a
	//set of strings in numerical order - invalids appear in the set as zero rather
	//than appearing as themselves but just sorted as if they were zero.  To fix this
	//clearly we can create and sort a parallel set with pointers to the original set,
	//then reorder the original set after sorting the parallel set.
	//* * * To check on M204 * * *

	//Actually convert the data once first, otherwise it would be ridiculous
	if (requested_sorttype == SORT_NUMERIC) {
		for (size_t x = 0; x < data->size(); x++)
			(*data)[x].ConvertToNumeric();
	}
	//All character sort types work on string format FieldValue objects
	else if (current_sorttype == SORT_NUMERIC) {
		for (size_t x = 0; x < data->size(); x++)
			(*data)[x].ConvertToString();
	}

	//Select the appropriate comparison algorithm, and sort
	FieldValueLessThanPredicate pred(requested_sorttype, requested_sortdir);

	Context()->DBAPI()->IncStatSORTS();

	//Not held at file level (in a group with dupe values who could say which file?)
	Context()->DBAPI()->AddToStatSTVALS(data->size());

	sorted = false;
	std::sort(data->begin(), data->end(), pred);

	current_sorttype = requested_sorttype;
	current_sortdir = requested_sortdir;
	sorted = true;
}

//***************************************************************************************
ValueSet* ValueSet::CreateSortedCopy
(SortType requested_sorttype, SortDirection requested_sortdir) const
{
	ValueSet* clone = context->CreateValueSet();

	clone->sorted = sorted;
	clone->current_sortdir = current_sortdir;
	clone->current_sorttype = current_sorttype;

	//Could very well fail with a very big set
	try {
		clone->data->reserve(data->size());
		*(clone->data) = *data;
		clone->Sort(requested_sorttype, requested_sortdir);
	}
	catch (...) {
		context->DestroyValueSet(clone);
		throw;
	}

	return clone;
}

//***************************************************************************************
void ValueSet::ReverseOrder()
{
	int num_elements = data->size();
	int num_swaps = num_elements / 2;

	FieldValue temp;
	int y = num_elements - 1;
	for (int x = 0; x < num_swaps; x++, y--)
		(*data)[x].Swap((*data)[y]);
}

//***************************************************************************************
ValueSetCursor* ValueSet::OpenCursor(bool gotofirst)
{
	ValueSetCursor* c = new ValueSetCursor(this);
	RegisterNewCursor(c);

	if (gotofirst)
		c->GotoFirst();

	return c;
}

//***************************************************************************************
ValueSetCursor* ValueSetCursor::CreateClone()
{
	ValueSetCursor* c = Set()->OpenCursor(false);

	c->ix = ix;

	return c;
}

//***************************************************************************************
//This is reasonable on this system because we know that field attributes must match 
//across a group, so the values for one field will always arrive in the same order.
//***************************************************************************************
void ValueSet::Merge(std::vector<FieldValue>* newvals)
{
	std::vector<FieldValue>* merged_vector = new std::vector<FieldValue>;

	try {
		//See tech docs for comments on this size-guessing process.  Note also that when
		//building a value set from btrees, the values are always in ascending order, so 
		//we can do these tests knowing that the first element in each mergee is its lowest
		//and the last element is its highest.
		double sizeguess;
		const FieldValue& curlo = (*data)[0];
		const FieldValue& curhi = (*data)[data->size()-1];
		const FieldValue& newlo = (*newvals)[0];
		const FieldValue& newhi = (*newvals)[newvals->size()-1];

		//Non-overlapping sets - no "guessing" required
		if (curhi.Compare(newlo) < 0 || newhi.Compare(curlo) < 0)
			sizeguess = data->size() + newvals->size(); 

		//Overlapping - probably lots of shared values
		else {
			if (newvals->size() > data->size())
				sizeguess = newvals->size() + (data->size() * 0.2); 
			else
				sizeguess = data->size() + (newvals->size() * 0.2);
		}

		merged_vector->reserve((size_t)sizeguess);

		//The making of the above guess is the reason not to use the STL generalized 
		//merge below (we would be wasting our knowledge of "typical" DPT data profiles)
		//The actual merging is very straightforward and efficient anyway:

		//Let there be two vectors, D and N.  Here is a cursor into each.
		size_t cursor_d = 0;
		size_t cursor_n = 0;

		for (;;) {
			bool use_d;
			bool same_value = false;

			//Reached end of D?
			if (cursor_d == data->size()) {

				//N too means the end of the merge
				if (cursor_n == newvals->size())
					break;                           

				//So use value from N
				use_d = false;
			}

			//Reached end of N?
			else if (cursor_n == newvals->size()) {

				//So use value from D
				use_d = true;
			}

			//Still going through both
			else {

				//So use value that's lowest 
				int cmp = (*data)[cursor_d].Compare( (*newvals)[cursor_n] );
				use_d = (cmp <= 0);
				same_value = (cmp == 0);
			}

			//---------------------------------------------------
			//Place value from D in result and advance its cursor
			if (use_d) {
				merged_vector->push_back( (*data)[cursor_d]);
				cursor_d++;

				//(advance both cursors if it's a dupe value)
				if (same_value)
					cursor_n++;
			}

			//Or do the same with N
			else {
				merged_vector->push_back( (*newvals)[cursor_n]);
				cursor_n++;
			}
		}
	}
	catch (...) {
		delete merged_vector;
		throw;
	}

	//Now just swap in the new vector
	delete data;
	data = merged_vector;

	context->DBAPI()->IncStatMERGES();
}

//***************************************************************************************
FieldValue ValueSet::AccessRandomValue(int x)
{
	if (data == NULL || x >= data->size())
		throw Exception(DML_NONEXISTENT_RECORD, "Invalid random access value set index given");

	return (*data)[x];
}


} //close namespace


