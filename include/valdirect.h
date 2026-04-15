/****************************************************************************************
A direct value cursor walks a btree one value at a time rather than collecting
values for later use, as happens with valueset/valuesetcursor.
****************************************************************************************/

#if !defined(BB_VALDIRECT)
#define BB_VALDIRECT

#include "dbcursor.h"
#include "fieldval.h"
#include "apiconst.h"

namespace dpt {

class SingleDatabaseFileContext;
struct PhysicalFieldInfo;
class CFRSentry;
class BTreeAPI;
class FindValuesSpecification;
class FieldAttributes;
namespace util {
	class Pattern;
}

//**************************************************************************************
class DirectValueCursor : public DBCursor {
	SingleDatabaseFileContext* context;
	PhysicalFieldInfo* pfi;

	BTreeAPI* btree;
	bool found_initial_position;

	CursorDirection direction;
	bool Ascending() {return (direction == CURSOR_ASCENDING);}

	bool lolimit;
	bool lolimit_inclusive;
	FieldValue lolimit_value;
	bool hilimit;
	bool hilimit_inclusive;
	FieldValue hilimit_value;
	bool pattern;
	util::Pattern* pattern_value;
	std::string pattern_string;
	std::string* pattern_fixed_prefix;
	bool pattern_notlike;

	int leaf_tstamp;
	int advance_count;

	void Clear();
	void DeletePattern();
	void InitializeScan();

	void StartWalkAtLoLimit();
	void StartWalkAtHiLimit();

	FieldValue test_value;
	void GetValueForTest();
	bool LoLimitPassed();
	bool HiLimitPassed();
	bool PatternFailsMatch();

	//Repositioning is now handled physically with the btree leaf timestamp
//	void RequestReposition(int param) {if (param == 2) needs_reposition = true;}

	friend class SingleDatabaseFileContext;
	friend class GroupDatabaseFileContext;
	friend class PAVCommand;
	DirectValueCursor(SingleDatabaseFileContext*, const std::string&);
	DirectValueCursor(SingleDatabaseFileContext*, const FindValuesSpecification&);
	~DirectValueCursor() {Clear(); DeletePattern();}

	friend class FindOperation;
	friend class DatabaseFileIndexManager;
	DirectValueCursor(SingleDatabaseFileContext*, PhysicalFieldInfo*, CFRSentry*);

	friend class DatabaseFileContext;
	CFRSentry* index_prelock;
	void DynamicIndexLock(CFRSentry*);
	void PreLockIndex(CFRSentry*);

	void AdvanceMain(int);

public:

	//-------------------------
	//Separate functions for these as an alternative to creating a find spec and giving
	//it to Open...()  NB. any existing position in the btree is lost if these are used.
	void SetDirection(CursorDirection cd) {
							Clear(); direction = cd;}
	void SetRestriction_LoLimit(const FieldValue& v, bool i = true) {
							Clear(); lolimit = true; lolimit_value = v; lolimit_inclusive = i;}
	void SetRestriction_HiLimit(const FieldValue& v, bool i = true) {
							Clear(); hilimit = true; hilimit_value = v; hilimit_inclusive = i;}
	//Pattern with initial fixed portion (e.g. ABC*) may also imply hi and lo limit too.
	void SetRestriction_Pattern(const std::string& p, bool notlike = false);

	//Can be nicer to use.  The UL Find engine uses this a lot.
	void SetRestriction_Generic(const FindOperator&, const FieldValue&, const FieldValue& = FieldValue());

	//-------------------------
	//NB. Different meaning depending on direction.
	void GotoFirst() {if (Ascending()) StartWalkAtLoLimit(); else StartWalkAtHiLimit();}
	void GotoLast() {if (Ascending()) StartWalkAtHiLimit(); else StartWalkAtLoLimit();}

	//NB1. If GF or GL isn't called before first Adv, GF is substituted regardless of delta.  
	//NB2. If descending, delta>0 moves down, delta<0 moves up. Delta=0 always does nothing.
	//NB3. Once btree end or a limit is reached, cursor reinits (so next call is NB1 case)
	void Advance(int delta = 1);

	//-------------------------
	//Valid if the last Advance() or Gotoxxx() finished on a value in the btree.
	bool Accessible();
	//Return value confirms valid pos.  If not, value is set to zero/null as per ftype
	bool GetCurrentValue(FieldValue&);
	//More convenient syntax sometimes
	FieldValue GetCurrentValue() {FieldValue temp; GetCurrentValue(temp); return temp;}

	//Can be handy to know the processing order type, or if it's an invisible field etc.
	const FieldAttributes* GetFieldAtts();

	//V2.06 Jul 07.  Use for "bookmarks" to return to during btree walkabouts.
	DirectValueCursor* CreateClone();
	void AdoptPositionFrom(DirectValueCursor&); //invalidates donor
	void SwapPositionWith(DirectValueCursor&);
	
	//Often faster than your own loop since less of the btree may have to be walked.
	//RC says whether entry found.  (Default option leaves cursor inaccessible if not).
	bool SetPosition(const FieldValue&);
};

} //close namespace

#endif
