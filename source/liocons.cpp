
#include "stdafx.h"

#include "liocons.h"
#include "except.h"
#include "msg_util.h"
#include <iostream>

namespace dpt { namespace util {

//*****************************************************************************************
int StdioConsoleLineIO::LineInputPhysicalReadLine(char* buffer) 
{
	std::cin.getline(buffer, 32767);
	//getline() doesn't tell us how long the line was, so...
	int len = strlen(buffer);
	
	//We will only handle input lines up to a certain length.  One option would be to
	//just take the "chopped-off" portion of the line as the next input line, but that
	//would be unexpected, and this is a "line input" function after all.  (The caller
	//can then do that if they wish).
	if (len == (32767)) {
		throw Exception(UTIL_LINEIO_ERROR,
			std::string("Error in StdioConsoleLineIO::LineInputPhysicalReadLine() "
			"(line too long)."));
	}

	return len;
}

//*****************************************************************************************
void StdioConsoleLineIO::LineOutputPhysicalWrite(const char* c, int len) 
{
	//Hex zero will terminate the string here
	printf("%s", c);
}

//*****************************************************************************************
void StdioConsoleLineIO::LineOutputPhysicalNewLine() 
{
	printf("%s", "\r\n");
}

}} //close namespace
