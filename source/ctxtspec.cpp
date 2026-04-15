
#include "stdafx.h"

#include "ctxtspec.h"

//Utils
#include "parsing.h"
//API Tiers
#ifdef _BBHOST
#include "ulvars.h"
#include "string2help.h" //V2.26
#endif
//Diagnostics
#include "except.h"
#include "msg_file.h"

namespace dpt {

void ContextSpecification::ContextSpecification_S(const std::string& str, int* incurs)
{
	refcount = 0;

	//V2 Jan 07.  Bug in various places where a spec consisting of spaces was passed in
	//but the input cursor was zero.
	//Moved this test down.
//	if (str.length() == 0 || (incurs && *incurs == std::string::npos))
//		throw Exception(CONTEXT_BAD, "Misssing context specification");

	membervar = NULL;
	level = cspec_dynamic_any;

	int dummy = 0;
	int* pcurs = &dummy;
	if (incurs)
		pcurs = incurs;
	int& cursor = *pcurs;

	cursor = str.find_first_not_of(' ', cursor);

	//See V2 fix comment above.
	if ((size_t)cursor == std::string::npos)
		throw Exception(CONTEXT_BAD, "Missing context specification");

	size_t term = str.find_first_of(" ,", cursor); //comma because of adhoc groups
	
	std::string word;
	if (term == std::string::npos)
		word = str.substr(cursor);
	else
		word = str.substr(cursor, term - cursor);
	
	cursor = term;

	if (word == "TEMP" || word == "TEMPORARY") {
		level = cspec_temp_group;
		word = util::GetWord(str, &cursor);
		basic_spec_string = "TEMP GROUP ";
	}
	else if (word == "PERM" || word == "PERMANENT") {
		level = cspec_perm_group;
		word = util::GetWord(str, &cursor);
		basic_spec_string = "PERM GROUP ";
	}

	if (level != cspec_dynamic_any) {
		if (word != "GROUP")
			throw Exception(CONTEXT_BAD, "Expected keyword 'GROUP'");

		word = util::GetWord(str, &cursor);
		if (word.length() == 0)
			throw Exception(CONTEXT_BAD, "No group name given");

	}
	else if (word == "GROUP") {
		level = cspec_dynamic_group;
		basic_spec_string = "GROUP ";

		word = util::GetWord(str, &cursor);
		if (word.length() == 0)
			throw Exception(CONTEXT_BAD, "No group name given");

	}
	else if (word == "FILE") {
		level = cspec_single_file;
		basic_spec_string = "FILE ";

		word = util::GetWord(str, &cursor);

		if (word.length() == 0)
			throw Exception(CONTEXT_BAD, "No file name given");

		//V2.08 Oct 07.  In a deferred update style OPEN command the file name may
		//be terminated by a comma (e.g. OPEN FILE TEAMS, TAPEN, TAPEA).  If the
		//keyword FILE was not used it's OK because we already looked for comma above to
		//catch ad-hoc groups in UL.
		if (word[word.length()-1] == ',') {
			word.resize(word.length()-1);
			cursor--;
		}
	}

	short_name = word;
	basic_spec_string.append(short_name);
}




//************************************************************************************
//This is here to cater for "special" UL contexts, namely:
//  $UPDATE
//  $CURFILE
//  group member %var
//  ad hoc groups
//************************************************************************************

#ifdef _BBHOST

ContextSpecification::ContextSpecification
(const std::string& str, int* cursor, bool allow_special)
{
	//In any case see what comes out of the normal parse - we can use some of it
	int special_cursor = (cursor) ? *cursor : 0;
	ContextSpecification_S(str, &special_cursor);

	if (allow_special) {

		if (level == cspec_dynamic_any) {
	
			//These two must be stand-alone
			if (basic_spec_string == "$CURFILE")
				level = cspec_$CURFILE;
			else if (basic_spec_string == "$UPDATE")
				level = cspec_$UPDATE;
			
			//An ad hoc group follows if we now have a comma
			else {
				int acurs = special_cursor;
				acurs = str.find_first_not_of(' ', acurs);
				if (acurs != std::string::npos && str[acurs] == ',') {
					level = cspec_adhoc_group;

					for (;;) {
						acurs = str.find_first_not_of(' ', acurs+1);
						if (acurs == std::string::npos)
							throw Exception(CONTEXT_BAD, 
								"Expected next ad-hoc group member after comma");

						int term = str.find_first_of(" ,", acurs);
						std::string mem;
						if (term == std::string::npos)
							mem = str.substr(acurs);
						else
							mem = str.substr(acurs, term - acurs);

						if (mem.length() == 0)
							throw Exception(CONTEXT_BAD, 
								"Null ad-hoc group member is not allowed");

						basic_spec_string.append(1,',').append(mem);
						acurs = term;
						acurs = str.find_first_not_of(' ', acurs);

						if (acurs == std::string::npos)
							break;
						if (str[acurs] != ',')
							break;
					}

					special_cursor = acurs;
					short_name = basic_spec_string;
				}
			}
		}
			
		//GROUP MEMBER is just a variation on a normal group context spec
		else if (level == cspec_temp_group 
					|| level == cspec_perm_group 
					|| level == cspec_dynamic_group) 
		{
			int curslit = special_cursor;
			std::string lit = util::GetWord(str, 1, ' ', curslit, &curslit);

			if (lit == "MEMBER") {

				bool got_type = false;
				int cursvar = str.find_first_not_of(' ', curslit);
				if (cursvar != std::string::npos)
					membervar = ULWorkingStorageReference::Parse(MakeString2(str), cursvar, false);

				if (membervar) {

					//Currently not allowing array variables although there's no real 
					//reason I guess not to. (SIVs are a slight complication).
					if (membervar->Subscripts()->size() > 0 ||
							membervar->SourceSubscripts()->size() > 0) {
						delete membervar;
						throw Exception(CONTEXT_BAD, 
							"Group member variable can not be subscripted");
					}

					special_cursor = cursvar;
					got_type = true;
				}

				if (!got_type)
					throw Exception(CONTEXT_BAD, "Invalid MEMBER specification");
			}
		}
	}

	if (cursor)
		*cursor = special_cursor;
}

//************************************************************************************
void ContextSpecification::SetRunTimeFileName(const std::string& rtfilename)
{
	short_name = rtfilename;

	//The file name will have been given at compile time as a variable reference
	int percpos = basic_spec_string.find_first_of('%');
	basic_spec_string.replace(percpos, -1, rtfilename);
}

#endif

//************************************************************************************
ContextSpecification::~ContextSpecification()
{

#ifdef _BBHOST
	if (membervar)
		delete membervar;
#endif

}

} //close namespace


