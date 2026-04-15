
#include "stdafx.h"

#include "btree.h"

#include "float.h"
#include <algorithm>

//Utils
#include "parsing.h"
#include "dataconv.h"
#include "bbfloat.h"
//API Tiers
#include "cfr.h"
#include "dbctxt.h"
#include "bmset.h"
#include "frecset.h"
#include "dbf_field.h"
#include "dbf_tabled.h"
#include "dbfile.h"
#include "btree.h"
#include "page_t.h" //#include "page_T.h" : V2.24 case is less interchangeable on *NIX - Roger M.
//Diagnostics
#include "except.h"
#include "msg_db.h"
#include "infostructs.h"

//For analyze3
#ifdef _BBHOST
#include "iodev.h"
#else
#include "lineio.h"
#endif

namespace dpt {

//****************************************************************************************
void BTreeAPI::Initialize(SingleDatabaseFileContext* sfc, PhysicalFieldInfo* p)
{
	context = sfc;
	pfi = p;

	if (!pfi->atts.IsOrdered())
		throw Exception(DML_INDEX_REQUIRED, 
			std::string("Field is not indexed: ").append(pfi->name));

	//These are used all over the code below - retrieve once now for readability
	file = context->GetDBFile();
	tdmgr = file->GetTableDMgr();
	dbapi = context->DBAPI();
}

//****************************************************************************************
void BTreeAPI::InitializeCache()
{
	for (int x = 0; x < MAX_TREE_DEPTH; x++) {
		buffpage[x].Release(); 
		buffpagenum[x] = -1; 
	}
	leaf_level = -1;
	last_locate_successful = false;
}

//****************************************************************************************
BTreeAPI* BTreeAPI::CreateClone()
{
	BTreeAPI* clone = new BTreeAPI(context, pfi);

	try {
		clone->leaf_level = leaf_level;
		clone->leaf_value_offset = leaf_value_offset;
		clone->leaf_ilmr_offset = leaf_ilmr_offset;

		clone->last_value_located = last_value_located;
		clone->last_locate_successful = last_locate_successful;

		int x;
		for (x = 0; x < MAX_TREE_DEPTH; x++)
			clone->buffpagenum[x] = buffpagenum[x];

		//This is the main overhead when cloning a DVC.  This object needs its own page handles
		//- all should be in buffers but in any case still faster than rewalking from start.
		for (x = 0; x < MAX_TREE_DEPTH; x++) {
			if (buffpagenum[x] >= 0)
				clone->buffpage[x] = tdmgr->GetTableDPage(dbapi, buffpagenum[x]);
		}

		return clone;
	}
	catch (...) {
		delete clone;
		throw;
	}
}

//****************************************************************************************
InvertedListAPI BTreeAPI::InvertedList()
{
	//The offsets used within this class are all area-relative to cater for the different
	//possible configurations of btree node, but the inverted list code uses a "normal" 
	//page mapper so translate to page-relative here.
	BTreePage pt(buffpage[leaf_level]);
	short page_offset = pt.BL_PagePos(leaf_ilmr_offset);

	return InvertedListAPI(context, &buffpage[leaf_level], page_offset);
}

//****************************************************************************************
bool BTreeAPI::LocateValueEntry_Part1(bool refind)
{
	InitializeCache();

	//Increment stat whatever happens to indicate we tried something
	if (refind)
		file->IncStatBXRFND(dbapi);
	else
		file->IncStatBXFIND(dbapi);

	//Start at the root if there is one
	buffpagenum[0] = pfi->btree_root;
	if (buffpagenum[0] == -1)
		return false;
	else
		return true;
}

//****************************************************************************************
void BTreeAPI::LocateValueEntry_Part2(const FieldValue* pvalue, bool lowest)
{
	//Move leafwards (the root may also be a leaf)
	short search_level = 0;
	for (;;) {
		buffpage[search_level] = tdmgr->GetTableDPage(dbapi, buffpagenum[search_level]);
		BTreePage pt(buffpage[search_level]);

		//Reached a leaf
		if (pt.is_leaf) {
			leaf_level = search_level;
			break;
		}

		//Otherwise follow the appropriate branch
		short value_offset;
		short pagenum_offset;

		//We may be looking for:
		if (pvalue)
			//A. Specific value (want page containing value, or value to insert after)
			pt.B_LocateValueLE(*pvalue, value_offset, pagenum_offset);
		else if (lowest)
			//B. Lowest value on page
			pt.BL_LocateNextValue(-1, value_offset, pagenum_offset);
		else
			//C. Highest value on page
			pt.BL_LocatePreviousValue(-1, value_offset, pagenum_offset);

		search_level++;
		pt.BL_GetData(pagenum_offset, buffpagenum[search_level], NULL);
	}
}

//****************************************************************************************
bool BTreeAPI::LocateValueEntry(const FieldValue& value, bool findonly, bool refind)
{
	//Position at root if any
	if (!LocateValueEntry_Part1(refind))
		return false;

/*	In a find we could check a low value cached at the root, as we won't be
	needing to log the "route" to it, or walk from it.  (Still thinking about this
	idea - see tech docs for more).
	if (findonly) {
		//BTreePage pt(buffpage[0]);
		//get loval, compare, return false if pvalue is lower
		if (false)
			return false;
	}
*/

	//Scan down to the appropriate leaf
	LocateValueEntry_Part2(&value);

	//Look on the leaf page we end up on - this is where it'll be if it's there at all.
	BTreePage pt(buffpage[leaf_level]);

	last_locate_successful = pt.L_LocateValue(value, leaf_value_offset, leaf_ilmr_offset);
	if (last_locate_successful)
		last_value_located = value;

	return last_locate_successful;
}

//****************************************************************************************
bool BTreeAPI::LocateLowestValueEntryPreBrowse() 
{
	//Start at root if any
	if (!LocateValueEntry_Part1())
		return false;

	//Move to first leaf
	LocateValueEntry_Part2(NULL, true);

	BTreePage pt(buffpage[leaf_level]);

	//See tech docs - the leftmost node may be empty
	if (pt.BL_LocateNextValue(-1, leaf_value_offset, leaf_ilmr_offset))
		
		//Leftmost node not empty
		pt.BL_GetValue(leaf_value_offset, last_value_located);

	else {
		//Othwerwise walk right.  No BXNEXT stat - let's say it's part of the BXFIND :-)
		buffpagenum[leaf_level] = pt.L_GetRightSibling();
		buffpage[leaf_level] = tdmgr->GetTableDPage(dbapi, buffpagenum[leaf_level]);
		BTreePage ptrsib(buffpage[leaf_level]);

		ptrsib.BL_LocateNextValue(-1, leaf_value_offset, leaf_ilmr_offset);
		ptrsib.BL_GetValue(leaf_value_offset, last_value_located);
	}

	last_locate_successful = true;
	return true;
}

//****************************************************************************************
bool BTreeAPI::LocateHighestValueEntryPreBrowse() 
{
	//Start at root if any
	if (!LocateValueEntry_Part1())
		return false;

	//Move to last leaf
	LocateValueEntry_Part2(NULL, false);

	BTreePage pt(buffpage[leaf_level]);

	//There must be a value on the rightmost leaf (cf. leftmost above)
	pt.BL_LocatePreviousValue(-1, leaf_value_offset, leaf_ilmr_offset);
	pt.BL_GetValue(leaf_value_offset, last_value_located);

	last_locate_successful = true;
	return true;
}

//****************************************************************************************
bool BTreeAPI::DVCReposition(int prev_leaf_tstamp, CursorDirection cursordir)
{
	//Has someone else changed the leaf since the DVC released the INDEX CFR?
	try {
		BTreePage pt(buffpage[leaf_level]);

		//The page may have been emptied and reused and now not be a leaf.
		if (pt.is_leaf)
			if (pt.L_GetTStamp() == prev_leaf_tstamp)
				return false;
	}
	catch (Exception& e) {
		//The leaf may have been emptied and reused as something non-btree-related
		if (e.Code() != DB_UNEXPECTED_PAGE_TYPE)
			throw;
	}

	//Relocate the last value we were at.  NB: It might be fairly easy to avoid
	//going right back to the root if only the leaf has changed, just by holding
	//some more node version numbers, but a total re-scan is nice and clean.
	if (LocateValueEntry(last_value_located, false, true))
		return false;

	//The old value has gone - move to the next
	if (cursordir == CURSOR_ASCENDING)
		WalkToNextValueEntry();
	else
		WalkToPreviousValueEntry();

	//This says that the DVC does not have to do an advance itself now.
	return true;
}

//****************************************************************************************
int BTreeAPI::GetLeafTStamp()
{
	if (leaf_level == -1)
		return -1;
	else
		return BTreePage(buffpage[leaf_level]).L_GetTStamp();
}

//****************************************************************************************
bool BTreeAPI::WalkToNextValueEntry()
{
	//Can't walk if there's no tree yet
	if (leaf_level == -1)
		return false;

	BTreePage pt(buffpage[leaf_level]);

	//Previous search got a value
	if (last_locate_successful) {

		//So get the next
		last_locate_successful = pt.BL_LocateNextValue
				(leaf_value_offset, leaf_value_offset, leaf_ilmr_offset);

		file->IncStatBXNEXT(dbapi);

		if (last_locate_successful) {
			pt.BL_GetValue(leaf_value_offset, last_value_located);
			return true;
		}
	}

	//Previous search failed.  This walk function would be called under such circumstances
	//in e.g. FRV loops or range finds which specify GE a nonexistent value, Also any GT.
	else {
		
		//We may already be positioned at the next value from the failed search
		if (pt.BL_PositionIsWithinData(leaf_value_offset)) {
			last_locate_successful = true;
			pt.BL_GetValue(leaf_value_offset, last_value_located);
			return true;
		}
	}

	//No more values on the current page - chain right if possible
	int right_sibling = pt.L_GetRightSibling();
	if (right_sibling == -1)
		return false;

	buffpagenum[leaf_level] = right_sibling;
	buffpage[leaf_level] = tdmgr->GetTableDPage(dbapi, right_sibling);
	BTreePage ptright(buffpage[leaf_level]);

	//In theory there must be at least one value on the right sibling if it exists
	last_locate_successful = true;
	ptright.BL_LocateNextValue(-1, leaf_value_offset, leaf_ilmr_offset);
	ptright.BL_GetValue(leaf_value_offset, last_value_located);
	return true;
}

//****************************************************************************************
bool BTreeAPI::WalkToPreviousValueEntry()
{
	//Can't walk if there's no tree yet
	if (leaf_level == -1)
		return false;

	BTreePage pt(buffpage[leaf_level]);

	//See comments above re. the previous found/not found situations.  In this case we
	//will never be positioned in the right place after a failed search - always walk back.
	last_locate_successful = pt.BL_LocatePreviousValue
			(leaf_value_offset, leaf_value_offset, leaf_ilmr_offset);

	//Same stat though - there is no BXPREV stat
	file->IncStatBXNEXT(dbapi);

	if (last_locate_successful) {
		pt.BL_GetValue(leaf_value_offset, last_value_located);
		return true;
	}

	//No more values on the current page - chain left if possible
	int left_sibling = pt.L_GetLeftSibling();
	if (left_sibling == -1)
		return false;

	buffpagenum[leaf_level] = left_sibling;
	buffpage[leaf_level] = tdmgr->GetTableDPage(dbapi, left_sibling);
	BTreePage ptleft(buffpage[leaf_level]);

	//See tech docs ... the left sibling may be empty if it's the leftmost leaf.
	if (!ptleft.BL_LocatePreviousValue(-1, leaf_value_offset, leaf_ilmr_offset))
		return false;

	last_locate_successful = true;
	ptleft.BL_GetValue(leaf_value_offset, last_value_located);
	return true;
}


//****************************************************************************************
void BTreeAPI::InsertValueEntry(const FieldValue& value, bool recursive_call)
{
	//First value for this field - there will not be a btree at all yet
	if (leaf_level == -1) {
		CreateNewSoloRoot();
		leaf_value_offset = -1;
	}

	BTreePage pt(buffpage[leaf_level]);

	//Sufficient room on the page?
	if (pt.BL_InsertValue(value, leaf_value_offset, leaf_ilmr_offset)) {
		pt.L_IncTStamp();
		file->IncStatBXINSE(dbapi);
		return;
	}

	//No - it's leaf split time.
	//Only this first allocation can be benign vis-a-vis backout
	int pnumnew = tdmgr->AllocatePage(dbapi, 'T', !recursive_call);
	BufferPageHandle bhnew = tdmgr->GetTableDPage(dbapi, pnumnew, true);

	BTreePage ptnew(bhnew, pfi->atts.IsOrdNum(), 'L');

	//The new page is always treated as the "right hand" one of the two, so splice it
	//in between the current page and its right sibling, if any.
	int old_right_sibling = pt.L_GetRightSibling();
	pt.L_SetRightSibling(pnumnew);
	ptnew.L_SetRightSibling(old_right_sibling);
	ptnew.L_SetLeftSibling(buffpagenum[leaf_level]);

	if (old_right_sibling != -1) {
		BufferPageHandle bhrsib = tdmgr->GetTableDPage(dbapi, old_right_sibling);
		BTreePage ptrsib(bhrsib);
		ptrsib.L_SetLeftSibling(pnumnew);
	}

	//Another node is required if the existing leaf was a solo root.
	if (pt.is_root) {
		MakeNewRootDuringSplit(pt);
		leaf_level++;
	}

	//Divide up the entries
	FieldValue splitvalleft;
	pt.BL_DivideValues(&ptnew, pfi->atts.Splitpct(), splitvalleft);

	//Create a new branch pointer at the next-highest level for the new right hand node
	FieldValue splitvalright;
	ptnew.BL_GetValue(0, splitvalright);

	//For string trees we can apply suffix compression to the branch value actually stored
	if (pfi->atts.IsOrdChar()) {
		short sh = util::CountSharedChars(splitvalleft.StrChars(), splitvalleft.StrLen(),
										splitvalright.StrChars(), splitvalright.StrLen());

		//Add one character to differentiate.  Note that there must be at least one
		//differentiating character if we have got to here.
		splitvalright.StrTruncate(sh + 1);
	}
	
	//This may cause splits further up
	if (InsertBranchPointer(leaf_level - 1, splitvalright, pnumnew))
		leaf_level++;

	file->IncStatBXSPLI(dbapi);

	//Is the new value going on the new leaf?  Note that we have to compare with the
	//value actually stored in the branch area (i.e. possibly truncated).
	BTreePage& ptchosen = pt;
	if (value.Compare(splitvalright) >= 0) {
		buffpagenum[leaf_level] = pnumnew;
		buffpage[leaf_level] = bhnew;
		ptchosen = ptnew;
	}

	//Relocate the appropriate position on either the L or R node
	ptchosen.L_LocateValue(value, leaf_value_offset, leaf_ilmr_offset);

	//Not necessarily finished yet - prefix compression may be compromised so much that
	//more than one leaf split is required - hence recusive call.  See tech notes for more.
	InsertValueEntry(value, true);
}

//****************************************************************************************
void BTreeAPI::CreateNewSoloRoot()
{
	int pnum = tdmgr->AllocatePage(dbapi, 'T', true);
	leaf_level = 0;
	buffpagenum[0] = pnum;
	buffpage[0] = tdmgr->GetTableDPage(dbapi, pnum, true);
	BTreePage pt(buffpage[leaf_level], pfi->atts.IsOrdNum(), 'S');

	//Update the field attributes to reflect the new root
	pfi->btree_root = pnum;
	file->GetFieldMgr()->UpdateBTreeRootPage(dbapi, pfi);
}

//****************************************************************************************
bool BTreeAPI::InsertBranchPointer(short lev, const FieldValue& value, const int child_pagenum)
{
	BTreePage pt(buffpage[lev]);

	//Locate the insertion position.  This function is designed to find a value less than 
	//the one it's given if anything, so advance to the next position (possibly the end).
	short valpos, datapos;
	if (pt.B_LocateValueLE(value, valpos, datapos))
		pt.BL_LocateNextValue(valpos, valpos, datapos);

	//Sufficient room on the page?
	if (pt.BL_InsertValue(value, valpos, datapos)) {
		pt.B_SetChildPageNum(datapos, child_pagenum);
		return false;
	}

	//No - INode split time.  This is quite similar to the leaf code above.  The
	//main difference is the absence of sibling pointers at these levels.
	int pnumnew = tdmgr->AllocatePage(dbapi, 'T', false);
	BufferPageHandle bhnew = tdmgr->GetTableDPage(dbapi, pnumnew, true);
	BTreePage ptnew(bhnew, pfi->atts.IsOrdNum(), 'N');

	//Another node is required if the existing node was the root.
	bool new_level_required = false;
	if (pt.is_root) {
		MakeNewRootDuringSplit(pt);
		new_level_required = true;
		lev++;
	}

	//Divide up the entries
	FieldValue splitvalleft;
	pt.BL_DivideValues(&ptnew, pfi->atts.Splitpct(), splitvalleft);

	//Create a new branch pointer at the next-highest level for the new right hand node.
	FieldValue splitvalright;
	ptnew.BL_GetValue(0, splitvalright);

	//Suffix compression - see comments in leaf code above.
	//Oops - no suffix compression on higher branches - see tech docs!!!
	//if (pfi->atts.IsOrdChar()) {
	//	short sh = util::CountSharedChars(splitvalleft.StrChars(), splitvalleft.StrLen(),
	//									splitvalright.StrChars(), splitvalright.StrLen());
	//	splitvalright.StrTruncate(sh + 1);
	//}

	//This might cause a new level further up
	if (InsertBranchPointer(lev - 1, splitvalright, pnumnew)) {
		new_level_required = true;
		lev++;
	}

	file->IncStatBXSPLI(dbapi);

	//Choose sibling - see comments in leaf code above.
	if (value.Compare(splitvalright) >= 0) {
		buffpagenum[lev] = pnumnew;
		buffpage[lev] = bhnew;
	}

	//Recursive call - see comment in leaf code above.
	new_level_required |= InsertBranchPointer(lev, value, child_pagenum);

	return new_level_required;
}

//****************************************************************************************
void BTreeAPI::MakeNewRootDuringSplit(BTreePage& ptoldroot)
{
	if (leaf_level == MAX_TREE_DEPTH - 1)
		throw Exception(DB_STRUCTURE_BUG, "Bug: trying to grow btree too deep");

	int pnum = tdmgr->AllocatePage(dbapi, 'T', false);
	BufferPageHandle bh = tdmgr->GetTableDPage(dbapi, pnum, true);
	BTreePage ptnewroot(bh, pfi->atts.IsOrdNum(), 'R');

	//Convert the old root into a pure Inode or leaf
	ptoldroot.ConvertRootToPureNode(ptnewroot);

	//Shuffle up our cached buffer handles to make room
	for (int x = leaf_level; x >= 0; x--) {
		buffpagenum[x+1] = buffpagenum[x];
		buffpage[x+1] = buffpage[x];
	}
	buffpagenum[0] = pnum;
	buffpage[0] = bh;

	//Rejig field attributes to point at the new root page
	pfi->btree_root = pnum;
	file->GetFieldMgr()->UpdateBTreeRootPage(dbapi, pfi);

	//Store an initial branch pointer to the old root which is now just another node.
	//Note that the leftmost leaf or INode is always associated with the minimum value,
	//since we always split nodes to the right.
	FieldValue loval;
	if (FieldIsOrdNum())
		loval = RangeCheckedDouble::MAXIMUM_NEGATIVE_VALUE;

	short valpos = -1;
	short datapos;
	ptnewroot.BL_InsertValue(loval, valpos, datapos);
	ptnewroot.B_SetChildPageNum(datapos, buffpagenum[1]);
}

//****************************************************************************************
void BTreeAPI::RemoveValueEntry()
{
	BTreePage pt(buffpage[leaf_level]);

	//No problems if other values remain on the page
	bool empty_page = pt.BL_RemoveValue(leaf_value_offset);

	file->IncStatBXDEL(dbapi);
	pt.L_IncTStamp();

	if (!empty_page)
		return;

	int right_sibling = pt.L_GetRightSibling();
	int left_sibling = pt.L_GetLeftSibling();

	//See tech docs.  A leftmost leaf is left in place even if empty.  Conceptually
	//ugly, but makes things simpler in a lot of areas.
	if (left_sibling == -1 && right_sibling != -1) {
		//Could optionally update tree low ptr to next page lowval (see H_SetLowestValue())
		return;
	}

	//Remove the page altogether - first splice right and left siblings together
	if (right_sibling != -1) {
		BufferPageHandle bhrsib = tdmgr->GetTableDPage(dbapi, right_sibling);
		BTreePage ptrsib(bhrsib);
		ptrsib.L_SetLeftSibling(left_sibling);
	}

	bool remove_final_empty_leaf = false;
	if (left_sibling != -1) {
		BufferPageHandle bhlsib = tdmgr->GetTableDPage(dbapi, left_sibling);
		BTreePage ptlsib(bhlsib);
		ptlsib.L_SetRightSibling(right_sibling);

		if (right_sibling == -1 && ptlsib.MapNumVals() == 0) 
			remove_final_empty_leaf = true;
	}

	//Delete the empty page
	tdmgr->ReturnPage(buffpage[leaf_level], buffpagenum[leaf_level]);

	//Remove branch from its parent
	if (leaf_level != 0)
		RemoveBranchPointer(leaf_level - 1);

	//Or if the root, remove pointer from the field attribute page
	else {
		pfi->btree_root = -1;
		file->GetFieldMgr()->UpdateBTreeRootPage(dbapi, pfi);
	}

	file->IncStatBXFREE(dbapi);

	//This will clear the tree down completely when it's empty
	if (remove_final_empty_leaf) {

		//We can easily populate the "chain" by doing a dummy search now.
		if (FieldIsOrdNum()) 
			LocateValueEntry(0.0);
		else
			LocateValueEntry("anything");

		for (int x = leaf_level; x >= 0; x--) {
			tdmgr->ReturnPage(context->DBAPI(), buffpagenum[x]);
			file->IncStatBXFREE(dbapi);
		}

		pfi->btree_root = -1;
		file->GetFieldMgr()->UpdateBTreeRootPage(dbapi, pfi);
	}
}

//****************************************************************************************
void BTreeAPI::RemoveBranchPointer(short lev)
{
	BTreePage pt(buffpage[lev]);

	//There must be an appropriate branch pointer if we get to here
	short valpos, datapos;
	if (!pt.B_LocateValueLE(last_value_located, valpos, datapos))
		throw Exception(DB_STRUCTURE_BUG, "Bug: btree branch entry is missing during remove");

	//No further problems if other values remain on the page
	bool empty_page = pt.BL_RemoveValue(valpos);
	if (!empty_page)
		return;

	//Delete the empty page
	tdmgr->ReturnPage(buffpage[lev], buffpagenum[lev]);

	//Remove branch from its parent
	if (lev != 0)
		RemoveBranchPointer(lev - 1);

	//Or if the root, remove pointer from the field attribute page
	else {
		pfi->btree_root = -1;
		file->GetFieldMgr()->UpdateBTreeRootPage(dbapi, pfi);
	}

	//Branch page deletions count towards this too
	file->IncStatBXFREE(dbapi);
}

//****************************************************************************************
//Used in DELETE FIELD and when REDEFINEing from one index type to another
void BTreeAPI::DeleteAllNodes()
{
	if (!LocateLowestValueEntryPreBrowse())
		return;

	//Start at root level and work towards leaves, deleting whole pages at a time.
	std::vector<int> pagelist;
	pagelist.push_back(pfi->btree_root);
	DeleteAllNodes_1Level(&pagelist);

	//V3.0. for reorg OI.  In redefine this wasn't a problem because new was assumed.
	pfi->btree_root = -1;
	file->GetFieldMgr()->UpdateBTreeRootPage(dbapi, pfi);
}

//****************************************************************************************
void BTreeAPI::DeleteAllNodes_1Level(std::vector<int>* pagelist)
{
	bool at_leaf_level = false;
	std::vector<int> next_level_pagelist;

	//Pass across pages in the current level
	for (size_t x = 0; x < pagelist->size(); x++) {
		int pnum = (*pagelist)[x];

		BufferPageHandle bh = tdmgr->GetTableDPage(dbapi, pnum);
		BTreePage pt(bh);
		at_leaf_level = pt.is_leaf;

		//Add child pages of this page to a running collection for the next level 
		if (!at_leaf_level) {
			int current_level_children = next_level_pagelist.size();

			next_level_pagelist.resize(current_level_children + pt.MapNumVals());

			short value_offset = -1;
			short data_offset;
			int x = current_level_children;

			while (pt.BL_LocateNextValue(value_offset, value_offset, data_offset)) {
				pt.BL_GetData(data_offset, next_level_pagelist[x], NULL);
				x++;
			}
		}

		//Now we can delete this leaf
		tdmgr->ReturnPage(bh, pnum);
	}

	//Move down to process all the next level pages, as collected above
	if (!at_leaf_level)
		DeleteAllNodes_1Level(&next_level_pagelist);
}




//****************************************************************************************
//Diagnostics
//****************************************************************************************
double BTreeAPI::CurrentLeafFree()
{
	if (!AtLeaf())
		return 0.0;

	BTreePage pt(buffpage[leaf_level]);
	return pt.BL_FreeBytes();
}

//****************************************************************************************
bool BTreeAPI::FieldIsOrdNum()
{
	return pfi->atts.IsOrdNum();
}



//****************************************************************************************
//****************************************************************************************
//****************************************************************************************
void BTreeAPI::Analyze1
(BTreeAnalyzeInfo* btinfo, InvertedListAnalyze1Info* ilinfo, 
 SingleDatabaseFileContext* sfc, PhysicalFieldInfo* pfi, bool descending)
{
	btinfo->numeric = pfi->atts.IsOrdNum();

	bool gotval;
	if (descending)
		gotval = LocateHighestValueEntryPreBrowse();
	else
		gotval = LocateLowestValueEntryPreBrowse();

	if (!gotval) 
		return;

	btinfo->rootpage = pfi->btree_root;
	btinfo->depth = Depth();

	//Walk values until there are no more
	btinfo->leafpage_tot = 1;
	double leafpage_totfree = CurrentLeafFree();

	int currpage = CurrentLeafPageNum();
	for (;;) {
		btinfo->totvals++;
		InvertedListAPI il = InvertedList();
		il.Analyze(ilinfo);

		//Get the next/prev value
		if (descending)
			gotval = WalkToPreviousValueEntry();
		else
			gotval = WalkToNextValueEntry();

		if (!gotval)
			break;

		//For each different leaf page note page stats
		int p = CurrentLeafPageNum();
		if (p != currpage) {
			currpage = p;
			btinfo->leafpage_tot++;
			leafpage_totfree += CurrentLeafFree();
		}
	}

	//Calculate average free space from the total
	btinfo->leafpage_avefree_frac = 
		(leafpage_totfree / DBPAGE_SIZE) / btinfo->leafpage_tot;

	//------------------------------
	//Next, because the above value walk only touches the leaves, we call this 
	//special function to obtain freespace information about branch areas.
	double branchpage_totfree = 0;
	btinfo->branchpage_tot = BranchAnalysis(branchpage_totfree, descending);

	if (btinfo->branchpage_tot > 0) 
		btinfo->branchpage_avefree_frac = 
			(branchpage_totfree / DBPAGE_SIZE) / btinfo->branchpage_tot;
	else
		btinfo->branchpage_avefree_frac = 0; //avoid DBZ
}

//****************************************************************************************
int BTreeAPI::BranchAnalysis(double& totfree, bool descending)
{
	//The return code is the number of branches processed
	if (pfi->btree_root == -1)
		return 0;
	else
		return BranchAnalysis_1Node(pfi->btree_root, totfree, descending);
}

//****************************************************************************************
int BTreeAPI::BranchAnalysis_1Node(int pagenum, double& totfree, bool descending)
{
	//Retrieve and analyze this page
	BufferPageHandle bh = tdmgr->GetTableDPage(dbapi, pagenum);
	BTreePage pt(bh);

	if (pt.is_leaf)
		return 0;

	//It's a branch node
	int branches = 1;
	totfree += pt.BL_FreeBytes();

	//So check out all its children (there must be at least one)
	short value_offset = -1;
	short pagenum_offset = -1;
	
	for (;;) {
		bool gotchild;
		if (descending)
			gotchild = pt.BL_LocatePreviousValue(value_offset, value_offset, pagenum_offset);
		else
			gotchild = pt.BL_LocateNextValue(value_offset, value_offset, pagenum_offset);

		//No more children
		if (!gotchild)
			break;

		pt.BL_GetData(pagenum_offset, pagenum, NULL);

		//Recursively process the child.  If the child is a leaf we can give up on
		//this whole loop because all its siblings will be leaves too.
		int subtree_branches = BranchAnalysis_1Node(pagenum, totfree, descending);
		if (subtree_branches == 0)
			break;

		//Go round and process all the siblings
		branches += subtree_branches;
	}

	return branches;
}

//****************************************************************************************
//****************************************************************************************
//****************************************************************************************
void BTreeAPI::Analyze3(SingleDatabaseFileContext* sfc, BB_OPDEVICE* op, 
 PhysicalFieldInfo* pfi, bool leaves_only, bool descending)
{
	op->Write("B-tree page details for ");
	op->Write(pfi->name);
	op->Write(" (ORD ");
	op->WriteLine( (pfi->atts.IsOrdNum()) ? "NUM)" : "CHAR)");
	op->WriteLine("");

	//The return code is the number of branches processed
	if (pfi->btree_root == -1) {
		op->WriteLine("Tree is empty");
		return;
	}

	std::vector<int> pagelist;
	pagelist.push_back(pfi->btree_root);
	Analyze3_1Level(sfc, op, leaves_only, descending, 1, &pagelist);
}

//****************************************************************************************
void BTreeAPI::Analyze3_1Level
(SingleDatabaseFileContext* sfc, BB_OPDEVICE* op, 
 bool leaves_only, bool descending, int level, std::vector<int>* pagelist)
{
	DatabaseFile* file = sfc->GetDBFile();
	DatabaseFileTableDManager* tdmgr = file->GetTableDMgr();
	DatabaseServices* dbapi = sfc->DBAPI();

	op->Write("Level ");
	op->Write(util::IntToString(level));
	op->Write(" (");
	if (level == 1)
		op->Write("Root+");

	//Test the first page to see whether we're at the leaf level yet
	BufferPageHandle bh0 = tdmgr->GetTableDPage(dbapi, (*pagelist)[0]);
	BTreePage pt0(bh0);
	bool at_leaf_level = pt0.is_leaf;

	if (at_leaf_level)
		op->WriteLine("Leaf)");
	else
		op->WriteLine("Branch)");

	std::vector<int> next_level_pagelist;

	//Show details of all pages on this level before moving to the next
	if (at_leaf_level || !leaves_only) {

		op->Write("  D Page    Free%  #Vals  ");
		if (at_leaf_level)
			op->Write("First value             Last value              Siblings");
		else
			op->Write("First GE branch         Last GE branch");
		op->WriteLine("");

		op->Write("  --------  -----  -----  ");
		if (at_leaf_level)
			op->Write("----------------------  ----------------------  --------");
		else
			op->Write("----------------------  ----------------------");
		op->WriteLine("");
	}
	else if (!at_leaf_level) {
		op->WriteLine("  -* Leaf-only report requested *-");
	}

	//Iterate
	for (size_t x = 0; x < pagelist->size(); x++) {
		int pnum = (*pagelist)[x];

		BufferPageHandle bh = tdmgr->GetTableDPage(dbapi, pnum);
		BTreePage pt(bh);

		//Print page info if desired
		if (at_leaf_level || !leaves_only) {
			//op->Write("  ");
			op->Write(util::SpacePad(pnum, 10, true));
			op->Write("  ");

			double freebytes = pt.BL_FreeBytes();
			freebytes /= DBPAGE_SIZE;
			freebytes *= 100;
			RangeCheckedDouble fbrcd(freebytes);
			op->Write(util::PadRight(fbrcd.ToStringWithFixedDP(2), ' ', 5));
			op->Write("  ");

			op->Write(util::SpacePad(pt.MapNumVals(), 5, true));
			op->Write("  ");

			//The leftmost node may now be empty
			if (pt.MapNumVals() == 0) {
				op->Write("n/a                     n/a                     ");
			}

			else {
				short value_offset;
				short data_offset;
				pt.BL_LocateNextValue(-1, value_offset, data_offset);

				FieldValue v;
				pt.BL_GetValue(value_offset, v);

				std::string s = pt.BL_FormatDumpDisplayString(value_offset, 22);
				op->Write(s);
				op->Write("  ");

				pt.BL_LocatePreviousValue(-1, value_offset, data_offset);
				s = pt.BL_FormatDumpDisplayString(value_offset, 22);
				op->Write(s);
				op->Write("  ");
			}

			if (pt.is_leaf) {
				op->Write(util::IntToString(pt.L_GetLeftSibling()));
				op->Write("/");
				op->Write(util::IntToString(pt.L_GetRightSibling()));
			}
			
			op->WriteLine("");
		}

		//Collect child pages of this page for the next level 
		if (!at_leaf_level) {
			int current_level_children = next_level_pagelist.size();

			next_level_pagelist.resize(current_level_children + pt.MapNumVals());

			short value_offset = -1;
			short data_offset;
			int x = current_level_children;

			while (pt.BL_LocateNextValue(value_offset, value_offset, data_offset)) {
				pt.BL_GetData(data_offset, next_level_pagelist[x], NULL);
				x++;
			}
		}
	}

	op->WriteLine("");

	//Recursively process the next level
	if (!at_leaf_level)
		Analyze3_1Level(sfc, op, leaves_only, descending, level+1, &next_level_pagelist);
}





//****************************************************************************************
//The idea here is to simply return the same leaf page as the last value was added
//to, *until* a value is received out of order.  Then revert to normal finding.
//****************************************************************************************
bool BTreeAPI_Load::LocateValueEntry(const FieldValue& value, bool, bool)
{
	//We may start loading into an existing index - it's OK if we continue at the end.
	if (status == NOT_INIT) {
		status = IN_ORDER;

		//Empty index
		if (!LocateHighestValueEntryPreBrowse()) {
			prev_value = value;
			return false;
		}

		//Proceed as if we loaded the old (pre-load) high value last time
		GetLastValueLocated(prev_value);
	}

	//If our value is a new highest, we already have the correct leaf page to insert on...
	if (status == IN_ORDER) {
		if (value.Compare(prev_value) > 0) {
			prev_value = value;

			//...plus no need to scan page from the start - just walk past prev value
			BTreePage pt(buffpage[leaf_level]);
			pt.BL_LocateNextValue(leaf_value_offset, leaf_value_offset, leaf_ilmr_offset);

			return false;
		}
	}

	//Values appearing out of order, so revert to full root scan from now on.
	status = UNORDERED;
	return BTreeAPI::LocateValueEntry(value);
}

//**************************************************
void BTreeAPI_Load::Initialize(SingleDatabaseFileContext* sfc, PhysicalFieldInfo* p)
{
	//This object might be used for several fields, so release buffers for prev field
	InitializeCache();

	status = NOT_INIT;
	BTreeAPI::Initialize(sfc, p);
}

//**************************************************
//V2.19 June 2009.  Used during redefine if index collating order changes
void BTreeAPI_Load::BuildFromExtract(BTreeExtract& extract)
{
	bool numeric = extract.IsNumeric();

	for (size_t x = 0; x < extract.data.size(); x++) {
		BTreeExtract::Entry& e = extract.data[x];
		const FieldValue& value = e.val;

		//Add a new entry on the btree leaf
		InsertValueEntry(value);

		//Attach the ILMR
		BufferPageHandle* buff = &buffpage[leaf_level];
		BTreePage pt(*buff);
		pt.BL_WriteData_NoMove(leaf_ilmr_offset, e.idata, e.sdata);

		//V2.23 Oct 09.  While we're in position in the btree, merge invlists 
		//for upcoming same values.  (Can only happen in char->num case).
		if (numeric) {
			while ( (x+1) < extract.data.size()) {
				BTreeExtract::Entry& enext = extract.data[x+1];
				const FieldValue& valnext = enext.val;

				if (valnext.Compare(value) != 0)
					break;

				short abspagepos = pt.BL_PagePos(leaf_ilmr_offset);

				//Retrieve existing set and delete its disk copy
				BitMappedFileRecordSet oldset(context);
				BitMappedFileRecordSet* poldset = &oldset;

				InvertedListAPI ilcurr(context, buff, abspagepos);
				ilcurr.ReplaceRecordSet(NULL, &poldset);

				//Attach next ILMR to the btree entry and add in the accumulated set
				pt.BL_WriteData_NoMove(leaf_ilmr_offset, enext.idata, enext.sdata);

				//(Make new IL object to take in new ILMR pointers)
				InvertedListAPI ilnext(context, buff, abspagepos);
				ilnext.AugmentRecordSet(poldset, NULL);

				//See if yet more for the same value
				x++;
			}
		}

		//Finally reposition at the right hand side ready for the next value
		pt.BL_LocateNextValue(leaf_value_offset, leaf_value_offset, leaf_ilmr_offset);
	}
}





//****************************************************************************************
//Utility class
//****************************************************************************************
BTreeExtract::BTreeExtract(SingleDatabaseFileContext* sfc, PhysicalFieldInfo* pfi)
{
	data.reserve(8192);

	BTreeAPI bt(sfc, pfi);

	for (bt.LocateLowestValueEntryPreBrowse(); 
		 bt.LastLocateSuccessful(); 
		 bt.WalkToNextValueEntry()) 
	{
		BTreePage pt(bt.buffpage[bt.leaf_level]);

		int idata;
		short sdata;
		pt.BL_GetData(bt.leaf_ilmr_offset, idata, &sdata);

		data.push_back(Entry(bt.last_value_located, idata, sdata));
	}

	numeric = pfi->atts.IsOrdNum();
}

//**************************************************
void BTreeExtract::ConvertAndSort(bool throw_badnums)
{
	if (data.size() > 0) {

		for (size_t x = 0; x < data.size(); x++) {
			FieldValue& v = data[x].val;

			if (numeric)
				v.ConvertToString();
			else
				v.ConvertToNumeric(throw_badnums);
		}

		std::sort(data.begin(), data.end());
	}

	numeric = !numeric;
}

} //close namespace

