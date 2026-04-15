
//The temporary working structure used during a database find.  The find-time diagnostics
//allow the user to watch what happens do this structure as the find progresses.

#if !defined(BB_FINDWORK)
#define BB_FINDWORK

#include "apiconst.h"
#include "fieldval.h"

#include <vector>

namespace dpt {

class FieldValue;
class SingleDatabaseFileContext;
class FindSpecification;
struct FindSpecNode_Boolean;
struct FindSpecNode_Leaf;
struct FindSpecNode;
class FindWorkNode_Combo;
class FindWorkNode_Leaf;
class FindWorkInfo;
class BitMappedFileRecordSet;
struct PhysicalFieldInfo;
namespace util {
	class Pattern;
	class LineOutput;
}

//*****************************
class FindWorkNode {
	bool complete;
	FindWorkNode_Combo* parent;

protected:
	FindWorkNode(FindWorkNode_Combo* p) : complete(false), parent(p) {}
	void SetCompleteFlag() {complete = true;}

	std::string PtrString() const;

public:
	virtual ~FindWorkNode() {}

	virtual FindWorkNode_Combo* CastToCombo() {return NULL;}
	virtual FindWorkNode_Leaf* CastToLeaf() {return NULL;}

	FindWorkNode_Combo* Parent() {return parent;}
	void Adopt(FindWorkNode_Combo*);
	virtual void PromoteLevel() = 0;

	bool IsComplete() {return complete;}
	virtual void ForceComplete(util::LineOutput*) {SetCompleteFlag();}

	virtual void Dump(std::vector<std::string>&, int) const = 0;
};
	
	
//*****************************
class FindWorkNode_Combo : public FindWorkNode {
	std::vector<FindWorkNode*> children;
	BitMappedFileRecordSet* working_set;
	bool node_and;
	bool node_root;
	int complete_subcount;

	void Init() {working_set = NULL; complete_subcount = 0;}
	void NewChild(FindWorkInfo*, SingleDatabaseFileContext*, FindSpecNode*, int);

	void RegisterCompleteNodeSet(util::LineOutput*);
	void RegisterImprovedWorkingSet(util::LineOutput*);
	void SegmentRestrict(BitMappedFileRecordSet*, util::LineOutput*);

	void CollapseChild(int);

public:
	FindWorkNode_Combo(FindWorkInfo*, FindWorkNode_Combo*, SingleDatabaseFileContext*, 
						FindSpecNode_Leaf*);
	FindWorkNode_Combo(FindWorkInfo*, FindWorkNode_Combo*, SingleDatabaseFileContext*, 
						FindSpecNode_Boolean*, int);
	FindWorkNode_Combo(FindWorkInfo*, FindWorkNode_Combo*, SingleDatabaseFileContext*, 
						const FindSpecification*);
	void Collapse();

	~FindWorkNode_Combo();
	FindWorkNode_Combo* CastToCombo() {return this;}
	void PromoteLevel();

	bool IsAndNode() {return node_and;}

	BitMappedFileRecordSet* WorkingSet() {return working_set;}
	void SetWorkingSet(BitMappedFileRecordSet* s, bool improved, util::LineOutput*);

	BitMappedFileRecordSet* RestrictingSet();

	void GetUnprocessedLeavesByType(std::vector<FindWorkNode_Leaf*>*, FindOperator, bool);
	void CopyUpCompleteChildSet(BitMappedFileRecordSet*, util::LineOutput*);
	void IncrementCompleteCount(util::LineOutput*);
	void ForceComplete(util::LineOutput*);

	void Dump(std::vector<std::string>&, int) const;
};

//*****************************
class FindWorkNode_Leaf : public FindWorkNode {
	int tree_depth;
	FindOperator basic_op;
	FieldValue operand1;
	FieldValue operand2;
	PhysicalFieldInfo* pfi;
	bool positive;
	bool force_alpha;
	bool force_num;
	bool will_use_index;

	bool tbs_required;
	bool tbs_compare_num;
	util::Pattern* tbs_pattern;
	void FlagForTBS(bool);

	void FlagForIndexed() {will_use_index = true;}

public:
	FindWorkNode_Leaf(FindWorkInfo*, FindWorkNode_Combo*, SingleDatabaseFileContext*, 
						FindSpecNode_Leaf*, int d);
	~FindWorkNode_Leaf();
	void PromoteLevel() {tree_depth--;}

	FindWorkNode_Leaf* CastToLeaf() {return this;}

	int TreeDepth() const {return tree_depth;}
	bool Positive() const {return positive;}
	FindOperator BasicOp() const {return basic_op;}
	const FieldValue& Operand1() const {return operand1;}
	const FieldValue& Operand2() const {return operand2;}
	PhysicalFieldInfo* PFI() const {return pfi;}

	BitMappedFileRecordSet* RestrictingSet() {return Parent()->RestrictingSet();}

	bool IsDesired(FindOperator, bool);

	void RegisterLeafSet(BitMappedFileRecordSet*, util::LineOutput*);
	void RegisterWholeFileLeafSet(util::LineOutput*);

	bool TableBSearchPositiveTestField(FieldValue&);

	void Dump(std::vector<std::string>&, int) const;
};

//****************************************************************************************
class FindWorkInfo {
	FindWorkNode_Combo* root;

	bool early_ebp_flag;
	bool index_flag;
	bool tbs_flag;
	bool op_flags[FD_HI_BASIC_OPTR+1];

public:
	FindWorkInfo(SingleDatabaseFileContext*, const FindSpecification&);
	~FindWorkInfo() {delete root;}
	void SetLeafFlags(FindOperator, bool, bool);

	BitMappedFileRecordSet* RootSet() {return root->WorkingSet();}
	void SetRootSet(BitMappedFileRecordSet* s, bool b, util::LineOutput* pop) {
		root->SetWorkingSet(s, b, pop);}
	void SetEarlyEBPFlag() {early_ebp_flag = true;}
	bool GotEarlyEBPForNegation() {return early_ebp_flag;}
	bool IsComplete() {return root->IsComplete();}

	void GetUnprocessedLeavesByType(std::vector<FindWorkNode_Leaf*>*, FindOperator, bool = true);

	bool IndexSearchFlag() {return index_flag;}
	bool TableBSearchFlag() {return tbs_flag;}

	void Dump(std::vector<std::string>&, bool) const;
};


} //close namespace

#endif
