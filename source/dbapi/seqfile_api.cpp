
#include "stdafx.h"

#include "dbapi\seqfile.h"
#include "seqfile.h"

namespace dpt {


APISequentialFileView::APISequentialFileView(SequentialFileView* t) 
: LineInput(t->LineInput::GetName()), LineOutput(t->LineOutput::GetName()), target(t) {}
APISequentialFileView::APISequentialFileView(const APISequentialFileView& t) 
: LineInput(t.LineInput::GetName()), LineOutput(t.LineOutput::GetName()), target(t.target) {}



//**************************************
int APISequentialFileView::LineInputPhysicalReadLine(char* buff)
{
	return target->LineInputPhysicalReadLine(buff);
}

void APISequentialFileView::LineOutputPhysicalWrite(const char* c, int l)
{
	target->LineOutputPhysicalWrite(c, l);
}

void APISequentialFileView::LineOutputPhysicalNewLine()
{
	target->LineOutputPhysicalNewLine();
}




//**************************************
bool APISequentialFileView::ReadNoCRLF(char* buff, int len)
{
	return target->ReadNoCRLF(buff, len);
}

void APISequentialFileView::WriteNoCRLF(const char* buff, int len)
{
	target->WriteNoCRLF(buff, len);
}


//**************************************
int APISequentialFileView::MRL() 
{
	return target->MRL();
}

} //close namespace


