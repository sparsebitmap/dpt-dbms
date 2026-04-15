//*****************************************************************************************
//Line-mode text input and output classes.
//Assorted derived classes are used thoughout the system, such as fstreams, 
//stdio files, Win32 file HANDLEs, sockets, list boxes etc.
//API programs can derive their own versions to use with the API.
//*****************************************************************************************

#if !defined(BB_LINEIO)
#define BB_LINEIO

#include <vector>
#include <string>

namespace dpt { 

namespace util {

const int LINEIPEOF = -1;
const int USERLINELEN = 32000;

//*****************************************************************************************
//Input.  
//*****************************************************************************************
class LineInput {
protected:
	std::string name;
	unsigned int last_read_length; 
	bool simulated_eol;

	//This function must be provided by derived classes
	virtual int LineInputPhysicalReadLine(char*) = 0;

	virtual void Rewind() {}

public:
	LineInput(const std::string&);
	virtual ~LineInput();

	virtual const char* GetName() const;
	virtual void* GetCustomData() {return NULL;} //V2.25

	bool ReadLine(char*); //caller must ensure buffer is big enough
	bool ReadLine(std::string&); //no worries
	unsigned int GetLastReadLength() {return last_read_length;}

	//Kluge that was required with procedures.  Probably not generally useful.
	virtual void LoadBufferIncludingEOLs(std::vector<std::string>&);
};

//*****************************************************************************************
//Output.
//*****************************************************************************************
class LineOutput {
	std::string name;
	LineOutput* secondary;

protected:
	//These two functions must be provided by derived classes
	virtual void LineOutputPhysicalWrite(const char* c, int l) = 0;
	virtual void LineOutputPhysicalNewLine() = 0;

public:
	LineOutput(const std::string&);
	virtual ~LineOutput();

	//Write to two destinations at once if desired.
	void SetSecondary(LineOutput*);

	virtual const char* GetName() const;
	virtual std::string GetDesc() const {return std::string();}
	virtual void* GetCustomData() {return NULL;} //V2.25

	void Write(const char* c, int l = -1);
	void Write(const std::string&);

	//The data length saves a strlen call (also allows the string to contain hex zeroes)
	void WriteLine(const char* c, int l = -1);
	void WriteLine(const std::string&);

	virtual void Flush() {}
};


}} //close namespace

#endif
