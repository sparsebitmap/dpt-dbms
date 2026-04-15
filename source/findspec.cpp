
#include "stdafx.h"

#include "findspec.h"

#include <map>

//Utils
#include "lineio.h"
#include "dataconv.h"
#include "charconv.h"
#include "parsing.h"
//Diagnostics
#include "except.h"
#include "msg_db.h"

namespace dpt {

const char* fodesc[FD_HI_BASIC_OPTR+1] = 
{	"Null", "EQ", "GT", "LT", "GE", "LE", 
	"GE/LE", "GT/LE", "GE/LT", "GT/LT",
	 "LIKE", "UNLIKE",
	 "PRESENT", "POINT$", "SET$", "FILE$", 
	"Single record", "All records", "All values"};

//*****************************************************************************
void FindSpecNode_Leaf::MaskBasicFDOperator()
{
	basic_op = optr;
	positive = true;
	force_alpha = false;
	force_num = false;

	if (basic_op > FD_FORCE_NUM) {
		force_num = true;
		basic_op -= FD_FORCE_NUM;
	}

	if (basic_op > FD_FORCE_ALPHA) {
		force_alpha = true;
		basic_op -= FD_FORCE_ALPHA;
	}

	if (basic_op > FD_NOT) {
		positive = false;
		basic_op -= FD_NOT;
	}

	if (force_num && force_alpha)
		throw Exception(DML_BAD_FIND_SPEC, 
			"You can't force both an alpha and a numeric find");

//	if (basic_op < 0 || basic_op > FD_HI_BASIC_OPTR) //V2.24 Nice catch gcc (unsigned can never be negative)
	if (basic_op > FD_HI_BASIC_OPTR)
		throw Exception(DML_BAD_FIND_SPEC, 
			"Invalid combination of find operator flags (unknown reason)");
}

//*****************************************************************************
FindSpecNode_Leaf::FindSpecNode_Leaf
(const std::string& n, const FindOperator& o, const FieldValue* pv1, const FieldValue* pv2)
: fieldname(n), optr(o)
{
	if (pv1) 
		operand1 = *pv1;
	if (pv2) 
		operand2 = *pv2;

	MaskBasicFDOperator();

	if (!pv2) {
		if (FOIsRange2(basic_op))
			throw Exception(DML_BAD_FIND_SPEC, std::string("The '")
				.append(FODesc(basic_op))
				.append("' operator requires two field values to be specified"));
	}
	else {
		if (!FOIsRange2(basic_op))
			throw Exception(DML_BAD_FIND_SPEC, 
				"Only two-ended range operators may have two field values specified");
	}
}

//*****************************************************************************
FindSpecNode_Leaf::FindSpecNode_Leaf(const FindOperator& op, const int& recnum)
: optr(op), operand1(recnum)
{
	MaskBasicFDOperator();

	if (basic_op != FD_POINT$ && basic_op != FD_SINGLEREC && basic_op != FD_SET$)
		throw Exception(DML_BAD_FIND_SPEC, std::string("The '")
			.append(FODesc(basic_op))
			.append("' find operator is not unary prefix"));

	if (basic_op == FD_SINGLEREC && !positive)
		throw Exception(DML_BAD_FIND_SPEC, std::string("The '")
			.append(FODesc(basic_op))
			.append("' find operator may not be negated"));
}

//*****************************************************************************
FindSpecNode_Leaf::FindSpecNode_Leaf(const FindOperator& op, const std::string& filename)
: optr(op), operand1(filename)
{
	MaskBasicFDOperator();

	if (basic_op != FD_FILE$)
		throw Exception(DML_BAD_FIND_SPEC, std::string("The '")
			.append(FODesc(basic_op))
			.append("' find operator is not unary prefix"));
}

//*****************************************************************************
FindSpecNode_Leaf::FindSpecNode_Leaf(const std::string& n, const FindOperator& op)
: fieldname(n), optr(op)
{
	MaskBasicFDOperator();

	if (basic_op != FD_PRESENT && basic_op != FD_ALLVALUES)
		throw Exception(DML_BAD_FIND_SPEC, std::string("The '")
			.append(FODesc(basic_op))
			.append("' find operator is not unary postfix"));
}

//*****************************************************************************
FindSpecNode_Leaf::FindSpecNode_Leaf(const FindOperator& op)
: optr(op)
{
	MaskBasicFDOperator();

	if (basic_op != FD_ALLRECS && basic_op != FD_NORECS)
		throw Exception(DML_BAD_FIND_SPEC, std::string("The '")
			.append(FODesc(basic_op))
			.append("' operator requires at least one operand"));
}

//*****************************************************************************
FindSpecNode* FindSpecNode_Boolean::MakeCopy_D()
{
	FindSpecNode_Boolean* thecopy = new FindSpecNode_Boolean(node_and);

	try {
		thecopy->node1 = MakeCopy(node1);
		thecopy->node2 = MakeCopy(node2);
	}
	catch (...) {
		if (node1) 
			delete node1;
		delete thecopy;
		throw;
	}

	return thecopy;
}

//*****************************************************************************
void FindSpecification::Grow(const FindSpecification& rhs, bool optr_and)
{
	if (!rootnode)
		throw Exception(DML_BAD_FIND_SPEC, 
			"Corrupt find specification - probably due to previous memory problems");

	//The find engine does most of the intelligent stuff.  However, I've decided to
	//put in a special bit here to cater for the common case of a default object
	//(which has a dummy enetry of ALL RECORDS) being modified.  
	//AND with this makes the all records condition irrelevant, so we delete it. 
	//OR leaves the condition as all records, so the modifier is ignored.
	FindSpecNode_Leaf* leaf = rootnode->CastToLeaf();
	if (leaf) {
		if (leaf->optr == FD_ALLRECS) {
			if (optr_and)
				*this = rhs;
			return;
		}
	}

	//Otherwise deal with it normally by inserting a new boolean node at the root
	FindSpecNode_Boolean* newroot = new FindSpecNode_Boolean(optr_and);

	try {
		newroot->node2 = FindSpecNode::MakeCopy(rhs.rootnode);
	}
	catch (...) {
		delete newroot;
		throw;
	}

	newroot->node1 = rootnode;
	rootnode = newroot;
}

//*****************************************************************************
FindSpecification* FindSpecification::Splice
(FindSpecification* left, FindSpecification* right, bool optr_and)
{
	FindSpecification* result = NULL;
	try {
//		result = new FindSpecification(NULL); //V3.0
		result = new FindSpecification(false);

		FindSpecNode_Boolean* root = new FindSpecNode_Boolean(optr_and);
		result->rootnode = root;
		root->node1 = left->rootnode;
		root->node2 = right->rootnode;
		left->rootnode = NULL;
		delete left;
		right->rootnode = NULL;
		delete right;
	}
	catch (...) {
		if (result)
			delete result;
		throw;
	}

	return result;
}





//*****************************************************************************
//*****************************************************************************
//Diagnostics
//*****************************************************************************
//*****************************************************************************
static int dump_pad;

void FindSpecification::Dump(std::vector<std::string>& result) const
{
	result.clear();

	result.push_back("Find Specification");
	result.push_back("------------------");

	dump_pad = 0;
	rootnode->Dump(result);
}

//********************
void FindSpecNode_Boolean::Dump(std::vector<std::string>& result) const
{
	std::string line(dump_pad, ' ');
	line.append( (node_and) ? "AND <" : "OR  <");

	dump_pad += 6;

	node1->Dump(result);
	result.push_back(line);
	node2->Dump(result);

	dump_pad -= 6;
}

//********************
void FindSpecNode_Leaf::Dump(std::vector<std::string>& result) const
{
	std::string line(dump_pad, ' ');

	if (!positive)
		line.append("Not ");

	//Most operators use the field name
	if (FOIsField(basic_op))
		line.append(fieldname).append(1, ' ');

	line.append(FODesc(basic_op)).append(1, ' ');

	if (basic_op != FD_PRESENT) {
		line.append(operand1.ExtractString());

		if (FOIsRange2(basic_op)) {
			line.append("/");
			line.append(operand2.ExtractString());
		}

		line.append(1, ' ');
	}

	if (force_num)
		line.append("(force num)");
	else if (force_alpha)
		line.append("(force alpha)");

	result.push_back(line);
}

//*****************************************************************************
void FindSpecification::SetRunTimeDiagnosticLevel(unsigned int i) 
{
	if (i & FD_RTD_CRIT_COUNTS) {
		i |= FD_RTD_CRIT_LOG;
	}

	//This was going to be a more detailed level still - see comments in header file
//	if (i & FD_RTD_WORK_COUNTS) {
//		i |= FD_RTD_CRIT_LOG;
//		i |= FD_RTD_CRIT_COUNTS;
//	}

	rtd = i;
}


//*****************************************************************************
//*****************************************************************************
FindValuesSpecification::FindValuesSpecification
(const std::string& fname, const FindOperator& optr, 
	const FieldValue* pfrom, const FieldValue* pto, const std::string* plike, bool notlike)
//: FindSpecification(NULL) //V3.0
: FindSpecification(false)
{
	FindOperator lkop = (notlike) ? FD_UNLIKE : FD_LIKE;

	if (pfrom)
		rootnode = new FindSpecNode_Leaf(fname, optr, pfrom, pto);
	else if (pto)
		rootnode = new FindSpecNode_Leaf(fname, optr, pto, NULL);
	else if (plike) {
		FieldValue v(*plike);
		rootnode = new FindSpecNode_Leaf(fname, lkop , &v, NULL);
	}
	else
		rootnode = new FindSpecNode_Leaf(fname, optr);

	//pattern given as well as range
	if ( (pfrom || pto) && plike)
		operator&=(FindValuesSpecification(fname, lkop, *plike));
}

//*****************************************************************************
std::string FindValuesSpecification::FieldName()
{
	if (rootnode->CastToLeaf())
		return rootnode->CastToLeaf()->fieldname;
	else
		return rootnode->CastToBoolean()->node1->CastToLeaf()->fieldname;
}






//*****************************************************************************
//V3.0.  Added these primarily to help API programs making large numbers of
//calls to construct queries, as well as provide the ability for users of
//API programs to enter queries in text as it can be very convenient.  Also the
//file browser application needed a format in which to serialize queries, and 
//this is ideal for that too.
//Note on the format: It's similar to UL but not with fewer frills and little
//nuances.  Very basic parsing.  Pseudo grammar is in the docs somewhere.
//*****************************************************************************
void FindSpecification::BadParse(const std::string& m, size_t cursor) 
{
	std::string msg(m);
	if (cursor != std::string::npos)
		msg.append(" at or near position ").append(util::IntToString(cursor));

	throw Exception(DML_BAD_FIND_SPEC, msg);
}

//*****************************************************************************
void FindSpecification::NextChar
(const std::string& line, size_t& cursor, bool throweol) 
{
	//Locate next non-whitespace at or after the cursor
	for (;;) {
		if (AtEOL(line, cursor)) {
			if (throweol)
				BadParse("Unexpected end of expression");
			return;
		}

		if (line[cursor] > ' ')
			return;

		cursor++;
	}
}

//*****************************************************************************
bool FindSpecification::ParseKeyword
(const std::string& line, size_t& cursor, const char* keyword)
{
	const char* startpos = line.c_str() + cursor;

	const char* p1 = startpos;
	const char* p2 = keyword;

	char c1 = util::ToUpper(*p1);
	char c2 = util::ToUpper(*p2);

	while (c1 == c2) {                //match so far
		p1++;
		p2++;                         //advance pointers

		if (*p2 == '\0') {            //no more chars to compare - success
			cursor += p1 - startpos;
			NextChar(line, cursor, false);
			return true;
		}

		while (*p1 == ' ')            //ignore spaces in multiword keywords - IN RANGE etc.
			p1++;                     //(so actually will allow through e.g. AL PHA but that's OK)
		
		if (*p1 == '\0')              //end of string - failure
			return false;

		c1 = util::ToUpper(*p1);      //for case-insensitive comparison
		c2 = util::ToUpper(*p2);
	}

	return false;
}

//*****************************************************************************
//Field names and operands are both just strings, with optional quotes
std::string FindSpecification::ParseOperand(const std::string& line, size_t& cursor)
{
	NextChar(line, cursor);

	size_t startpos = cursor;
	std::string operand;

	//Hex strings are e.g. x'414243'
	bool hexformat = false;
	if (line.length() >= cursor+3) {
		char x = line[cursor];
		char q = line[cursor+1];
		if ( (q == '\'' || q == '\"') && (x == 'X' || x == 'x') ) {
			hexformat = true;
			cursor++;
			startpos++;
		}
	}

	//With quoted strings we can parse directly to the other end of the string
	char quotechar = 0;
	char c = line[cursor];
	if (c == '\'' || c == '\"')
		quotechar = c;

	if (quotechar) {
		cursor++;

		//Doubled quotes do not end the string - parse past them
		for (;;) {
			cursor = line.find(quotechar, cursor);
			if (cursor == std::string::npos)
				BadParse("Missing close quote");

			//Single quote?
			if (cursor == line.length()-1 || line[cursor+1] != quotechar) {
				cursor++;
				break;
			}
			else {
				//No, doubled so move past
				cursor += 2;
			}
		}

		//Pull the operand out of the line
		operand = line.substr(startpos+1, cursor - startpos - 2);

		//Replace any doubled quotes in it
		util::ReplaceString(operand, std::string(2,quotechar), std::string(1,quotechar));

		//Unhex it (hex always quoted so no need to do this in the "quick" unquoted version below)
		if (hexformat)
			operand = util::HexStringToAsciiString(operand);
	}

	//No quotes - go up to the next character deemed a "terminator" (like the UL set)
	else {
		cursor = line.find_first_of("=!¬^><()|& ", cursor);
		if (cursor == std::string::npos)
			operand = line.substr(startpos);
		else
			operand = line.substr(startpos, cursor - startpos);

		//Unquoted ones get uppercased - why????
		//util::ToUpper(operand);
	}

	return operand;
}

//*****************************************************************************
//A couple of different valid locations for NOT but otherwise simple.
FindOperator FindSpecification::ParseOperator(const std::string& line, size_t& cursor) 
{
	NextChar(line, cursor);

	bool isnot = false;
	while (ParseKeyword(line, cursor, "NOT"))
		isnot = !(isnot);

	bool force_alpha = false;
	bool force_num = false;

	//This is syntactic sugar
	//if (ParseKeyword(line, cursor, "IS"))
	//	bool whocares = true;
	ParseKeyword(line, cursor, "IS");

	//Alternative position 1 for NOT
	while (ParseKeyword(line, cursor, "NOT"))
		isnot = !(isnot);

	if (ParseKeyword(line, cursor, "ALPHABETICALLY") || ParseKeyword(line, cursor, "ALPHA"))
		force_alpha = true;
	else if (ParseKeyword(line, cursor, "NUMERICALLY") || ParseKeyword(line, cursor, "NUM"))
		force_num = true;

	//Alternative position 2 for NOT
	while (ParseKeyword(line, cursor, "NOT"))
		isnot = !(isnot);

	FindOperator optr = ParseBasicOperator(line, cursor);

	//NE and != imply not
	if (optr & FD_NOT) {
		isnot = !(isnot);
		optr -= FD_NOT;
	}

	//Add modifiers as parsed above
	if (isnot) 
		optr |= FD_NOT;
	if (force_alpha) 
		optr |= FD_FORCE_ALPHA;
	if (force_num) 
		optr |= FD_FORCE_NUM;

	return optr;
}

//*****************************************************************************
FindOperator FindSpecification::ParseBasicOperator
(const std::string& line, size_t& cursor, bool throw_invalid) 
{
	NextChar(line, cursor);

	//NB the order of these is important in some cases where the start of the
	//operator string is the same (> and >= for example), so we must check >= first.
	static std::vector<std::pair<std::string, FindOperator> >mops;
	if (mops.empty()) {
		mops.push_back(std::make_pair<std::string, FindOperator>("EQ", FD_EQ));
		mops.push_back(std::make_pair<std::string, FindOperator>("NE", (FD_EQ | FD_NOT)));
		mops.push_back(std::make_pair<std::string, FindOperator>("LT", FD_LT));
		mops.push_back(std::make_pair<std::string, FindOperator>("LE", FD_LE));
		mops.push_back(std::make_pair<std::string, FindOperator>("GT", FD_GT));
		mops.push_back(std::make_pair<std::string, FindOperator>("GE", FD_GE));
		mops.push_back(std::make_pair<std::string, FindOperator>("==", FD_EQ));
		mops.push_back(std::make_pair<std::string, FindOperator>("=", FD_EQ));
		mops.push_back(std::make_pair<std::string, FindOperator>("!=", (FD_EQ | FD_NOT)));
		mops.push_back(std::make_pair<std::string, FindOperator>("¬=", (FD_EQ | FD_NOT)));
		mops.push_back(std::make_pair<std::string, FindOperator>("^=", (FD_EQ | FD_NOT)));
		mops.push_back(std::make_pair<std::string, FindOperator>("<=", FD_LE));
		mops.push_back(std::make_pair<std::string, FindOperator>("<", FD_LT));        
		mops.push_back(std::make_pair<std::string, FindOperator>(">=", FD_GE));
		mops.push_back(std::make_pair<std::string, FindOperator>(">", FD_GT));
		mops.push_back(std::make_pair<std::string, FindOperator>("RANGE", FD_RANGE));
		mops.push_back(std::make_pair<std::string, FindOperator>("INRANGE", FD_RANGE));
		mops.push_back(std::make_pair<std::string, FindOperator>("LIKE", FD_LIKE));
		mops.push_back(std::make_pair<std::string, FindOperator>("UNLIKE", FD_UNLIKE));
		mops.push_back(std::make_pair<std::string, FindOperator>("PRESENT", FD_PRESENT));
		mops.push_back(std::make_pair<std::string, FindOperator>("BETWEEN", FD_RANGE_GT_LT));
		mops.push_back(std::make_pair<std::string, FindOperator>("POINT$", FD_POINT$));
		mops.push_back(std::make_pair<std::string, FindOperator>("REC$", FD_SINGLEREC));
	}

	for (int i = 0; i < mops.size(); i++) {
		if (ParseKeyword(line, cursor, mops[i].first.c_str()))
			return mops[i].second;
	}

	if (throw_invalid)
		BadParse("Invalid operator", cursor);
	return FD_NULLOP;
}

//*****************************************************************************
FindSpecNode_Leaf* FindSpecification::ParseCondition(const std::string& line, size_t& cursor)
{
	NextChar(line, cursor);

	std::string fieldname;
	FindOperator optr;

	//The fieldname is not required on some operators, so try them first.
	optr = ParseBasicOperator(line, cursor, false);

	if (optr != FD_NULLOP) {
		if (optr != FD_POINT$ && optr != FD_SINGLEREC)
			BadParse("Expected field name, not operator", cursor);
	}

	//The usual situation
	else {
		fieldname = ParseOperand(line, cursor);
		optr = ParseOperator(line, cursor);
		if (optr == FD_POINT$ || optr == FD_SINGLEREC)
			BadParse("Operator should not have prefix operand", cursor);
	}

	//Now get operands according to which operator it was
	FieldValue v1, v2;

	switch (FOBasic(optr)) {

	case FD_EQ: 
	case FD_GT: 
	case FD_GE: 
	case FD_LT: 
	case FD_LE: 
	case FD_LIKE: 
	case FD_UNLIKE: 
		v1 = ParseOperand(line, cursor);
		return new FindSpecNode_Leaf(fieldname, optr, &v1, NULL);

	case FD_RANGE:

		//V3.02. Optional FROM
		ParseKeyword(line, cursor, "FROM");

		v1 = ParseOperand(line, cursor);

		NextChar(line, cursor);
		if (!ParseKeyword(line, cursor, "TO"))
			BadParse("Expected connector 'TO' in RANGE condition");

		v2 = ParseOperand(line, cursor);

		return new FindSpecNode_Leaf(fieldname, optr, &v1, &v2);

	//Between
	case FD_RANGE_GT_LT:
		v1 = ParseOperand(line, cursor);

		NextChar(line, cursor);
		if (!ParseKeyword(line, cursor, "AND"))
			BadParse("Expected connector 'AND' in BETWEEN condition");

		v2 = ParseOperand(line, cursor);

		return new FindSpecNode_Leaf(fieldname, optr, &v1, &v2);

	case FD_POINT$:
	case FD_SINGLEREC:
		return new FindSpecNode_Leaf(optr, util::StringToInt(ParseOperand(line, cursor)));
	
	case FD_PRESENT:
		return new FindSpecNode_Leaf(fieldname, optr);
	}

	BadParse("Oops - did not handle find operator!");
	return NULL;
}

//*****************************************************************************
FindSpecNode* FindSpecification::ParseExpression
(const std::string& intext, size_t& cursor, bool insub)
{
	//First strip out comment lines and turn tabs and newlines into whitespace
	std::vector<std::string> loglines;
	util::Tokenize(loglines, intext, "\n", true);

	std::string line;
	for (int x = 0; x < loglines.size(); x++) {
		std::string l = loglines[x];

		util::ReplaceChar(l, '\t', ' ');
		util::ReplaceChar(l, '\r', ' ');
		util::DeBlank(l);

		if (l.length() == 0)
			continue;
		if (l[0] == '*')
			continue;

		line.append(l).append(1, ' ');
	}

	size_t startcursor = cursor;
	NextChar(line, cursor, false);

	//Completely empty expression is fine
	if (AtEOL(line, cursor) && startcursor == 0)
		return new FindSpecNode_Leaf(FD_ALLRECS);

	bool isnot = false;
	while (ParseKeyword(line, cursor, "NOT"))
		isnot = !(isnot);

	FindSpecNode_Boolean* thisnode = NULL;
	FindSpecNode* lhs = NULL;
	FindSpecNode* rhs = NULL;

	try {

		//The leftmost item is a standalone condition or bracketed subexpression
		if (line[cursor] != '(')
			lhs = ParseCondition(line, cursor);

		else {
			cursor++;
			lhs = ParseExpression(line, cursor, true);

			if (line[cursor] != ')')
				BadParse("Missing right bracket");
	
			cursor++;
			lhs->bracketed = true;
		}

		if (isnot)
			lhs->Negate();

		//Now let's carry on and see if there's a right hand side to this expression
		NextChar(line, cursor, false);

		if (AtEOL(line, cursor))
			return lhs;

		//At the end of a bracketed subexpression unwind one level
		if (line[cursor] == ')') {
			if (!insub)
				BadParse("Unbalanced right bracket", cursor);
			return lhs;
		}

		//Otherwise parse conjunction
		bool thisand;
		if (ParseKeyword(line, cursor, "OR") || 
			ParseKeyword(line, cursor, "|"))
			thisand = false;
		else if (ParseKeyword(line, cursor, "AND") || 
			ParseKeyword(line, cursor, "&"))
			thisand = true;
		else
			BadParse("Invalid boolean, expected AND/OR", cursor);

		thisnode = new FindSpecNode_Boolean(thisand);

		//Attach what we have so far as the LHS
		thisnode->node1 = lhs;
		lhs = NULL;

		//Recurse to get the RHS
		rhs = ParseExpression(line, cursor, insub);

		//Unbracketed OR on the RHS should "lose out" to AND at this level
		FindSpecNode_Boolean* rhsbool = rhs->CastToBoolean();

		if (thisand && rhsbool && !rhsbool->node_and && !rhs->bracketed) {
			thisnode->node2 = rhsbool->node1; //steal LHS from the OR
			rhsbool->node1 = thisnode;        //LHS of the OR now the result of our AND
			return rhs;                       //move OR up the tree (lower priority)
		}

		//Just plain left-to-right association with no funny business
		thisnode->node2 = rhs;
		return thisnode;
	}
	catch (...) {
		if (thisnode) 
			delete thisnode;
		if (lhs) 
			delete lhs;
		if (rhs) 
			delete rhs;
		throw;
	}
}

} //close namespace


