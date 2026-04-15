
#include "stdafx.h"

#include "lineio.h"
#include "except.h"
#include "msg_util.h"

namespace dpt { namespace util {

//*****************************************************************************************
//Line Input
//*****************************************************************************************
LineInput::LineInput(const std::string& n) 
: name(n), last_read_length(0), simulated_eol(false) 
{}

LineInput::~LineInput() {}
const char* LineInput::GetName() const {return name.c_str();}

//*****************************************************************************************
//Copy the input data into the caller's own buffer area
//*****************************************************************************************
bool LineInput::ReadLine(char* dest) 
{
	simulated_eol = false;

	int len = LineInputPhysicalReadLine(dest);
	if (len == LINEIPEOF) {
		*dest = 0;
		last_read_length = 0;
		return true;
	}
	else {
		last_read_length = len;
		return false;
	}
}

//*****************************************************************************************
//Copy the input data into the caller's STL string.
//*****************************************************************************************
bool LineInput::ReadLine(std::string& s) 
{
	char buff[32768];
	int len = LineInputPhysicalReadLine(buff);
	if (len == LINEIPEOF) {
		s = std::string();
		last_read_length = 0;
		return true;
	}
	else {
		s = std::string(buff, len);
		last_read_length = len;
		return false;
	}
}

//*****************************************************************************************
//I hate this kluge but I can't be bothered right now to tidy it up.  
//It's here to get round a design decision I made early on and haven't
//got round to reviewing.  In file based specializations we gloss over whether
//the final line has a CRLF on it.  Or put another way if the file ends with CRLF
//there is not considered to be another empty line after that CRLF (unlike 
//scintilla incidentally).  But this causes a problem with procedures where
//this class is otherwise quite useful, but the user will expect the presence of 
//absence of the final CRLF to be retained in the editor just as they edit it. 
//Some people like one on there and some people don't.  Well OK most don't care :-)
//*****************************************************************************************
void LineInput::LoadBufferIncludingEOLs(std::vector<std::string>& buff)
{
	buff.clear();
	Rewind();

	std::string line;
	while (!ReadLine(line)) {

		//What this means is that if the newline in the file was explicit, the CRLF
		//would have been stripped off, so we want to put them back here.
		if (!simulated_eol)
			line.append("\r\n");
		
		buff.push_back(line);
	}
}









//*****************************************************************************************
//Line Output
//*****************************************************************************************
LineOutput::LineOutput(const std::string& n) : name(n), secondary(NULL) {}
LineOutput::~LineOutput() {}
const char* LineOutput::GetName() const {return name.c_str();}

//*******************************************
void LineOutput::SetSecondary(LineOutput* s)
{
	if (s == this) 
		throw Exception(API_MISC, "LineOutput object may not echo itself");

	secondary = s;
}
//*******************************************
void LineOutput::Write(const char* c, int l) 
{
	if (l == -1)
		l = strlen(c);

	LineOutputPhysicalWrite(c,l);

	if (secondary) 
		secondary->LineOutputPhysicalWrite(c, l);
}

void LineOutput::Write(const std::string& s) {Write(s.c_str(), s.length());}

//*******************************************
void LineOutput::WriteLine(const char* c, int l) 
{
	Write(c, l);
	LineOutputPhysicalNewLine();

	if (secondary) 
		secondary->LineOutputPhysicalNewLine();
}

void LineOutput::WriteLine(const std::string& s) {WriteLine(s.c_str(), s.length());}


}} //close namespace
