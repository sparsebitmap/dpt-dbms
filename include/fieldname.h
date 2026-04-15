//******************************************************************************************
//Field name parsing as used in:
// - Commands like DEFINE FIELD
// - Operands in UL expressions
// - Field names in UL file-access statements like FIND and CHANGE
//
//V1.1 18/6/06
//We are now much more forgiving with the use of reserved words, as that's how it looks
//like M204 is.  The set of reserved words used now varies according to the situation.
//******************************************************************************************

#if !defined BB_FIELDNAME
#define BB_FIELDNAME

#include <string>

namespace dpt {

class ParseFieldNameOperation {
	//Parameters
	std::string line;
	int& line_pos;

	const std::string* terminator_words;
	std::string terminating_word;
	//bool check_terminator_chars;
	const char* custom_terminator_chars;

	//Working variables
	int start_pos;
	int cursor;
	char c;
	char cprev;

	bool in_quotes;
	bool pending_possible_doublequote;
	int word_num_quoted_sections;

	std::string fname;
	std::string word;
	int word_pos;

	int terminator_pos;

public:
	//V3.0
//	ParseFieldNameOperation(const std::string&, int&, const std::string* = NULL, bool = true);
	ParseFieldNameOperation(const std::string&, int&, const std::string* = NULL, const char* terms = NULL);

	std::string Perform();
	void AppendChar();
	bool TestForTerminatorWordOrAppend();
};

//*********************************************************************
//Jul 09, in prep for V3.0.  Factored this out from the command.
class FieldNameParser {
protected:
	std::string fieldname;

public:
	//V1.1: 18/6/06 - terminator options
	//int ParseFieldName(const std::string&, const std::string* = NULL, bool = true);
	int ParseFieldName(const std::string&, const std::string* = NULL, const char* terms = NULL);

	const std::string& FieldName() {return fieldname;}
};


}	//close namespace

#endif

