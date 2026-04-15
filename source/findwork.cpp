
#include "stdafx.h"

#include "findwork.h"

#include <functional>
#include <algorithm>

//Utils
#include "dataconv.h"
#include "pattern.h"
#include "lineio.h"
//API Tiers
#include "bmset.h"
#include "findspec.h"
#include "dbf_field.h"
#include "dbfile.h"
#include "dbctxt.h"
//Diagnostics
#include "except.h"
#include "msg_db.h"

namespace dpt {

//************************************************************************************
void FindWorkNode::Adopt(FindWorkNode_Combo* newparent)
{
	parent = newparent;
	PromoteLevel();
}

//************************************************************************************
FindWorkNode_Combo::FindWorkNode_Combo
(FindWorkInfo* fwi, FindWorkNode_Combo* parent, SingleDatabaseFileContext* sfc, 
 FindSpecNode_Leaf* sololeaf)
: FindWorkNode(parent), node_and(true), node_root(true)
{
	Init();
	NewChild(fwi, sfc, sololeaf, 0);
}

//************************************************************************************
FindWorkNode_Combo::FindWorkNode_Combo
(FindWorkInfo* fwi, FindWorkNode_Combo* parent, SingleDatabaseFileContext* sfc, 
 FindSpecNode_Boolean* fromnode, int depth)
: FindWorkNode(parent), node_and(fromnode->node_and), node_root(depth == 0)
{
	Init();
	NewChild(fwi, sfc, fromnode->node1, depth);
	NewChild(fwi, sfc, fromnode->node2, depth);
}

//************************************************************************************
FindWorkNode_Combo::FindWorkNode_Combo
(FindWorkInfo* fwi, FindWorkNode_Combo* parent, SingleDatabaseFileContext* sfc, 
 const FindSpecification* spec)
: FindWorkNode(parent), node_and(true), node_root(true)
{
	Init();
	NewChild(fwi, sfc, spec->rootnode, 0);
}

//************************************************************************************
void FindWorkNode_Combo::NewChild
(FindWorkInfo* fwi, SingleDatabaseFileContext* sfc, FindSpecNode* source, int depth)
{
	FindWorkNode* child = NULL;

	try {
		FindSpecNode_Boolean* combo = source->CastToBoolean();
		if (combo)
			child = new FindWorkNode_Combo(fwi, this, sfc, combo, depth+1);
		else
			child = new FindWorkNode_Leaf(fwi, this, sfc, source->CastToLeaf(), depth);

		children.push_back(child);
	}
	catch (...) {
		if (child)
			delete child;
		throw;
	}
}

//************************************************************************************
void FindWorkNode_Combo::Collapse()
{
	//Original left sibling
	CollapseChild(0);						
	
	//This happens when the original spec root was a single criterion, or an OR node 
	//which would have been made into a single child of an inserted root AND node (the
	//root of the work tree is always AND)
	if (children.size() == 1)
		return;

	//Because of the way the child-collapsing function (below) keeps the node "reading
	//order" the same, the original right sibling may now be, say, sibling 5.
	CollapseChild(children.size() - 1);
}

//************************************************************************************
void FindWorkNode_Combo::CollapseChild(int childid)
{
	FindWorkNode_Combo* child = children[childid]->CastToCombo();
	if (!child)
		return;

	//Recursion - the algorithm will work from the leaves up
	child->Collapse();

	//Child type must be different to survive
	if (child->IsAndNode() != node_and)
		return;

	//Same type - adopt grandchildren
	int num_grand_children = child->children.size();
	if (num_grand_children == 0)
		throw Exception(BUG_MISC, "Bug: No grandchildren in FWNC::Collapse");

	//Prepare beds now in case of mem problems.  It results in the most intuitive-looking
	//tree if the grandchildren are squeezed in at the place their parent was.  Having
	//said that the actual order of processing still depends on the search types.
	int old_num_children = children.size();
	int new_num_children = old_num_children + num_grand_children - 1;
	children.resize(new_num_children, NULL);
	for (int cid = old_num_children-1; cid > childid; cid--)
		children[cid + num_grand_children - 1] = children[cid];

	for (int gcid = 0; gcid < num_grand_children; gcid++) {
		int bedslot = childid + gcid;

		//Make the physical change
		FindWorkNode* grandchild = child->children[gcid];
		child->children[gcid] = NULL;
		children[bedslot] = grandchild;

		//Tell our new child to forget its old parent
		grandchild->Adopt(this);
	}

	//Finally kill the redundant generation
	delete child;
}

//************************************************************************************
void FindWorkNode_Combo::PromoteLevel()
{
	for (size_t x = 0; x < children.size(); x++)
		children[x]->PromoteLevel();
}

//************************************************************************************
FindWorkNode_Combo::~FindWorkNode_Combo()
{
	if (working_set)
		delete working_set;

	for (size_t x = 0; x < children.size(); x++)
		if (children[x])
			delete children[x];
}

//************************************************************************************
void FindWorkNode_Combo::SetWorkingSet
(BitMappedFileRecordSet* s, bool copydown, util::LineOutput* pop)
{
	//I don't think there would be any problems using this at other levels, but it's
	//here initially to set the root set at certain key stages (post EBP etc.)
	assert(node_root);

	working_set = s;

	//There are times we know that there will be little worth doing further down
	//the tree, so skip the fruitless tree scan in those cases.
	if (copydown && !IsComplete()) {
		RegisterImprovedWorkingSet(pop);
	}
}

//************************************************************************************
void FindWorkNode_Combo::GetUnprocessedLeavesByType(std::vector<FindWorkNode_Leaf*>* leaves, 
 FindOperator reqd_op, bool reqd_ix) 
{
	for (size_t x = 0; x < children.size(); x++) {
		FindWorkNode* n = children[x];

		//Get local child leaves and all grand^n child leaves - will be sorted afterwards
		FindWorkNode_Combo* nc = n->CastToCombo();
		if (nc)
			nc->GetUnprocessedLeavesByType(leaves, reqd_op, reqd_ix);

		else {
			FindWorkNode_Leaf* nl = n->CastToLeaf();
			if (!nl->IsComplete() && nl->IsDesired(reqd_op, reqd_ix))
				leaves->push_back(nl);
		}
	}
}

//************************************************************************************
BitMappedFileRecordSet* FindWorkNode_Combo::RestrictingSet()
{
	//OR nodes maintain an increasingly large set - refer up to the next AND node
	if (!node_and)
		return Parent()->RestrictingSet();

	//If any criteria have happened so far on an AND node, that's good - use it.
	//Otherwise we'll look further up the tree for the nearest such node.
	if (working_set)
		return working_set;

	//If no parent nodes at all have a base set yet, we simply can't restrict
	if (node_root)
		return NULL;

	//May as well save a couple of ticks by cutting out the intervening OR node
	return Parent()->Parent()->RestrictingSet();
}

//************************************************************************************
void FindWorkNode_Combo::RegisterCompleteNodeSet(util::LineOutput* pop)
{
	if (!Parent())
		return;

	//Treat a complete node just like a leaf.
	BitMappedFileRecordSet* temp = working_set;
	working_set = NULL;							//ensure no double delete

	Parent()->CopyUpCompleteChildSet(temp, pop);
	Parent()->IncrementCompleteCount(pop);
}

//************************************************************************************
void FindWorkNode_Combo::CopyUpCompleteChildSet
(BitMappedFileRecordSet* childset, util::LineOutput* pop)
{
	if (pop)
		pop->WriteLine(std::string("    Incorporating complete child of ").append(PtrString()));

	//After the first child of any combo node, just adopt its set
	if (!working_set)
		working_set = childset;

	else {
		try {
			//Refine the working set for this node
			if (node_and)
				working_set = BitMappedFileRecordSet::BitAnd(working_set, childset);
			else
				working_set->BitOr(childset);
			
			if (childset)
				delete childset;
		}
		catch (...) {
			if (childset)
				delete childset;
			throw;
		}
	}

	//Pass down the benefits of set restriction on an AND node
	if (node_and)
		RegisterImprovedWorkingSet(pop);
}

//************************************************************************************
void FindWorkNode_Combo::IncrementCompleteCount(util::LineOutput* pop)
{
	if (!IsComplete()) {
		complete_subcount++;
		if ((size_t)complete_subcount == children.size()) {
			if (pop) {
				pop->WriteLine(std::string("    Node is complete (all children complete) ")
					.append(PtrString()));
			}

			SetCompleteFlag();
			RegisterCompleteNodeSet(pop);
		}
	}
}

//************************************************************************************
void FindWorkNode_Combo::ForceComplete(util::LineOutput* pop)
{
	if (IsComplete())
		return;

	if (pop)
		pop->WriteLine(std::string("    Node forced complete ").append(PtrString()));

	//No need for these any more
	if (working_set) {
		delete working_set;
		working_set = NULL;
	}

	for (size_t x = 0; x < children.size(); x++)
		children[x]->ForceComplete(pop);

	complete_subcount = children.size();
	SetCompleteFlag();
}

//************************************************************************************
void FindWorkNode_Combo::RegisterImprovedWorkingSet(util::LineOutput* pop)
{
	//Some records remain, but still possible benefit to children
	if (working_set)
		SegmentRestrict(NULL, pop);

	//All records ANDed away - great news!  All children now irrelevant.
	else {
		if (pop) {
			pop->WriteLine(std::string("    All records ANDed away - forcing node complete ")
								.append(PtrString()));
		}

		ForceComplete(pop);
		RegisterCompleteNodeSet(pop);
	}
}

//************************************************************************************
void FindWorkNode_Combo::SegmentRestrict
(BitMappedFileRecordSet* parent_wkset, util::LineOutput* pop)
{
	if (pop)
		pop->WriteLine(std::string("    Segment-restricting node ").append(PtrString()));

	//Unstarted nodes will pick up the set as a base set later on, so skip here.
	//Also skip the originating node (would have no effect) - NULL parm means that.
	if (working_set && parent_wkset) {

		//Could do full BitAnd here but that will happen anyway when the node completes,
		//so there's no need.  Segment restricting is what delivers all the benefits - 
		//both less disk reads and less bitmap ANDd/OR work for later criteria.
		working_set = BitMappedFileRecordSet::SegmentAnd(working_set, parent_wkset);

		//Even just segment restricting could take away all our records
		if (node_and && !working_set) {
			if (pop) {
				pop->WriteLine(std::string("    No records remain - forcing complete ")
									.append(PtrString()));
			}

			ForceComplete(pop);
			RegisterCompleteNodeSet(pop);
			return;
		}
	}

	//Also pass down to the children
	BitMappedFileRecordSet* passdown_set = parent_wkset;

	//May as well further restrict if possible.  Note that we may pass through several 
	//levels of unstarted AND nodes before reaching a lower part-complete one.
	if (node_and && working_set) 
		passdown_set = working_set;

	for (size_t x = 0; x < children.size(); x++) {
		FindWorkNode_Combo* nc = children[x]->CastToCombo();
		if (nc && !nc->IsComplete()) {
			if (pop)
				pop->WriteLine("    Child...");

			nc->SegmentRestrict(passdown_set, pop);
		}
	}
}





//************************************************************************************
//************************************************************************************
//************************************************************************************
FindWorkNode_Leaf::FindWorkNode_Leaf
(FindWorkInfo* fwi, FindWorkNode_Combo* parent, SingleDatabaseFileContext* sfc, 
 FindSpecNode_Leaf* spec, int d) 
: FindWorkNode(parent), tree_depth(d),
  basic_op(spec->basic_op), operand1(spec->operand1), operand2(spec->operand2),
  pfi(NULL), positive(spec->positive), force_alpha(spec->force_alpha), force_num(spec->force_num),
  will_use_index(false), tbs_required(false), tbs_pattern(NULL)
{
	if (basic_op == FD_FILE$)
		operand1.ConvertToString();

	else if (basic_op == FD_POINT$ || basic_op == FD_SET$ || basic_op == FD_SINGLEREC)
		operand1.ConvertToNumeric();

	//----------------------------------------------
	//Field-based operators - check atts and see if what's requested is possible
	if (FOIsField(basic_op)) {
		std::string& fieldname = spec->fieldname;
		pfi = sfc->GetDBFile()->GetFieldMgr()->GetPhysicalFieldInfo(sfc, fieldname);

		//This always requires a table B search
		if (basic_op == FD_PRESENT)
			tbs_required = true;

		//For all other operators we stand a chance of using an index.  Unless the user
		//forces the issue, the existing index is the default one to use.
		else if (pfi->atts.IsOrdChar()) {
			if (force_num)
				FlagForTBS(true);
			else
				FlagForIndexed();
		}

		//ORD NUM is slightly less functional as it can't do pattern matching
		else if (pfi->atts.IsOrdNum()) {
			if (force_alpha || FOIsPattern(basic_op))
				FlagForTBS(false);
			else
				FlagForIndexed();
		}

		//No index, so table B search by default
		else {
			bool numcomp;
			//Need to check this on M204
				//Manual p4-16 has some interesting comments - not sure if correct though.
				//I've decided for now that it's nicest if we do a string comparison if
				//the user gave a string and didn't otherwise force.  My previous thinking
				//was to make it like an IF test, where either side numeric forces numeric.
//			if (force_num || operand1.CurrentlyNumeric() || pfi->atts.IsFloat())
			if (force_num || operand1.CurrentlyNumeric())
				numcomp = true;
			else
				numcomp = false;
			
			FlagForTBS(numcomp);
		}

		//Anything above forcing a table B search? 
		if (tbs_required) {
			
			//If so the field will have to be visible
			if (pfi->atts.IsInvisible()) {
				throw Exception(DML_TABLEB_BADSPEC, 
					std::string("Table B search required but field is invisible: ")
					.append(fieldname));
			}

			if (FOIsPattern(basic_op))
				tbs_pattern = new util::Pattern(operand1.ExtractString());
		}
	}

	//Set some flags in the overal containter obejct to save scanning later
	fwi->SetLeafFlags(basic_op, will_use_index, tbs_required);
}

//************************************************************************************
void FindWorkNode_Leaf::FlagForTBS(bool numcomp)
{
	tbs_required = true;
	tbs_compare_num = numcomp;

	if (tbs_compare_num) {
		operand1.ConvertToNumeric();
		if (FOIsRange2(basic_op))
			operand2.ConvertToNumeric();
	}
	else {
		operand1.ConvertToString();
		if (FOIsRange2(basic_op))
			operand2.ConvertToString();
	}
}

//************************************************************************************
FindWorkNode_Leaf::~FindWorkNode_Leaf()
{
	if (tbs_pattern)
		delete tbs_pattern;
}

//************************************************************************************
bool FindWorkNode_Leaf::IsDesired(FindOperator o, bool want_to_use_index)
{
	if (o == FD_FILE$) 
		return (basic_op == FD_FILE$ || basic_op == FD_ALLRECS);

	//All types of indexed ranges and patterns
	if (o == FD_RANGE) 
		return (will_use_index && FOIsOrdered(basic_op));
	
	//Table B search phase - return all incomplete nodes
	if (o == FD_NULLOP) 
		return true;

	//Non field based operators
	if (FONonField(basic_op))
		return (o == basic_op);

	//Everything not dealt with above
	return (o == basic_op && want_to_use_index == will_use_index);
}

//************************************************************************************
void FindWorkNode_Leaf::RegisterLeafSet(BitMappedFileRecordSet* leafset, util::LineOutput* pop)
{
	SetCompleteFlag();
	Parent()->CopyUpCompleteChildSet(leafset, pop);
	Parent()->IncrementCompleteCount(pop);
}

//************************************************************************************
//Special version for ALLRECS which is common because it's used for an unadorned FD
//************************************************************************************
void FindWorkNode_Leaf::RegisterWholeFileLeafSet(util::LineOutput* pop)
{
	SetCompleteFlag();

	//There should be a base set by this time since we leave these criteria
	//till after the EBP has been retrieved.
	BitMappedFileRecordSet* base_set = RestrictingSet();
	if (!base_set)
		throw Exception(DB_ALGORITHM_BUG, "RAFS: no base set");

	//The reason for having this special function is to save creating a whole copy
	//just to then AND/OR it with itself (waste of time) in this fairly common case.
	if (Parent()->WorkingSet() != base_set) {
		BitMappedFileRecordSet* base_copy = base_set->MakeCopy();
		Parent()->CopyUpCompleteChildSet(base_copy, pop);
	}
	
	Parent()->IncrementCompleteCount(pop);
}

//************************************************************************************
bool FindWorkNode_Leaf::TableBSearchPositiveTestField(FieldValue& fval)
{
	if (basic_op == FD_PRESENT)
		return true;

	if (FOIsPattern(basic_op)) {
		bool lk = tbs_pattern->IsLike(fval.ExtractString());
		return (basic_op == FD_LIKE) ? lk : !lk;
	}

	if (tbs_compare_num)
		fval.ConvertToNumeric();
	else
		fval.ConvertToString();

	int cmp1, cmp2;
	cmp1 = fval.Compare(operand1);
	if (FOIsRange2(basic_op))
		cmp2 = fval.Compare(operand2);

	switch (basic_op) {
	case FD_EQ:
		return (cmp1 == 0);
	case FD_GT:
		return (cmp1 > 0);
	case FD_LT:	
		return (cmp1 < 0);
	case FD_GE:
		return (cmp1 >= 0);
	case FD_LE:
		return (cmp1 <= 0);
	case FD_RANGE_GE_LE:
		return (cmp1 >= 0 && cmp2 <= 0);
	case FD_RANGE_GT_LE:
		return (cmp1 > 0 && cmp2 <= 0);
	case FD_RANGE_GE_LT:
		return (cmp1 >= 0 && cmp2 < 0);
	case FD_RANGE_GT_LT:
		return (cmp1 > 0 && cmp2 < 0);
	default:
		throw "TBSTF: bug if you see this";
	};
}






//************************************************************************************
//************************************************************************************
//************************************************************************************
FindWorkInfo::FindWorkInfo(SingleDatabaseFileContext* sfc, const FindSpecification& spec)
: early_ebp_flag(false), index_flag(false), tbs_flag(false)
{
	memset(op_flags, 0, FD_HI_BASIC_OPTR+1);

	//See tech docs for more on the why of this process.  The how is fairly simple.
	FindSpecNode_Boolean* srb = spec.rootnode->CastToBoolean();
	if (!srb)
		//Single criterion
		root = new FindWorkNode_Combo(this, NULL, sfc, spec.rootnode->CastToLeaf());
	else if (srb->node_and)
		//Root was AND
		root = new FindWorkNode_Combo(this, NULL, sfc, srb, 0);
	else
		//Root was OR - insert an extra AND node above it
		root = new FindWorkNode_Combo(this, NULL, sfc, &spec);

	//Collapse vertically-adjacent AND/OR nodes
	root->Collapse();
}

//************************************************************************************
void FindWorkInfo::SetLeafFlags(FindOperator bop, bool will_use_index, bool tbs_required)
{
	if (FOIsOrdered(bop))
		op_flags[FD_RANGE] = true;
	else if (bop == FD_ALLRECS)
		op_flags[FD_FILE$] = true;
	else
		op_flags[bop] = true;

	if (will_use_index)
		index_flag = true;
	else if (tbs_required) {
		tbs_flag = true;
		op_flags[FD_NULLOP] = true;
	}
}

//************************************************************************************
//This is how we ensure that equivalent UL find expression syntaxes never
//return different results.  Sort by tree depth and then field name.
//************************************************************************************
struct LeafInfoLessThanPredicate
: public std::binary_function<FindWorkNode_Leaf*, FindWorkNode_Leaf*, bool> 
{
	bool operator()(const FindWorkNode_Leaf* lhs, const FindWorkNode_Leaf* rhs) {
		
		//Criteria higher in the tree always get processed first
		if (lhs->TreeDepth() != rhs->TreeDepth())
			return (lhs->TreeDepth() < rhs->TreeDepth());

		//Process non-negated ones first - see tech docs
		if (lhs->Positive() != rhs->Positive())
			return lhs->Positive();

		//Criteria at a particular level might appear in various orders depending on
		//exactly how the user gives them.  Therefore sort by field id.  It's arbitrary
		//really - see tech docs - and this is the quickest to sort by.
		if (lhs->PFI())
			return (lhs->PFI()->id < rhs->PFI()->id);
	//		return (lhs->PFI()->name < rhs->PFI()->name); //clearer for testing

		//Leaves not relating to a field: sort by operand 1.
		if (lhs->BasicOp() == FD_FILE$)
			return (lhs->Operand1().CompareString(rhs->Operand1()) < 0);
		else //SET$ or POINT$
			return (lhs->Operand1().CompareNumeric(rhs->Operand1()) < 0);
	}
};

//************************************************************************************
void FindWorkInfo::GetUnprocessedLeavesByType
(std::vector<FindWorkNode_Leaf*>* leaves, FindOperator o, bool i)
{
	leaves->clear();

	if (IsComplete())
		return;

	if (op_flags[o]) {
		root->GetUnprocessedLeavesByType(leaves, o, i);

		if (!leaves->empty()) {
			LeafInfoLessThanPredicate pred;
			std::sort(leaves->begin(), leaves->end(), pred);
		}
	}
}

//************************************************************************************
//Diagnostics
//************************************************************************************
static const int indent = 2;
void FindWorkInfo::Dump(std::vector<std::string>& result, bool ebp) const
{
	result.clear();

	result.push_back("Find Runtime Work Tree");
	result.push_back("----------------------");
	root->Dump(result, (ebp) ? -1 : -2);
}

//***************************
std::string FindWorkNode::PtrString() const
{
	return util::UlongToHexString( (const unsigned long) this, 8);
}

//***************************
void FindWorkNode_Combo::Dump(std::vector<std::string>& result, int pad) const
{
	std::string line;
	if (pad > 0) {
		line.append(pad, ' ');
		line.append(util::IntToString((pad-1)/indent)).append(" ");
	}

	line.append( (node_and) ? "AND" : "OR").append(40, ' ');
	line.append(PtrString());
	result.push_back(line);

	if (pad == -1)
		result.push_back("  0 EBP (implied)");
	if (pad == -2)
		result.push_back("  0 Referback set (implied)");
	if (pad < 0)
		pad = 0;

	for (size_t x = 0; x < children.size(); x++)
		children[x]->Dump(result, pad+indent);
}

//***************************
void FindWorkNode_Leaf::Dump(std::vector<std::string>& result, int pad) const
{
	std::string line(pad, ' ');
	line.append(util::IntToString(tree_depth)).append(" ");

	if (!positive)
		line.append("Not ");

	if (pfi)
		line.append(pfi->name).append(1, ' ');

	line.append(FODesc(basic_op)).append(1, ' ');

	if (basic_op != FD_PRESENT) {
		if (force_alpha) line.append(1, '\'');
		line.append(operand1.ExtractString());
		if (force_alpha) line.append(1, '\'');

		if (FOIsRange2(basic_op)) {
			line.append("/");
			if (force_alpha) line.append(1, '\'');
			line.append(operand2.ExtractString());
			if (force_alpha) line.append(1, '\'');
		}

		line.append(1, ' ');

		if (force_num)
			line.append("(num) ");
		else if (force_alpha)
			line.append("(alpha) ");
	}

	line.append("  {");

	if (pfi) {
		line.append("Fatts:");
		if (pfi->atts.IsString()) line.append("S");
		if (pfi->atts.IsFloat()) line.append("F");
		if (pfi->atts.IsInvisible()) line.append("I");
		if (pfi->atts.IsOrdChar()) line.append("OC");
		if (pfi->atts.IsOrdNum()) line.append("ON");
		line.append(", ");
	}

	if (!will_use_index) 
		line.append("Ixs=N, ");
	else
		line.append("Ixs=Y:").append( (pfi->atts.IsOrdNum()) ? "num" : "alpha").append(", ");

	if (!tbs_required)
		line.append("Tbs=N");
	else {
		line.append("Tbs=Y");
		if (basic_op != FD_PRESENT)
			line.append( (tbs_compare_num) ? ":num" : ":alpha"); 
	}

	line.append("}  ");
	line.append(PtrString());

	result.push_back(line);
}



} //close namespace


