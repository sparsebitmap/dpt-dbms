
#if !defined(BB_FINDSPEC)
#define BB_FINDSPEC

#include "apiconst.h"
#include "fieldval.h"

#include <vector>

namespace dpt {

class FindSpecification;
struct FindSpecNode_Leaf;
struct FindSpecNode_Boolean;

//**********************
struct FindSpecNode {
	bool bracketed; //V3.0 Helps with expression parsing

	FindSpecNode() : bracketed(false) {};
	virtual ~FindSpecNode() {}

	virtual FindSpecNode_Leaf* CastToLeaf() {return NULL;}
	virtual FindSpecNode_Boolean* CastToBoolean() {return NULL;}

	virtual FindSpecNode* MakeCopy_D() = 0;
	static FindSpecNode* MakeCopy(FindSpecNode* n) {if (n) return n->MakeCopy_D(); return NULL;}
	virtual void Negate() = 0;

	virtual void Dump(std::vector<std::string>&) const = 0;
};

//**********************
struct FindSpecNode_Leaf : public FindSpecNode {
	std::string fieldname;
	FindOperator optr;
	FieldValue operand1;
	FieldValue operand2;

	FindOperator basic_op;
	bool positive;
	bool force_num;
	bool force_alpha;

	void MaskBasicFDOperator();

	//Copy
	FindSpecNode_Leaf(FindSpecNode_Leaf* l) 
		: fieldname(l->fieldname), optr(l->optr), operand1(l->operand1), operand2(l->operand2),
			basic_op(l->basic_op), positive(l->positive), force_num(l->force_num), 
			force_alpha(l->force_alpha) {}

	//Field based criteria with operands
	FindSpecNode_Leaf(const std::string&, const FindOperator&, const FieldValue*, const FieldValue*);
	FindSpecNode_Leaf(const std::string&, const FindOperator&); //present (no operands)
	FindSpecNode_Leaf(const FindOperator&, const int&);			//point$ or single rec
	FindSpecNode_Leaf(const FindOperator&, const std::string&);	//file$
	FindSpecNode_Leaf(const FindOperator&);						//all records

	~FindSpecNode_Leaf() {}

	FindSpecNode_Leaf* CastToLeaf() {return this;}
	FindSpecNode* MakeCopy_D() {return new FindSpecNode_Leaf(this);}
	void Negate() {positive = !positive;}

	void Dump(std::vector<std::string>&) const;
};

//**********************
struct FindSpecNode_Boolean : public FindSpecNode {
	FindSpecNode* node1;
	FindSpecNode* node2;
	bool node_and;

	FindSpecNode_Boolean(bool a) : node1(NULL), node2(NULL), node_and(a) {}

	FindSpecNode_Boolean* CastToBoolean() {return this;}
	FindSpecNode* MakeCopy_D();
	~FindSpecNode_Boolean() {if (node1) delete node1; if (node2) delete node2;}

	//Simple application of De Morgan's theorem.  Valid because NOT is a pure bitflip.
	void Negate() {if (node1) node1->Negate(); if (node2) node2->Negate(); node_and = !node_and;}

	void Dump(std::vector<std::string>&) const;
};

//**************************************************************************************
//Interface class - API users ignore the above.
//**************************************************************************************
class FindSpecification {
	friend class DirectValueCursor;
	friend class FindWorkInfo;
	friend class FindWorkNode_Combo;

	unsigned int rtd;

	void Grow(const FindSpecification&, bool);
	void Destroy() {if (rootnode) {delete rootnode; rootnode = NULL;}}

	//V3.0 This bunch
	void BadParse(const std::string&, size_t = std::string::npos);
	bool AtEOL(const std::string& l, size_t c) {return (c == std::string::npos || c >= l.length());}
	void NextChar(const std::string&, size_t&, bool = true);
	bool ParseKeyword(const std::string&, size_t&, const char*);
	std::string ParseOperand(const std::string&, size_t&);
	FindOperator ParseOperator(const std::string&, size_t&);
	FindOperator ParseBasicOperator(const std::string&, size_t&, bool = true);
	FindSpecNode_Leaf* ParseCondition(const std::string&, size_t&);
	FindSpecNode* ParseExpression(const std::string&, size_t&, bool = false);

protected:
	FindSpecNode* rootnode;
//	FindSpecification(void*) : rtd(0), rootnode(NULL) {} //V3.0 make space in overload set
	FindSpecification(bool dummy) : rtd(0), rootnode(NULL) {}

public:
	FindSpecification() : rtd(0), rootnode(new FindSpecNode_Leaf(FD_ALLRECS)) {}
	virtual ~FindSpecification() {Destroy();}

