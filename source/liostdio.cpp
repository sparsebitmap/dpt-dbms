
#include "stdafx.h"

#include "liostdio.h"

#include "dataconv.h"
#include "parsing.h"
#include "except.h"
#include "msg_util.h"
#include "bbstdio.h"
#include <direct.h>			//_getcwd

namespace dpt { namespace util {

//*****************************************************************************************
//Line input
//*****************************************************************************************
StdIOLineInput::StdIOLineInput(const std::string& fname, int disp, int sh, int perm) 
: LineInput(fname), created_here(true)
{
	//These disps don't make sense for an input file
	if (disp == STDIO_MOD || disp == STDIO_CMOD || 
		disp == STDIO_CLR || disp == STDIO_CCLR)
		throw Exception(UTIL_LINEIO_ERROR, 
			"StdioLineInput: disposition is nonsensical for an input file");

	//Since it's a read-only file
	disp &= ~_O_RDWR;
	disp |= _O_RDONLY;

	handle = StdioSopen(fname.c_str(), disp, sh, perm);

#ifdef DEBUG_V221_STDIO
	orig_handle = new int(handle);
	corrupt_flagged = false;
#endif
}

//***************************
StdIOLineInput::~StdIOLineInput() 
{
#ifdef DEBUG_V221_STDIO
	if (created_here) {
		CorruptHandleCheck("Close");
		if (!corrupt_flagged)
			_close(handle);
		else if (orig_handle)
			_close(*orig_handle);
	}

	if (orig_handle)
		delete orig_handle;
#else
	//Depending on which constructor was used...
	if (created_here)
		_close(handle);
#endif
}

//***************************
int StdIOLineInput::LineInputPhysicalReadLine(char* dest) 
{
#ifdef DEBUG_V221_STDIO
	CorruptHandleCheck("ReadLine");
#endif
	return StdioReadLine(handle, name.c_str(), dest, read_buffer, simulated_eol);
}





//*****************************************************************************************
//Line output
//*****************************************************************************************
StdIOLineOutput::StdIOLineOutput
(const std::string& fname, int disp, int archive_versions, int sh, int perm, bool readable)
: LineOutput(fname), created_here(true), commit_by_line(false)
{
	//This effectively means deleting any old version of the file, so switch to NEW mode
	if (archive_versions > 0)
		disp = STDIO_NEW;

	//Since we are using it in write-only mode.
	//V2.29 Allow readable-back files (e.g. audit trail)
	if (!readable) {
		disp &= ~_O_RDWR;
		disp |= _O_WRONLY;
	}

	//Shuffle back previous generations if archiving is required.
	if (archive_versions > 0) {

		//make up a full path just to be able to use _splitpath
		char path[_MAX_PATH];
		char drive[_MAX_DRIVE];
		char dir[_MAX_DIR];
		char prefix[_MAX_FNAME];
		char ext[_MAX_EXT];

		if (_getcwd(path, _MAX_PATH) == NULL) 
			throw Exception(UTIL_LINEIO_ERROR,
				std::string("_getcwd error archiving line o/p file ")
				.append(fname));
		if ((strlen(path) + fname.length() + 1 + 3) > _MAX_PATH)
			throw Exception(UTIL_LINEIO_ERROR,
				std::string("Path too long archiving line o/p file ")
				.append(fname));

		//get the full path details for the file, specifically the prefix component
		_splitpath(fname.c_str(), drive, dir, prefix, ext);

		if (strlen(prefix) > (_MAX_FNAME - 3))
			throw Exception(UTIL_LINEIO_ERROR,
				std::string("File prefix too long archiving line o/p file ")
				.append(fname));

		//use sprintf to add the "-nn" to the end of the prefix part
		char format[_MAX_PATH];
		strcpy(format, dir);
		strcat(format, prefix);
		strcat(format, "-%d");
		strcat(format, ext);

		//delete the very oldest version if it exists - ignore error
		char oldfname[_MAX_PATH];
		sprintf(oldfname, format, archive_versions);
		remove(oldfname);
		
		//then rename the others to be one older than they are currently.  Again 
		//ignore errors on the assumption that it just means the file's not there.
		char newfname[_MAX_FNAME];
		for (int g = archive_versions; g>0; g--) {
			if (g == 1) 
				strcpy(oldfname, fname.c_str());
			else {
				sprintf(oldfname, format, g-1); 
			}		
			sprintf(newfname, format, g);
			rename(oldfname, newfname);
		}
	}

	//Now actually open the file
	handle = StdioSopen(fname.c_str(), disp, sh, perm);

#ifdef DEBUG_V221_STDIO
	orig_handle = new int(handle);
	corrupt_flagged = false;
#endif
}


//*****************************************************************************************
StdIOLineOutput::StdIOLineOutput(int i, int disp) 
: LineOutput("Unspecified"), handle(i), created_here(false), 
commit_by_line(false)
{
	//Here we make an attempt at implementing the specified disp, even though technically
	//the OS file was already open.  It just saves the calling code having to call
	//chsize and lseek.
	errno = 0;
	const char* ftemp = "<adopted file handle>";
	if (disp & _O_TRUNC) {
		StdioChsize(handle, ftemp, 0);
		StdioLseek(handle, ftemp, 0, SEEK_SET);
	}
	else if (disp & _O_APPEND) {
		StdioLseek(handle, ftemp, 0, SEEK_END);
	}

#ifdef DEBUG_V221_STDIO
	orig_handle = new int(handle);
	corrupt_flagged = false;
#endif
}

//***************************
StdIOLineOutput::~StdIOLineOutput() 
{
#ifdef DEBUG_V221_STDIO
	if (created_here) {
		CorruptHandleCheck("Close");
		if (!corrupt_flagged)
			_close(handle);
		else if (orig_handle)
			_close(*orig_handle);
	}

	if (orig_handle)
		delete orig_handle;
#else
	//Depending on which constructor was used...
	if (created_here)
		_close(handle);
#endif
}

//***************************
std::string StdIOLineOutput::GetDesc() const
{
#ifdef DEBUG_V221_STDIO
	CorruptHandleCheck("GetDesc");
#endif
	return std::string("FH=").append(util::IntToString(handle));
}

//*****************************************************************************************
//Very straightforward implementations for output
void StdIOLineOutput::LineOutputPhysicalWrite(const char* source, int len) 
{
#ifdef DEBUG_V221_STDIO
	CorruptHandleCheck("Write");
#endif
	StdioWrite(handle, GetName(), source, len);
	if (commit_by_line)
		StdioCommit(handle, GetName());
}

//***************************
void StdIOLineOutput::LineOutputPhysicalNewLine() 
{
#ifdef DEBUG_V221_STDIO
	CorruptHandleCheck("Newline");
#endif
	StdIOLineOutput::LineOutputPhysicalWrite("\r\n", 2);
}



//*****************************************************************************************
//Locking versions, used e.g. for the bb audit trail.
//*****************************************************************************************
LockingStdIOLineOutput::LockingStdIOLineOutput(int i, int disp) 
: StdIOLineOutput(i, disp) 
#ifdef _DEBUG_LOCKS
, write_lock(std::string("LockingSTDLIO: handle ").append(util::IntToString((int)i)))
#endif
{}

//***************************
LockingStdIOLineOutput::LockingStdIOLineOutput
(const char* n, int disp, int a, int sh, int perm, bool r)
: StdIOLineOutput(n, disp, a, sh, perm, r) 
#ifdef _DEBUG_LOCKS
, write_lock(std::string("LockingSTDLIO: ").append(n))
#endif
{}

//***************************
void LockingStdIOLineOutput::LineOutputPhysicalNewLine() 
{
	LockingSentry ls(&write_lock);
	StdIOLineOutput::LineOutputPhysicalNewLine();
}

//***************************
void LockingStdIOLineOutput::LineOutputPhysicalWrite(const char* source, int len) 
{
	LockingSentry ls(&write_lock);
	StdIOLineOutput::LineOutputPhysicalWrite(source, len) ;
}

}} //close namespace
