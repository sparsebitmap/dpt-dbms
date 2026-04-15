/****************************************************************************************
Value sets - the results of a value find.  Using the value set cursor is like using
the direct value cursor (see valdirect).  Quicker for multiple walks, but requires 
memory to hold the set, and will become out of date if file info changes.
****************************************************************************************/

#if !defined(BB_VALSET)
#define BB_VALSET

#include <set>
#include <vector>
#include "dbcursor.h"
#include "bbfloat.h"
#include "fieldval.h"
#include "apiconst.h"

namespace dpt {

class DatabaseFileContext;
class SingleDatabaseFileContext;
class ValueSetCursor;
class FieldValue;

//***************************************************************************************
class ValueSet : public DBCursorTarget {
	DatabaseFileContext* context;

	bool sorted;
	SortType current_sorttype;
	SortDirection current_sortdir;

	int num_group_value_sets;
	SingleDatabaseFileContext* premerge_sfc;

	friend class ValueSetCursor;
	std::vector<FieldValue>* data; 

	friend class DatabaseFileContext;
	ValueSet(DatabaseFileContext* c) 
		: context(c), sorted(false), num_group_value_sets(0),
			premerge_sfc(NULL), data(new std::vector<FieldValue>) {}
	~ValueSet() {delete data;}

	friend class DatabaseFileIndexManager;
	void InitializeSetFromBtree(SortType t) {
		sorted = true; current_sortdir = SORT_ASCENDING; current_sorttype = t;}
	FieldValue* NewBackMember() {data->push_back(FieldValue()); return &(data->back());}

	void Merge(std::vector<FieldValue>*);
	void ReverseOrder();

public:
	DatabaseFileContext* Context() {return context;}

	ValueSetCursor* OpenCursor(bool gotofirst = true);
	void CloseCursor(ValueSetCursor* c) {DeleteCursor((DBCursor*)c);}

	int Count() {return data->size();}
	void Clear() {RequestRepositionCursors(); data->clear();}

	//Only non-empty member sets (i.e. where we had to merge -see also MRGVALS stat)
	int NumGroupValueSets() {return num_group_value_sets;}

	//Sort in place - actually converts data type of all vals if you request different.
	//Note: nothing is done if it is thought that the values are currently in order.
	void Sort(SortType = SORT_DEFAULT_TYPE, SortDirection = SORT_ASCENDING);

	//Leaves the original alone.  std::sort is used just the same.  The copy is "owned"
	//by the same context as the original.  Might change this later to be "floating".
	ValueSet* CreateSortedCopy
		(SortType = SORT_DEFAULT_TYPE, SortDirection = SORT_ASCENDING) const;

	//V3.0 Feb 2011. See comments in recset.h.
	FieldValue AccessRandomValue(int setix);
};

//***************************************************************************************
class ValueSetCursor : public DBCursor {
	ValueSet* Set() {return static_cast<ValueSet*>(Target());}

	int ix;

	friend class ValueSet;
	ValueSetCursor(ValueSet* s) : DBCursor(s), ix(-1) {}
	~ValueSetCursor() {}

	void RequestReposition(int) {ix = -1;}

public:
	bool Accessible() {return (ix >= 0 && ix < Set()->Count());}
	void GotoFirst() {if (Set()->Count() > 0) ix = 0; else ix = -1;}
	void GotoLast() {ix = Set()->Count() - 1;}
	void Advance(int n = 1) {ix += n; if (!Accessible()) ix = -1; PostAdvance(n);}

	bool GetCurrentValue(FieldValue& v ) {if (Accessible()) {
											v = (*Set()->data)[ix]; 
											return true;}
											else {
												v.AssignData("", 0); return false;}}

	FieldValue GetCurrentValue() {FieldValue v; GetCurrentValue(v); return v;}

	//V2.06 Jul 07.
	ValueSetCursor* CreateClone();
};

} //close namespace

#endif