	//---------------------------------------------------------------------
	//Single operand - EQ, LT etc.
	FindSpecification(const std::string& n, const FindOperator& o, const FieldValue& v1)
		: rtd(0), rootnode(new FindSpecNode_Leaf(n, o, &v1, NULL)) {}

	//Two operands - ranges
	FindSpecification(const std::string& n, const FindOperator& o, const FieldValue& v1, const FieldValue& v2)
		: rtd(0), rootnode(new FindSpecNode_Leaf(n, o, &v1, &v2)) {}

	//Point$ or single record number
	FindSpecification(const FindOperator& o, const int& v) 
		: rtd(0), rootnode(new FindSpecNode_Leaf(o, v)) {}

	//FILE$
	FindSpecification(const FindOperator& o, const std::string& v) 
		: rtd(0), rootnode(new FindSpecNode_Leaf(o, v)) {}

	//Field presence (or a value find with no criteria)
	FindSpecification(const std::string& n, const FindOperator& o)
		: rtd(0), rootnode(new FindSpecNode_Leaf(n, o)) {}

	//V3.0: Arbitrarily complex query expressed as a string
	FindSpecification(const char* expr) : rtd(0) {
		size_t cursor = 0; rootnode = ParseExpression(expr, cursor);} 

	//---------------------------------------------------------------------
	//Operations
	FindSpecification(const FindSpecification& from) 
		: rtd(from.rtd), rootnode(FindSpecNode::MakeCopy(from.rootnode)) {}

	FindSpecification& operator=(const FindSpecification& from) {
		if (this == &from) return *this;
		Destroy(); rtd = from.rtd; 
		rootnode = FindSpecNode::MakeCopy(from.rootnode); return *this;}

	//Combiners group A: Neat, convenient and readable, but involving more temporaries
	FindSpecification& operator&=(const FindSpecification& s) {Grow(s, true); return *this;}
	FindSpecification& operator|=(const FindSpecification& s) {Grow(s, false); return *this;}
	FindSpecification operator!() {FindSpecification temp(*this); temp.Negate(); return temp;}

	//Combiners group B: Less elegant but also less heap work.  Used by the UL compiler.
	static FindSpecification* Splice(FindSpecification* left, FindSpecification* right, bool optr_and);
	void Negate() {rootnode->Negate();}

	//See options at top
	void Dump(std::vector<std::string>&) const;
	void SetRunTimeDiagnosticLevel(unsigned int i);

	unsigned int Diag_Spec() const {return (rtd & FD_RTD_SPEC);}
	unsigned int Diag_Workinit() const {return (rtd & FD_RTD_WORKINIT);}
	unsigned int Diag_Norun() const {return (rtd & FD_RTD_NORUN);}
	unsigned int Diag_Critlog() const {return (rtd & FD_RTD_CRIT_LOG);}
	unsigned int Diag_Critcount() const {return (rtd & FD_RTD_CRIT_COUNTS);}
};

//**************************************************************************************
inline FindSpecification operator&(const FindSpecification& lhs, const FindSpecification& rhs) {
	FindSpecification result(lhs);
	result &= rhs;
	return result;
} 
inline FindSpecification operator|(const FindSpecification& lhs, const FindSpecification& rhs) {
	FindSpecification result(lhs);
	result |= rhs;
	return result;
}

//**************************************************************************************
//Special type for FindValues and CountValues
//**************************************************************************************
//V2.04.  Mar 07.  VC2005 is more strict.
//class FindValuesSpecification : private FindSpecification {
class FindValuesSpecification : public FindSpecification {
public:
	FindValuesSpecification(const std::string& fname) 
							: FindSpecification(fname, FD_ALLVALUES) {}

	FindValuesSpecification(const std::string& fname, const FindOperator& o, 
								const FieldValue& v1) 
						: FindSpecification(fname, o, v1) {}
	FindValuesSpecification(const std::string& fname, const FindOperator& o, 
								const FieldValue& v1, const FieldValue& v2) 
						: FindSpecification(fname, o, v1, v2) {}

	//Special form to allow both a range and a pattern to be given
	FindValuesSpecification(const std::string& fname, const FindOperator& o, 
								const std::string& lo, const std::string& hi, 
								const std::string& pattstring, bool notlike = false) 
						: FindSpecification(fname, o, lo, hi) {
		FindOperator lkop = (notlike) ? FD_UNLIKE : FD_LIKE;
		operator&=(FindValuesSpecification(fname, lkop, pattstring));}

	//A version of the above which is more useful in many situations because the calling 
	//code can be simpler.  Neither, either or both values and/or the pattern may be null.
	FindValuesSpecification(const std::string&, const FindOperator&, 
		const FieldValue*, const FieldValue*, const std::string*, bool);

	std::string FieldName();

	void Dump(std::vector<std::string>& vs) {
		vs.push_back("Values of: "); FindSpecification::Dump(vs);}
};

} //close namespace

#endif
