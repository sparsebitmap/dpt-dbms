//****************************************************************************************
//The result of database value search
//****************************************************************************************

#if !defined(BB_API_VALSET)
#define BB_API_VALSET

#include "apiconst.h"
#include "cursor.h"
#include "fieldval.h"

namespace dpt {

class ValueSetCursor;

class APIValueSetCursor : public APICursor {
public:
	ValueSetCursor* target;
	APIValueSetCursor(ValueSetCursor*);
	APIValueSetCursor(const APIValueSetCursor&);
	//-----------------------------------------------------------------------------------

	//Returns false if cursor is invalid
	bool GetCurrentValue(APIFieldValue&);

	//Alternate syntax involving an extra temporary and returning null string if invalid
	APIFieldValue GetCurrentValue();

	//Sometimes handy for use as a "bookmark".
	APIValueSetCursor* CreateClone();
};

//***************************************************************************************
class ValueSet;

class APIValueSet {
public:
	ValueSet* target;
	APIValueSet(ValueSet* t) : target(t) {}
	APIValueSet(const APIValueSet& t) : target(t.target) {}
	//-----------------------------------------------------------------------------------

	APIValueSetCursor OpenCursor();
	void CloseCursor(const APIValueSetCursor& c);

	int Count();
	void Clear();

	//Only non-empty member sets (i.e. where we had to merge -see also MRGVALS stat)
	int NumGroupValueSets();

	//Sort in place - actually converts data type of all vals if you request different.
	//Note: nothing is done if it is thought that the values are currently in order.
	void Sort(SortType = SORT_DEFAULT_TYPE, SortDirection = SORT_ASCENDING);

	//Leaves the original alone.  std::sort is used just the same.  The copy is "owned"
	//by the same context as the original.  Might change this later to be "floating".
	APIValueSet CreateSortedCopy
		(SortType = SORT_DEFAULT_TYPE, SortDirection = SORT_ASCENDING) const;

	//V3.0 Feb 2011. See comments in recset.h.
	APIFieldValue AccessRandomValue(int setix);
};

} //close namespace

#endif
