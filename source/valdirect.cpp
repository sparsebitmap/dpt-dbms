
#include "stdafx.h"

#include "valdirect.h"

//Utils
#include "pattern.h"
#include "dataconv.h"
#include "winutil.h"
//API Tiers
#include "cfr.h"
#include "btree.h"
#include "dbctxt.h"
#include "dbfile.h"
#include "dbf_field.h"
#include "findspec.h"
//Diagnostics
#include "except.h"
#include "msg_db.h"

namespace dpt {

static std::string static_empty_string;

//***********************************************************************************
DirectValueCursor::DirectValueCursor
(SingleDatabaseFileContext* c, const std::string& fname)
: DBCursor(c), context(c), pfi(NULL), btree(NULL), direction(CURSOR_ASCENDING),
	lolimit(false), hilimit(false),
	pattern(false), pattern_value(NULL),pattern_notlike(false),
	advance_count(0), index_prelock(NULL)
{
	//Get field attributes so we can find the btree root
	pfi = c->GetDBFile()->GetFieldMgr()->GetPhysicalFieldInfo(c, fname);

	context->GetDBFile()->CheckFileStatus(false, true, true, false);
}

//***********************************************************************************
//When used in FRV and also in Find/Count values
//***********************************************************************************
DirectValueCursor::DirectValueCursor
(SingleDatabaseFileContext* c, const FindValuesSpecification& spec)
: DBCursor(c), context(c), pfi(NULL), btree(NULL), direction(CURSOR_ASCENDING),
	lolimit(false), hilimit(false),
	pattern(false), pattern_value(NULL), pattern_notlike(false),
	advance_count(0), index_prelock(NULL)
{
	std::string fname;

	//The user may have given one or two criteria (range and/or pattern)
	FindSpecNode_Leaf* single;
	FindSpecNode_Boolean* twosome;
	FindSpecNode_Leaf* first;
	FindSpecNode_Leaf* second;

	single = spec.rootnode->CastToLeaf();

	if (single)
		fname = single->fieldname;
	else {
		twosome = spec.rootnode->CastToBoolean();
		first = twosome->node1->CastToLeaf();
		second = twosome->node2->CastToLeaf();
		fname = first->fieldname; //(both will be the same)
	}

	if (single)
		SetRestriction_Generic(single->optr, single->operand1, single->operand2);
	else {
		SetRestriction_Generic(first->optr, first->operand1, first->operand2);
		SetRestriction_Generic(second->optr, second->operand1, second->operand2);
	}

	//Get field attributes so we can find the btree root
	pfi = c->GetDBFile()->GetFieldMgr()->GetPhysicalFieldInfo(c, fname);

	context->GetDBFile()->CheckFileStatus(false, true, true, false);
}



//***********************************************************************************
//When used during record find operations
//***********************************************************************************
DirectValueCursor::DirectValueCursor
(SingleDatabaseFileContext* c, PhysicalFieldInfo* p, CFRSentry* lk)
: DBCursor(c), context(c), pfi(p), btree(NULL), direction(CURSOR_ASCENDING),
  lolimit(false), hilimit(false),
  pattern(false), pattern_value(NULL),pattern_notlike(false),
  advance_count(0), index_prelock(lk)
{}

//***************************************************************************************
DirectValueCursor* DirectValueCursor::CreateClone()
{
	//This gets the cursor registered with the owning context
	DirectValueCursor* clone = context->OpenDirectValueCursor(pfi);

	try {
		//This is the main overhead as the clone DVC needs its own btree page handles.
		clone->btree = btree->CreateClone();

		//A minor overhead and then only when a complex pattern was used.
		if (pattern_value)
			clone->pattern_value = new util::Pattern(pattern_string);

		//This should all be fairly trivial
		clone->found_initial_position = found_initial_position;
		clone->direction = direction;
		clone->lolimit = lolimit;
		clone->lolimit_inclusive = lolimit_inclusive;
		clone->lolimit_value = lolimit_value;
		clone->hilimit = hilimit;
		clone->hilimit_inclusive = hilimit_inclusive;
		clone->hilimit_value = hilimit_value;
		clone->pattern = pattern;
		clone->pattern_string = pattern_string;
		clone->pattern_fixed_prefix = pattern_fixed_prefix;
		clone->pattern_notlike = pattern_notlike;
		clone->leaf_tstamp = leaf_tstamp;
		clone->advance_count = advance_count;
		clone->test_value = test_value;

		return clone;
	}
	catch (...) {
		context->CloseDirectValueCursor(clone);
		throw;
	}
}

//***********************************************************************************
void DirectValueCursor::SwapPositionWith(DirectValueCursor& from)
{
	//No need to go item-by item like above.  None of the data items is an object
	//which has knowledge of the address of this object.
	const size_t bufsz = sizeof(DirectValueCursor);
	char buff[bufsz];
	memcpy(buff, this, bufsz);
	memcpy(this, &from, bufsz);
	memcpy(&from, buff, bufsz);
}

void DirectValueCursor::AdoptPositionFrom(DirectValueCursor& from)
{
	SwapPositionWith(from);
	from.Clear();
}

//***********************************************************************************
void DirectValueCursor::SetRestriction_Pattern(const std::string& p, bool notlike) 
{
	Clear();
	DeletePattern();

	pattern = true; 
	pattern_string = p; 
	pattern_notlike = notlike;

	pattern_value = new util::Pattern(pattern_string);

	//Slight kluge because of the convention decided years ago for the accessing 
	//pattern fixed prefixes turning out not to be quite suitable!  We want to do
	//minimal index searching for null patterns, not the whole index!
	if (pattern_value->IsNullPattern())
		pattern_fixed_prefix = &static_empty_string;
	else
		//V3.02. Case insensitive finds cannot do a narrowly-focused b-tree scan,
		//at least without a lot more work than there is time for right now.
		//pattern_fixed_prefix = pattern_value->FixedPrefix();
		pattern_fixed_prefix = pattern_value->FixedPrefixAndCase();
}

//***********************************************************************************
void DirectValueCursor::Clear()
{
	if (btree) {
		delete btree;
		btree = NULL;
	}

	found_initial_position = false;
}

void DirectValueCursor::DeletePattern()
{
	if (pattern_value) {
		delete pattern_value;
		pattern_value = NULL;
	}
}

//***********************************************************************************
void DirectValueCursor::DynamicIndexLock(CFRSentry* s)
{
	//Case 1: Use as an actual DVC - obtain lock for duration of single advance
	if (index_prelock == NULL) {
		s->Get(context->DBAPI(), context->GetDBFile()->cfr_index, BOOL_SHR);
		return;
	}

	//Case 2: DVCs are used internally by the system to implement things like 
	//FindValues and range searches.  In these cases the INDEX CFR is obtained 
	//once up front, not for each call, but we should release it "periodically".
	advance_count++;
	if (advance_count < 5000) //an arbitrary number - several leaves perhaps
		return;

	//Give other threads a chance if they want it
	index_prelock->Release();
	win::Cede();
	index_prelock->Get(context->DBAPI(), context->GetDBFile()->cfr_index, BOOL_SHR);
	advance_count = 0;
}

//***********************************************************************************
void DirectValueCursor::PreLockIndex(CFRSentry* s)
{
	index_prelock = s;
	index_prelock->Get(context->DBAPI(), context->GetDBFile()->cfr_index, BOOL_SHR);
}

//****************************************************************************************
void DirectValueCursor::SetRestriction_Generic
(const FindOperator& optr, const FieldValue& inval1, const FieldValue& inval2)
{
	if (optr == FD_ALLVALUES)
		return;

	if (FOIsPattern(optr))
		SetRestriction_Pattern(inval1.ExtractString(), optr == FD_UNLIKE);

	else {
		bool lo_inclusive = (optr == FD_GE || optr == FD_RANGE_GE_LT || optr == FD_RANGE_GE_LE);
		bool hi_inclusive = (optr == FD_LE || optr == FD_RANGE_GT_LE || optr == FD_RANGE_GE_LE);

		if (optr == FD_GT || optr == FD_GE)
			SetRestriction_LoLimit(inval1, lo_inclusive);

		else {
			if (optr == FD_LT || optr == FD_LE)
				SetRestriction_HiLimit(inval1, hi_inclusive);

			else {
				//With ranges the first operand is always assumed to be the lowest
				SetRestriction_LoLimit(inval1, lo_inclusive);
				SetRestriction_HiLimit(inval2, hi_inclusive);
			}
		}
	}
}

//***********************************************************************************
void DirectValueCursor::InitializeScan()
{
	//* * *
	//Obvious optimization:
	//* * *
	//Use the more restrictive of the pattern and the lo/hi limits, if any, in order
	//to save walking leaf values which will definitely not pass the pattern match.
	//This is done using the pattern fixed prefix which is determined when the pattern
	//is parsed.  By far the most common case is a trailing asterisk, e.g. 'AB*', but it
	//can be any pattern that starts with some number of hard-coded characters.
	//* * *
	//We work this out now rather than doing it in the loop because it then sets 
	//a fixed range of values that will apply even if the user reverses direction 
	//in the loop. The alternative of just using the pattern when opening the btree
	//to set the initial search point obviously doesn't have this benefit.
	//* * *
	if (pattern) {
		if (!pfi->atts.IsOrdChar())
			throw Exception(DML_BAD_INDEX_TYPE, 
				"Pattern matching here requires an ORDERED CHARACTER field");

		//E.g. the user gave AMT* 
		if (pattern_fixed_prefix && !pattern_notlike) { //forget it if user wants NOT LIKE

			bool usepattlo = false;
			if (!lolimit)
				usepattlo = true;
			else if (lolimit_value.ExtractString() < *pattern_fixed_prefix)
				usepattlo = true;

			//E.g. pretend the user gave AMT as the low limit
			if (usepattlo) {
				lolimit = true;
				lolimit_inclusive = true;
				lolimit_value.AssignData(pattern_fixed_prefix->c_str(), 
									pattern_fixed_prefix->length());
			}

			//High limit:
			bool usepatthi = false;

			//We could generate an inclusive endpoint, which would be e.g. AMTZZZZZZZZ..
			//but this would have to be 255 chars long, so I think it's less messy to 
			//go exclusive with AMU.  The pattern match will fail if we end up walking
			//slightly too far anyway, so it's no big deal.
			std::string spattnext = util::GenerateNextStringValue(*pattern_fixed_prefix);

			if (!hilimit)
				usepatthi = true;
			else if (hilimit_value.ExtractString() > spattnext)
				usepatthi = true;

			if (usepatthi) {
				hilimit = true;
				hilimit_inclusive = false;
				hilimit_value.AssignData(spattnext.c_str(), spattnext.length());
			}
		}
	}

	//Use limits in the appropriate format (user may have supplied them wrong)
	if (pfi->atts.IsOrdNum()) {
		if (lolimit)
			if (!lolimit_value.CurrentlyNumeric())
				lolimit_value.ConvertToNumeric();
		if (hilimit)
			if (!hilimit_value.CurrentlyNumeric())
				hilimit_value.ConvertToNumeric();
	}
	else {
		if (lolimit)
			if (lolimit_value.CurrentlyNumeric())
				lolimit_value.ConvertToString();
		if (hilimit)
			if (hilimit_value.CurrentlyNumeric())
				hilimit_value.ConvertToString();
	}

	Clear();

	//No sense doing anything in this case
	if (lolimit && hilimit)
		if (lolimit_value.Compare(hilimit_value) > 0)
			return;

	btree = new BTreeAPI(context, pfi);
}

//***********************************************************************************
void DirectValueCursor::StartWalkAtLoLimit()
{
	InitializeScan(); 

	if (!btree)
		return;

	CFRSentry s;
	DynamicIndexLock(&s);

	bool needs_advance = false;

	//First position in the tree at the lowest value between the limits set
	if (!lolimit) {
		btree->LocateLowestValueEntryPreBrowse();

		//Lowest tree value already higher than high limit?
		if (hilimit && Accessible()) {
			GetValueForTest();
			if (HiLimitPassed()) {
				Clear();
				return;
			}
		}
	}
	else {
		bool found_lolimit = btree->LocateValueEntry(lolimit_value);

		//If the value isn't found, the btree API object is between values at the point
		//where the requested value would have been.  So walk it onto the value above.
		//This also applies if the value was found but we set a GT range.
		if (!found_lolimit || !lolimit_inclusive)
			needs_advance = true;
	}

	//If there is a pattern the first value we use must match that too
	if (pattern && !needs_advance && Accessible()) {
		GetValueForTest();
		if (PatternFailsMatch())
			needs_advance = true;
	}
	
	//This takes account of limits and pattern as a matter of course
	if (needs_advance)
		Advance( (Ascending()) ? 1 : -1 );

	if (!Accessible()) {
		Clear();
		return;
	}

	leaf_tstamp = btree->GetLeafTStamp();
	found_initial_position = true;
}

//***********************************************************************************
void DirectValueCursor::StartWalkAtHiLimit()
{
	InitializeScan(); 

	if (!btree)
		return;

	CFRSentry s;
	DynamicIndexLock(&s);

	bool needs_advance = false;

	//This is all quite similar to the above - see comments there
	if (!hilimit) {
		btree->LocateHighestValueEntryPreBrowse();

		if (lolimit && Accessible()) {
			GetValueForTest();
			if (LoLimitPassed()) {
				Clear();
				return;
			}
		}
	}
	else {
		bool found_hilimit = btree->LocateValueEntry(hilimit_value);

		if (!found_hilimit || !hilimit_inclusive)
			needs_advance = true;
	}

	if (pattern && !needs_advance && Accessible()) {
		GetValueForTest();
		if (PatternFailsMatch())
			needs_advance = true;
	}

	if (needs_advance)
		Advance( (Ascending()) ? -1 : 1 );
	
	if (!Accessible()) {
		Clear();
		return;
	}

	leaf_tstamp = btree->GetLeafTStamp();
	found_initial_position = true;
}

//***********************************************************************************
void DirectValueCursor::GetValueForTest()
{
	assert (Accessible()) ;
	btree->GetLastValueLocated(test_value);
}

//***********************************************************************************
bool DirectValueCursor::LoLimitPassed()
{
	if (!lolimit)
		return false;

	int cmp = test_value.Compare(lolimit_value);

	if (cmp < 0)
		return true;
	else if (cmp == 0 && !lolimit_inclusive)
		return true;

	return false;
}

//***********************************************************************************
bool DirectValueCursor::HiLimitPassed()
{
	if (!hilimit)
		return false;

	int cmp = test_value.Compare(hilimit_value);

	if (cmp > 0)
		return true;
	else if (cmp == 0 && !hilimit_inclusive)
		return true;

	return false;
}

//***********************************************************************************
bool DirectValueCursor::PatternFailsMatch()
{
	if (!pattern)
		return false;

	bool match = pattern_value->IsLike(test_value.ExtractString());

	if (match == pattern_notlike)
		return true;

	return false;
}

//***********************************************************************************
void DirectValueCursor::Advance(int indelta)
{
	//V2.06 Jul 07.  Since there are several return points below.
	AdvanceMain(indelta);
	PostAdvance(indelta);
}

void DirectValueCursor::AdvanceMain(int indelta)
{
	//The user (API only) may have changed some attributes of this cursor object,
	//or otherwise if they forget to gotofirst to start with anyway.
	if (!btree) {
		GotoFirst();
		return;
	}

	if (indelta == 0)
		return;

	//A negative delta will reverse the direction.  I don't know whether you can change
	//the BY in UL, but this will allow it if you can.
	CursorDirection thiswalkdir = direction;
	int thiswalkdelta = indelta;
	if (indelta < 0) {
		thiswalkdelta = -thiswalkdelta;
		thiswalkdir = !thiswalkdir;
	}

	CFRSentry s;
	DynamicIndexLock(&s);

	//Here is the check for changes to the last-accessed btree leaf.  If the last value
	//we were looking at has gone, this func automatically positions at the next/prev,
	//thus making the first real step in the loop below redundant. 
	bool stepped = false;
	if (found_initial_position && index_prelock == NULL)
		stepped = btree->DVCReposition(leaf_tstamp, thiswalkdir);

	while (thiswalkdelta > 0) {
		if (stepped) 
			stepped = false;
		else {
			if (thiswalkdir == CURSOR_ASCENDING)
				btree->WalkToNextValueEntry();
			else
				btree->WalkToPreviousValueEntry();
		}

		//Various ways to end here.  Note that in all cases when we reach an endpoint
		//the btree API is destroyed.  This is really to ensure that we don't allow
		//them to just continue to advance past the endpoint when FROM/TO are given
		//just by calling Advance() again.  In other words it *would* otherwise be a 
		//valid btree position, but we make it not so by destroying the API.

		//No more values in the btree?
		if (!Accessible()) {
			Clear();
			return;
		}

		GetValueForTest();

		//Have we gone outside the requested range?
		bool pastlimit = false;
		if (thiswalkdir == CURSOR_ASCENDING)
			pastlimit = HiLimitPassed();
		else
			pastlimit = LoLimitPassed();

		if (pastlimit) {
			Clear();
			return;
		}

		//If there's a pattern the value must match it (or not).  Even though we
		//have limited the range using any fixed prefix, the FP says nothing about
		//whether values within the range will fit, since there can be all sorts of
		//funny stuff after a fixed prefix - AB(CD,JK,\1-7(F)) etc.
		if (PatternFailsMatch())
			continue;

		//The value is OK and counts towards our loop
		thiswalkdelta--;
	}

	//Success
	leaf_tstamp = btree->GetLeafTStamp();
}

//***********************************************************************************
bool DirectValueCursor::Accessible()
{
	if (btree)
		return btree->LastLocateSuccessful();
	else
		return false;
}

//***********************************************************************************
bool DirectValueCursor::GetCurrentValue(FieldValue& v)
{
	if (Accessible()) {
		btree->GetLastValueLocated(v);
		return true;
	}

	//This is similar to the record situation where a field is missing.  
	//Debatable whether it's best but this is the way it's going to work.  As
	//with records, the caller might test the type, so may as well make it correct.
	if (pfi->atts.IsOrdNum()) 
		v = 0.0; 
	else 
		v.AssignData("", 0);

	return false;
}

//***********************************************************************************
const FieldAttributes* DirectValueCursor::GetFieldAtts()
{
	if (!pfi)
		throw Exception(BUG_MISC, "Bug: No PFI in DVC");

	return &pfi->atts;
}


//***********************************************************************************
bool DirectValueCursor::SetPosition(const FieldValue& ival)
{
	//Ensure correct type for comparisons
	const FieldValue* pval = &ival;
	FieldValue dummy;
	if (pfi->atts.IsOrdNum() != ival.CurrentlyNumeric()) {
		dummy = ival;
		pval = &dummy;
		if (pfi->atts.IsOrdNum())
			dummy.ConvertToNumeric();
		else
			dummy.ConvertToString();
	}
	const FieldValue& findval = *pval;

	FieldValue curval;
	DirectValueCursor* bookmark = NULL;
	int scandelta;
	try {

		//We have a current position - search should be quicker from here instead of an end
		bool usecurval = GetCurrentValue(curval);
		if (usecurval) {

			//Unless the user forces scan from one end
			if ((opts & CURSOR_POS_FROM_FIRST) || (opts & CURSOR_POS_FROM_LAST) )
				usecurval = false;

			else {

				//Decide which way we will have to walk.  Note that this delta is relative
				//to the direction of this cursor.  (Remember DVCs have an intrinsic direction
				//in themselves).  See usages of the delta variable later.
				int cmp = curval.Compare(findval);

				//Looking for a higher value
				if (cmp < 0) {
					if (direction == CURSOR_ASCENDING)
						scandelta = 1;					//"forwards"
					else
						scandelta = -1;					//"backwards"
				}
				//Looking for a lower value
				else if (cmp > 0) {
					if (direction == CURSOR_DESCENDING)
						scandelta = 1;					//"forwards"
					else
						scandelta = -1;					//"backwards"
				}
			}

			//Make a note for later return in case desired entry not found.  Debatable
			//whether the overhead of making this clone will be greater than walking
			//back by value.  In any case it depends how often the exact value does not
			//get found to trigger this.  It's one for the user to optimize if desired.
			if (opts & CURSOR_POSFAIL_REMAIN)
				bookmark = CreateClone();
		}

		//No current position or full scan forced by user
		if (!usecurval) {

			if (opts & CURSOR_POS_FROM_LAST) {
				GotoLast();
				scandelta = -1; //"backwards"
			}
			else  {
				GotoFirst();
				scandelta = 1; //"forwards"
			}

			//Index empty
			if (!Accessible())
				return false;
		}

		//We are now positioned on an entry in the btree.  Start loop-and-test now.
		bool found = false;

		//NB it's important we run off the end cleanly so kluge that option in.
		CursorOptions tempopts = opts;
		opts = CURSOR_DEFOPTS;

		for ( ; Accessible(); Advance(scandelta)) {
			GetCurrentValue(curval);

			int cmp = curval.Compare(findval);

			//Match!
			if (cmp == 0) {
				found = true;
				break;
			}

			//We have a value lower than the one desired
			else if (cmp < 0) {

				//Give up if that means we've "gone past"
				if (direction == CURSOR_ASCENDING) {
					if (scandelta == -1)
						break;
				}
				else {
					if (scandelta == 1)
						break;
				}
			}

			//Higher than desired
			else {

				//Give up if that means we've "gone past"
				if (direction == CURSOR_ASCENDING) {
					if (scandelta == 1)
						break;
				}
				else {
					if (scandelta == -1)
						break;
				}
			}
		}

		opts = tempopts;

		//----------------------------------------------------------------------
		//Take the appropriate fallback action if requested entry does not exist
		if (!found) {

			//Return to starting position if option set and cursor did not start null
			if (opts & CURSOR_POSFAIL_REMAIN) {
				if (bookmark)
					SwapPositionWith(*bookmark);
				else
					Clear();
			}

			//The "next" entry is wanted.  We have to take into account where our starting
			//position was (hence scan direction) to know whether to "back up".
			//Note that the "next" and "prev" in these option meanings makes the intrinsic
			//direction of the cursor irrelevant here.
			else if (opts & CURSOR_POSFAIL_NEXT) {

				//Loop already went past, possibly off the end where we should stay
				if (scandelta == 1)
					;

				//Scanning "backwards" so advance 1 again, possibly have to restart at first
				else {
					if (Accessible())
						Advance(1);
					else
						GotoFirst();
				}
			}

			//The user wants the "previous" entry.  All the same comments but the 
			//reverse of the above.
			else if (opts & CURSOR_POSFAIL_PREV) {
				if (scandelta == -1)
					;
				else {
					if (Accessible())
						Advance(-1);
					else
						GotoLast();
				}
			}

			//The default action is to break the cursor though
			else {
				Clear();
			}
		}

		//This can be discarded now
		if (bookmark)
			context->CloseDirectValueCursor(bookmark);

		return found;
	}
	catch (...) {
		if (bookmark)
			context->CloseDirectValueCursor(bookmark);
		throw;
	}
}

} //close namespace


