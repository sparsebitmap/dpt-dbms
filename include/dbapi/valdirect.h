//****************************************************************************************
//For direct walks against a field's b-tree
//****************************************************************************************

#if !defined(BB_API_VALDIRECT)
#define BB_API_VALDIRECT

#include "apiconst.h"
#include "cursor.h"
#include "fieldval.h"
#include "fieldatts.h"

namespace dpt {

class DirectValueCursor;

class APIDirectValueCursor : public APICursor {
public:
	DirectValueCursor* target;
	APIDirectValueCursor(DirectValueCursor*);
	APIDirectValueCursor(const APIDirectValueCursor&);
	//-----------------------------------------------------------------------------------

	void Advance(int = 1); 
	bool Accessible();

	//Returns false if cursor is not at an accessible position in the btree
	bool GetCurrentValue(APIFieldValue&);

	//Alternate syntax involving an extra temporary and returning null string if invalid
	APIFieldValue GetCurrentValue();

	//Can be handy to know the processing order type, or if it's an invisible field etc.
	APIFieldAttributes GetFieldAtts();

	//------------------------------
	//Alternate syntax for setting up the cursor (any existing pos in btree is lost)
	void SetDirection(CursorDirection);
	void SetRestriction_LoLimit(const APIFieldValue&, bool inclusive = true);
	void SetRestriction_HiLimit(const APIFieldValue&, bool inclusive = true);
	void SetRestriction_Pattern(const std::string& p, bool notlike = false);
	void GotoFirst();
	void GotoLast();

	//Sometimes handy as "bookmarks".
	APIDirectValueCursor CreateClone();
	void AdoptPositionFrom(APIDirectValueCursor&); //invalidates donor
	void SwapPositionWith(APIDirectValueCursor&);

	//Often the fastest way to position the cursor within a range.
	bool SetPosition(const APIFieldValue&);
};


} //close namespace

#endif
