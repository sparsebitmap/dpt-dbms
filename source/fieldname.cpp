
#include "stdafx.h"

#include "fieldname.h"

//Utils
#include "parsing.h"
#include "rsvwords.h"
//Diagnostics
#include "except.h"
#include "msg_db.h"

namespace dpt {

//**************************************************************************************
//V3.0 see below.
//ParseFieldNameOperation::ParseFieldNameOperation
//(const std::string& l, int& lp, const std::string* tw, bool tc)
//: line(l), line_pos(lp), terminator_words(tw), check_terminator_chars(tc)
ParseFieldNameOperation::ParseFieldNameOperation
(const std::string& l, int& lp, const std::string* tw, const char* tc)
: line(l), line_pos(lp), terminator_words(tw), custom_terminator_chars(tc)
{
	//Initialize
	start_pos = line_pos;
	cursor = start_pos;
	in_quotes = false;
	pending_possible_doublequote = false;
	word_pos = -1;
	cprev = ' ';  //will force ignore of leading spaces
	terminator_pos = line_pos;
	word_num_quoted_sections = 0;
}

//**************************************************************************************
std::string ParseFieldNameOperation::Perform()
{
	//NB. the quotes will always at least be balanced - see iodev.
	for (;;) {

		//...So the end of the line must be outside quotes
		if ((size_t)cursor == line.length()) {
			TestForTerminatorWordOrAppend();
			break;
		}

		c = line[cursor];

		//Like a string literal a field name may have several quoted sections
		if (c == '\'') {

			//We are currently inside quotes
			if (in_quotes) {

				//It's a double quote - this means a single one actually in the string
				if (pending_possible_doublequote) {
					AppendChar(); //unlikely in field names!
					pending_possible_doublequote = false;
					word_num_quoted_sections--;
				}

				//First one, do nothing and wait and see
				else {
					pending_possible_doublequote = true;
				}
			}

			//We had dropped outside quotes, but now we're back in again
			else {
				in_quotes = true;
				word_num_quoted_sections++;
			}
		}

		//Non-quote characters
		else {

			//Have we just emerged from a quoted section?
			if (in_quotes && pending_possible_doublequote) {	//prev char was a quote
				in_quotes = false;								//so we're outside now
				pending_possible_doublequote = false;
			}

			//Inside quotes anything goes
			if (in_quotes)
				AppendChar();

			//Outside quotes we may reach a terminator character or word
			else {

				//Space is always a possible terminator
				if (c == ' ') {
					if (cprev != ' ')
						if (TestForTerminatorWordOrAppend())
							break;
				}

				//Other characters might be.  This issue has a couple of kluges
				//active - see detailed comments in ulfldval.cpp.
				//V3.0.  Changed so we can do a special awkward test for LIKE patterns.
				//else if (check_terminator_chars && strchr("@#=();,+-*/<>^¬$%", c)) {
				else if (!custom_terminator_chars && strchr("@#=();,+-*/<>^¬$%", c)) {
					TestForTerminatorWordOrAppend();
					break;
				}

				else if (custom_terminator_chars && strchr(custom_terminator_chars, c)) {
					TestForTerminatorWordOrAppend();
					break;
				}

				//It's a regular character, just outside the quotes - add it to the string.
				else {
					AppendChar();
				}
			}
		}

		cprev = c;
		cursor++;
	}

	line_pos = terminator_pos;
	return fname;
}

//**************************************************************************************
void ParseFieldNameOperation::AppendChar()
{
	//Make a note of the start of each word in case it's a reserved word
	if (word.length() == 0)
		word_pos = cursor;

	word.append(1, c);  
}

//**************************************************************************************
bool ParseFieldNameOperation::TestForTerminatorWordOrAppend()
{
	if (word.length() > 0) {

		//Have to test for reserved words if we're outside quotes
		if (word_num_quoted_sections == 0) {

//			if (ReservedWords::Find(word)) {

			//V1.1 18/6/06
			//No longer test against the full set of reserved words every time.  In many
			//cases there is no test to do at all.
			if (terminator_words) {
				bool is_terminator_word = false;

				//Couple of special cases where table look up is used.  Ugly code.
				if (terminator_words->length() == 1) {
					char usage = (*terminator_words)[0];
					if (usage == 'F')
						is_terminator_word = ReservedWords::FindFDWord(word);
					else if (usage == 'E')
						is_terminator_word = ReservedWords::FindExprWord(word);
					else
						is_terminator_word = ReservedWords::FindPrtExprWord(word);
				}
				else
					//But allows use of quick Oneof where only one or two words to check for
					is_terminator_word = util::OneOf(word, *terminator_words);

				if (is_terminator_word) {
					//So the name actually ended at the separator before the terminator word
					terminator_pos = word_pos;
					if (terminator_pos > start_pos)
						terminator_pos--;

					terminating_word = word;
					return true;
				}
			}
		}

		//The compressing of multiple embedded spaces is achieved by 
		//ignoring them and then just inserting a space between words.
		if (fname.length() > 0)
			fname.append(1, ' ');

		fname.append(word);
		word = std::string();
	}

	word_num_quoted_sections = 0;
	terminator_pos = cursor;
	return false;
}







//**************************************************************************************
//V3.0 as above
//int FieldNameParser::ParseFieldName(const std::string& i, const std::string* tw, bool tc)
int FieldNameParser::ParseFieldName(const std::string& i, const std::string* tw, const char* tc)
{
	int cursor = 0;

	//V1.1: 18/6/06 - terminator words
	ParseFieldNameOperation po(i, cursor, tw, tc);
	fieldname = po.Perform();

	if (fieldname.length() == 0) {
		if (i.find_first_not_of(" ", cursor) == std::string::npos)
			throw Exception(DBA_FIELDNAME_ERROR, "No field name given");
		else
			throw Exception(DBA_FIELDNAME_ERROR, 
				"Malformed command (put reserved words/characters in quotes?)");
	}

	return cursor;
}




} //close namespace


