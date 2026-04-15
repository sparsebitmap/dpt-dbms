//*****************************************************************************************
//A mainframe/M204 style sequential file.
//*****************************************************************************************/

#if !defined(BB_API_SEQFILE)
#define BB_API_SEQFILE

#include <string>
#include "lineio.h"

namespace dpt {

class SequentialFileView;

class APISequentialFileView : public util::LineInput, public util::LineOutput {
	int LineInputPhysicalReadLine(char*);
	void LineOutputPhysicalWrite(const char*, int len);
	void LineOutputPhysicalNewLine();
public:
	SequentialFileView* target;
	APISequentialFileView(SequentialFileView*);
	APISequentialFileView(const APISequentialFileView&);
	//-----------------------------------------------------------------------------------	

	//You can also do record-mode IO on this class
	bool ReadNoCRLF(char*, int len);
	void WriteNoCRLF(const char*, int len);

	int MRL();
};

} //close namespace

#endif
