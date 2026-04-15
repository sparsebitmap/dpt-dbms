
#include "stdafx.h"

#include "rsvwords.h"

//Utils
//Diagnostics

namespace dpt {

char ReservedWords::usage_fd = 1;
char ReservedWords::usage_expr = 2;
char ReservedWords::usage_prtexpr = 4;

std::map<std::string, char> ReservedWords::data;

void ReservedWords::Initialize()
{
	AddEntry("AFTER"	, usage_fd);
	AddEntry("ALL"		, usage_prtexpr);
	AddEntry("AND"		, usage_fd | usage_expr | usage_prtexpr);
	AddEntry("AT"		, usage_prtexpr);
	AddEntry("BEFORE"	, usage_fd);
	AddEntry("BY"		, usage_expr); //expr because of ixloop
	AddEntry("COUNT"	, usage_fd | usage_expr | usage_prtexpr);
	AddEntry("EACH"		, usage_prtexpr);
	AddEntry("EDIT"		, 0);
	AddEntry("END"		, 0);
	AddEntry("FROM"		, usage_fd);
	AddEntry("IN"		, usage_fd);
	AddEntry("IS"		, usage_fd | usage_expr);
	AddEntry("LIKE"		, usage_fd | usage_expr);
	AddEntry("NOR"		, usage_fd);
	AddEntry("NOT"		, usage_fd | usage_expr);
	AddEntry("OCC"		, usage_fd | usage_expr | usage_prtexpr);
	AddEntry("OCCURRENCE",usage_fd | usage_expr | usage_prtexpr);
	AddEntry("ON"		, 0);
	AddEntry("OR"		, usage_fd | usage_expr);
	AddEntry("RECORD"	, usage_prtexpr);
	AddEntry("RECORDS"	, 0);
	AddEntry("TAB"		, usage_prtexpr);
	AddEntry("THEN"		, usage_expr); //expr because of IF
	AddEntry("TO"		, usage_fd | usage_expr | usage_prtexpr); //expr because of ixloop
	AddEntry("VALUE"	, usage_fd | usage_expr | usage_prtexpr);
	AddEntry("VALUES"	, usage_fd); //poss I think for V5 eventually
	AddEntry("WHERE"	, 0);
	AddEntry("WITH"		, usage_expr | usage_prtexpr);
	AddEntry("EQ"		, usage_fd | usage_expr);
	AddEntry("GE"		, usage_fd | usage_expr);
	AddEntry("GT"		, usage_fd | usage_expr);
	AddEntry("LE"		, usage_fd | usage_expr);
	AddEntry("LT"		, usage_fd | usage_expr);
	AddEntry("NE"		, usage_fd | usage_expr);

	//From the "reserved characters" table
	AddEntry("..."		, usage_prtexpr);
}

} //close namespace


