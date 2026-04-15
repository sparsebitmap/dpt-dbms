
#if !defined(BB_LINIOCON)
#define BB_LINIOCON

#include "lineio.h"

namespace dpt {
namespace util {

class StdioConsoleLineIO : public LineInput, public LineOutput
{
protected:
	int LineInputPhysicalReadLine(char*);
	void LineOutputPhysicalWrite(const char*, int len);
	void LineOutputPhysicalNewLine();

public:
	StdioConsoleLineIO() : LineInput("Stdio console"), LineOutput("Stdio console") {}
	const char* GetName() const {return "Stdio console";}

};

}} //close namespace

#endif
