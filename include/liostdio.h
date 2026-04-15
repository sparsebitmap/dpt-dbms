//*****************************************************************************************
//C stdio stream specialisations for use where a BB LineInput or LineOutput is required
//but no sharing between threads.  If sharing is required, use SharableLineIO which
//requires the file position to be specified on all calls (since other threads may move it).
//See SharableLineIO for assorted comments some of which are relevant here too.
//*****************************************************************************************

#if !defined(BB_LIOSTDIO)
#define BB_LIOSTDIO

#include <vector>
#include <string>

#include <io.h>				//all the (non_ANSI) _xxxx file access functions
#include <sys\stat.h>

#include "bbstdio.h"
#include "lineio.h"
#include "lockable.h"

namespace dpt {

//Friends - call private constructors
class IODev1;
class IODev3;
	
namespace util {

//*****************************************************************************************
class StdIOLineInput : public LineInput {
	int handle;
	bool created_here;
	char read_buffer[32768];

#ifdef DEBUG_V221_STDIO
	int* orig_handle;
	mutable bool corrupt_flagged;
	void CorruptHandleCheck(const char* s) const {if (handle != *orig_handle) 
		HandleCheckError("StdioLineInput", s, handle, *orig_handle, corrupt_flagged, GetName());}
#endif

protected:
	int LineInputPhysicalReadLine(char*);
	void Rewind() {_lseek(handle, 0, SEEK_SET);}
	friend class IODev1;
	friend class IODev3;

public:
	StdIOLineInput(int i) 
		: LineInput("Unspecified"), handle(i), created_here(false) {}

	//_SH_DENYWR means other processes can read but not write it while we use it, and
	//_S_IREAD | _S_IWRITE is saved permanently into the file if we're creating now,
	//and means it's a read-write file.
	StdIOLineInput(const std::string&, int disp, 
		int sh = _SH_DENYWR, int perm = _S_IREAD | _S_IWRITE);

	~StdIOLineInput();
	std::string PrevLine() {return read_buffer;}
};

//*****************************************************************************************
class StdIOLineOutput : public LineOutput { 
	int handle;
	bool created_here;
	bool commit_by_line;

#ifdef DEBUG_V221_STDIO
	int* orig_handle;
	mutable bool corrupt_flagged;
	void CorruptHandleCheck(const char* s) const {if (handle != *orig_handle) 
		HandleCheckError("StdioLineOutput", s, handle, *orig_handle, corrupt_flagged, GetName());}
#endif

protected:
	virtual void LineOutputPhysicalWrite(const char*, int len);
	virtual void LineOutputPhysicalNewLine();
	friend class IODev1;
	friend class IODev3;

public:
	StdIOLineOutput(int, int = STDIO_OLD); 

	//_SH_DENYWR means other processes can read but not write it while we use it, and
	//_S_IREAD | _S_IWRITE is saved permanently into the file if we're creating now,
	//and means it's a read-write file.
	StdIOLineOutput(const std::string&, int disp, int archive_versions = 0,
		int sh = _SH_DENYWR, int perm = _S_IREAD | _S_IWRITE, bool = false);

	~StdIOLineOutput();
	void SetCommitByLine() {commit_by_line = true;}
	int GetHandle() {return handle;}

	std::string GetDesc() const;
};

//*****************************************************************************************
//This version locks across all output operations, and is ideal for the audit trail and
//other shared system level output streams which never need repositioning.
//*****************************************************************************************
class LockingStdIOLineOutput : public StdIOLineOutput {
	Lockable write_lock;
protected:
	void LineOutputPhysicalWrite(const char*, int len);
	void LineOutputPhysicalNewLine();
public:
	LockingStdIOLineOutput(int i, int disp = STDIO_OLD);
	LockingStdIOLineOutput(const char* n, int disp, int a = 0,
		int sh = _SH_DENYWR, int perm = _S_IREAD | _S_IWRITE, bool = false);
}; 

}} //close namespace

#endif
