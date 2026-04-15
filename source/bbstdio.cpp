
#include "stdafx.h"


#include <io.h>
#include <direct.h> //Aug 07.  For _mkdir()	

#include "lockable.h"
#include "bbstdio.h"
#include "winutil.h"
#include "dataconv.h"
#include "parsing.h"
#include "msg_util.h"
#include "except.h"

#ifdef DEBUG_V221_STDIO
#include "iowrappers.h"
#endif

namespace dpt { namespace util {

#define COPY_MAXCHUNK 65536

//*******************************************************************************
std::string MakeStdioDirectoryErrorString(
	const std::string& local_function,
	const std::string& lowlevel_function,
	const std::string& directory_name,
	int err)
{
	std::string s;
	
	s = "Directory access error in local function: ";
	s.append(local_function);

	s.append(". Calling system routine: ");
	s.append(lowlevel_function);

	s.append(". Directory name: ");
	s.append(directory_name);

	s.append(". System errno was: ");
	s.append(util::IntToString(err));

	//Some helpful suggestions
	switch (err) {
	case EEXIST :
		s.append(" (Directory already exists)");
		break;
	case EINVAL :
		s.append(" (Invalid function argument)");
		break;
	case ENOENT :
		s.append(" (Directory or parent directory does not exist)");
		break;
//	Could this ever happen creating a directory?!
//	case ENOSPC :
//		s.append(" (The disk is full)");
//		break;
	case ENOTEMPTY :
		s.append(" (Not a directory, not empty, or root directory)");
		break;
	default:
		s.append(" (Strange - this error should never occur here)");
	}
	
	return s;
}

//********************************************
std::string MakeStdioFileDirectoryErrorString(
	const std::string& local_function,
	const std::string& lowlevel_function,
	const std::string& file_name,
	int file_handle,
	int err)
{
	std::string s;

	s = "File/directory error in local function: ";
	s.append(local_function);

	s.append(". Calling system routine: ");
	s.append(lowlevel_function);

	s.append(". File name: ");
	s.append(file_name);

	//V2 - Nov 06.  The message can be unreadably long
	if (file_handle != -1) {
		s.append(". File handle: ");
		s.append(util::IntToString(file_handle));
	}

	s.append(". System errno was: ");
	s.append(util::IntToString(err));

	switch (err) {
	case EACCES :
		s.append(" (File is in use elsewhere, or is a read-only file, or is a directory)");
		break;
	case EBADF :
		s.append(" (File handle invalid or used improperly)");
		break;
	case EEXIST :
		s.append(" (File already exists)");
		break;
	case EMFILE :
		s.append(" (No file handles left)");
		break;
	case EINVAL :
		s.append(" (Invalid function argument)");
		break;
	case ENOENT :
		s.append(" (File does not exist)");
		break;
	case ENOSPC :
		s.append(" (The disk is full)");
		break;
	default:
		s.append(" (Strange - this error should never occur here)");
	}
	
	return s;
}

//********************************************
std::string MakeStdioFileErrorString(
	const std::string& local_function,
	const std::string& lowlevel_function,
	const std::string& file_name,
	int file_handle,
	_int64 expected_value,
	_int64 actual_value,
	int err)
{
	std::string s;

	s = "File IO error in local function: ";
	s.append(local_function);

	s.append(". Calling system routine: ");
	s.append(lowlevel_function);

	s.append(". File name: ");
	s.append(file_name);

	//V2 - Nov 06.  The message can be unreadably long
	if (file_handle != -1) {
		s.append(". File handle: ");
		s.append(util::IntToString(file_handle));
	}

	s.append(". Expected value: ");
	s.append(util::Int64ToString(expected_value));

	s.append(". Got value: ");
	s.append(util::Int64ToString(actual_value));

	s.append(". System errno was: ");
	s.append(util::IntToString(err));

	switch (err) {
	case EBADF :
		s.append(" (File handle invalid or used improperly)");
		break;
	case EINVAL :
		s.append(" (Invalid function argument)");
		break;
	case ENOSPC :
		s.append(" (The disk is full)");
		break;
	default:
		s.append(" (Strange - this error should never occur here)");
	}
	
	return s;
}

//********************************************
std::string StdioFileErrorBriefText(const std::string& fulltext)
{
	size_t rbrack = fulltext.rfind(')');
	size_t lbrack = fulltext.rfind('(', rbrack);
	if (rbrack == std::string::npos || lbrack == std::string::npos)
		return fulltext;
	else
		return fulltext.substr(lbrack+1, rbrack - lbrack - 1);
}


//*****************************************************************************************
//Debugging V2.21
/*
What I suspect is that something is closing the wrong file.  This could then cause a chain
of knock-on errors, since the object whose file was closed then either gets a write error
or if the handle has been reused writes to the wrong file.  Also when it tries to close
its file, it either gets a handle error or closes somebody else's file, continuing the chain.

This theory is the only thing I can come up with, based on current information and the
moderate examination of the code that I've done, that would explain the variety of STDIO file 
handle related errors that Robert Waggoner is getting at his site.

The plan is that all uses of the CRT _close (if they call it some time after open - long
enough for the variable to have got corrupted), make an effort to remember the handle as
it was at open time, to make sure they close their own original handle, not a different
one.

In addition, the read and write functions where Robert's errors are occurring also perform
the mismatch test.  I think this really will just cause the error to be thrown slightly
earlier than it would be if it was left till close time, but should be worthwhile as
the sooner we see the problem the easier it should be to link it to what happened just
before.  If we don't see till close that might be a long time after the original corruption.

*/
//*****************************************************************************************
#ifdef DEBUG_V221_STDIO

void HandleCheckError(
	const char* cls, const char* usage, int handle, 
	int orig_handle, bool& corrupt_flagged, const std::string& name, const std::string& extra_info)
{
	//Only flag once per object to prevent recursive throws
	if (corrupt_flagged)
		return;
	corrupt_flagged = true;

	static Lockable dumplock;
	LockingSentry ls(&dumplock);

	std::string msg = "Bug: CRT IO error detected, diagnostic report "; 

	try {

		//Raise this thread's priority, since we may be accessing other threads' files.
		SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);

		std::string dumpfname = std::string(util::StartupWorkingDirectory()).append("\\")
			.append("#DUMPS\\STDIO_BAD_HANDLE.TXT");
//		std::string dumpfname = "#DUMPS\\STDIO_BAD_HANDLE.TXT";
		util::StdioEnsureDirectoryExists(dumpfname.c_str());
		dumpfname = win::GetUnusedFileName(dumpfname);

		util::BBStdioFile f(dumpfname.c_str(), util::STDIO_NEW);

		f.WriteLine(std::string("Corrupt file handle detected in class ")
			.append(cls).append(" during ").append(usage));

		f.WriteLine(std::string("On ").append(win::GetCTime()));
		f.WriteLine("");

		f.WriteLine(std::string("Desired target object: ").append(name));

		if (extra_info.length() > 0)
			f.WriteLine(std::string("(").append(extra_info).append(")"));

		f.WriteLine(std::string("Handle should be ").append(util::IntToString(orig_handle))
			.append(", but is now ").append(util::IntToString(handle)));

		//Print out some contents of the other file since we can not get its name
		f.WriteLine(std::string());
		f.WriteLine(std::string("We know the following about the file on ")
			.append(util::IntToString(handle)));

		try {
			f.WriteLine(std::string("- Size: ")
				.append(util::IntToString(util::StdioFLength(handle, "DUMMY"))));
			f.WriteLine(std::string("- Current file pointer: ")
				.append(util::IntToString(util::StdioTell(handle, "DUMMY"))));

			//This is only probable because all the handles might be mixed up by now.
			//Also the name cache table is not tight - e.g. entries not cleared on close.
			f.WriteLine(std::string("- Path (probable): ")
				.append(GetStdioFileName(handle)));
		}
		catch (...) {
			f.WriteLine("- It's an invalid handle");
		}

		msg.append("was written to ");
		msg.append(dumpfname);
	}
	catch (Exception& e) {
		msg.append("could not be produced - ");
		msg.append(e.What());
	}
	catch (...) {
		msg.append("could not be produced - unknown reason");
	}

	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);

	throw Exception(MISC_DEBUG_USRAUDIT_ERROR, msg);
}

//****************************************************************************************
//Annoying that there's no way to get this - we have to remember.  Usually unnecessary
//but I think this will be handy for the debugging above.
//****************************************************************************************
static std::vector<std::string> stdio_fnames(1000);
static Lockable sfnlock;

void NoteStdioFileName(int h, const std::string& s)
{
	LockingSentry ls(&sfnlock);

	if (h >= stdio_fnames.size())
		stdio_fnames.resize(h);

	if (h < 0)
		return;

	stdio_fnames[h] = s;
}

std::string GetStdioFileName(int h) 
{
	LockingSentry ls(&sfnlock);

	std::string result;
	if (h >= 0 && h < stdio_fnames.size())
		result = stdio_fnames[h];

	if (result.length() == 0)
		return "Unknown";
	else
		return result;
}
#endif









//****************************************************************************************
//OS file info
//****************************************************************************************
int StdioFLength(int handle, const char* filename) 
{
	errno = 0;
	int rc = _filelength(handle);
	if (rc == -1)
		throw Exception(UTIL_STDIO_ERROR, MakeStdioFileErrorString
			("StdioFLength()", "_filelength()", filename, handle, 0, -1, errno));

	return rc;
}

//********************************************
time_t GetStdioFileUpdatedTime(int handle, const char* filename, bool commit)
{
	//If we don't commit any previous writes the last update time may not be reported
	//correctly since the OS may be buffering writes.  (It seems that _fstat "cuts under"
	//the buffering).
	if (commit)
		StdioCommit(handle, filename);

	struct _stat s;
	GetStdioFStat(handle, filename, &s);

	return s.st_mtime;
}

//********************************************
void GetStdioFStat(int handle, const char* filename, struct _stat* s)
{
	if (_fstat(handle, s))
		throw Exception(UTIL_STDIO_ERROR,	MakeStdioFileErrorString
			("StdIOGetFstat()", "_fstat()", filename, handle, 0, -1, errno));
}











//*****************************************************************************************
//Opening and closing
//*****************************************************************************************
int StdioSopen(const char* filename, int disp, int sh, int perm) 
{
	int handle;

	//Stdio has no "nocreate" option like iostream, so code it specifically.  Note however
	//that _sopen() will not create a directory.  See EnsureDirectoryExists().
	if (disp == STDIO_OLD || disp == STDIO_CLR || disp == STDIO_MOD || disp == STDIO_RDONLY) {

		//V2.16 April 09.  The open mode must include binary even here, otherwise trailing
		//X'14' (EOF) gets stripped from a file just from this open and close.
		//int tempdisp = (disp == STDIO_RDONLY) ? _O_RDONLY : _O_RDWR;
		int tempdisp = STDIO_RDONLY;

		errno = 0;
		int fh = _sopen(filename, tempdisp, sh, perm);
		if (fh == -1) {
			int ecode = (errno == EACCES) ? UTIL_LINEIO_SHR : UTIL_LINEIO_ERROR;
			throw Exception(ecode, MakeStdioFileDirectoryErrorString
				("StdioSopen()", "_sopen(a)", filename, -1, errno));
		}

		_close(fh);
	}

	//Now open the file properly
	errno = 0;

	//NOTE on share modes (sopen parm 3):
	//- these apply to how the opening process blocks other processes.
	//NOTE on permission modes (sopen parm 4):
	//- these are written permanently into the file and apply for its whole life
	handle = _sopen(filename, disp, sh, perm);
	if (handle == -1) {
		int ecode = (errno == EACCES) ? UTIL_LINEIO_SHR : UTIL_LINEIO_ERROR;
		throw Exception(ecode, MakeStdioFileDirectoryErrorString
			("StdioSopen()", "_sopen(b)", filename, handle, errno));
	}

#ifdef DEBUG_V221_STDIO
	NoteStdioFileName(handle, filename);
#endif

	return handle;
}

//********************************************
void StdioClose(int handle) 
{
	if (handle != -1)
		_close(handle);
}










//*****************************************************************************************
//Reading
//*****************************************************************************************
int StdioLseek(int handle, const char* filename, int pos, int base) 
{
	errno = 0;
	int rc = _lseek(handle, pos, base);
	if (rc == -1)
		throw Exception(UTIL_STDIO_ERROR, MakeStdioFileErrorString
			("StdioLseek()", "_lseek()", filename, handle, pos, rc, errno));

	return rc;
}

//*****************************************************************************************
int StdioRead(int handle, const char* filename, void* buff, unsigned int numbytes) 
{
	errno = 0;

	int readlen = _read(handle, buff, numbytes);
	if (readlen == -1)
		throw Exception(UTIL_STDIO_ERROR, MakeStdioFileErrorString
			("StdioRead()", "_read()", filename, handle, numbytes, -1, errno));

	return readlen;
}

//********************************************
//Factored this out from pure LineInput for general use with STDIO files.
//Ideally needs rewriting to make it less tailored for use in the LineInput situation
//but works OK.
//********************************************
int StdioReadLine
(int handle, const char* filename, char* dest, char* read_buffer, bool& simulated_eol) 
{
	int logical_line_length = 0;
	char prev_char = 0;

	//Block up calls to the IO system into chunks.  On reflection the IO system would 
	//probably buffer up single-byte calls anyway, and the routine below might actually 
	//degrade performance because of the need to lseek back after each line!  Oh well
	//it was fun to code :-)  Benchmark this and rewrite it later if I feel like it.
	//What was I thinking?
	int CHUNKSIZE = 100;
	int buffer_offset = -CHUNKSIZE;
	int chunk_offset = CHUNKSIZE;
	int chunk_read = 0;

	//Read byte-by-byte to look for the new line.  In SharableLineIO this is necessary,
	//so I copied it here.  Could possibly have used fgets().  Never mind.
	for (;;) {

		//Read a chunk every so often
		if (chunk_offset == CHUNKSIZE) {

			buffer_offset += CHUNKSIZE;
			chunk_offset = 0;

			errno = 0;
			chunk_read = _read(handle, (read_buffer + buffer_offset), CHUNKSIZE);

			if (chunk_read == -1) {
				//read error - set string to null and throw
				dest[0] = 0;
				throw Exception(UTIL_LINEIO_ERROR, MakeStdioFileErrorString
					("StdioReadLine", "_read()", 
					filename, handle, CHUNKSIZE, -1, errno));
			}
			else if (chunk_read == 0) {
				//No more data at all
				return LINEIPEOF;			
			}
			else if (chunk_read < CHUNKSIZE) {
				//Last partial chunk - force the scan to find a newline
				read_buffer[buffer_offset + chunk_read] = '\n';
			}
		}

		//Examine characters one by one for newline
		char* pc = read_buffer + buffer_offset + chunk_offset;
		char c = *pc;

		//Check for new line
		if (c != '\n')
			dest[logical_line_length] = c;

		else {

			//Discard CR before LF only
			if (prev_char == '\r') {
				logical_line_length--;
				pc--;
			}

			//Replace the newline with end-of-string
			dest[logical_line_length] = 0;

			//Do the same in the read buffer for the benefit of later PrevLine() call
			*pc = 0;

			break;
		}

		prev_char = c;
		chunk_offset++;
		logical_line_length++;

		if (logical_line_length > (32760 - CHUNKSIZE) ) {
			//V2 - Nov 06.  Reformatted so the bracketed text has standalone meaning.
			throw Exception(UTIL_LINEIO_ERROR,
//				std::string("Physical input line too long in file: ")
//				.append(filename)
//				.append(".  (Calling function StdioReadLine())."));
				std::string("Logical error in local function StdioReadLine(). File name: ")
				.append(filename)
				.append(".  (CRLF-delimited line exceeded 32K in length)"));
		}
	}

	//See comment at the top
	int backtrack = chunk_read - chunk_offset - 1;
	if (backtrack > 0) 
		_lseek(handle, -backtrack, SEEK_CUR);

	//Caller may or may not care whether the last line had EOL chars.  This tests if
	//the EOL was the extra \n appended above.
	if (chunk_read <= CHUNKSIZE && chunk_offset == chunk_read)
		simulated_eol = true;

	//Unlike SharableLineIO the logical line length is more useful to the caller here,
	return logical_line_length;
}

//********************************************
//V2 - Nov 06.
//Introduced for $functions to provide neat access in User Language, but generally useful:
//Offset = -1 means don't reposition.
//********************************************
int StdioReadAppendString
(int handle, const char* filename, std::string& target, int total_required, int offset)
{
	if (total_required < 0)
		total_required = StdioFLength(handle, filename);

	if (offset >= 0)
		StdioLseek(handle, filename, offset, SEEK_SET);

	int startlen = target.length();
	int still_required = total_required;
	char buff[COPY_MAXCHUNK];

	while (still_required > 0) {
		int try_chunk = (still_required > COPY_MAXCHUNK) ? COPY_MAXCHUNK : still_required;
		int got_chunk = StdioRead(handle, filename, buff, try_chunk);

		target.append(buff, got_chunk);

		//Reached EOF
		if (got_chunk < try_chunk)
			break;

		still_required -= got_chunk;
	}

	return target.length() - startlen;
}

//********************************************
int StdioTell(int handle, const char* filename) 
{
	errno = 0;
	int rc = _tell(handle);
	if (rc == -1)
		throw Exception(UTIL_STDIO_ERROR, MakeStdioFileErrorString
			("StdIOTell()", "_tell()", filename, handle, 0, -1, errno));

	return rc;
}












//*****************************************************************************************
//Writing
//*****************************************************************************************
void StdioWrite(int handle, const char* filename, const void* source, unsigned int len) 
{
	errno = 0;
	if (_write(handle, source, len) == -1)
		throw Exception(UTIL_STDIO_ERROR,	MakeStdioFileErrorString
			("StdioWrite()", "_write()", filename, handle, len, -1, errno));
}

//********************************************
void StdioWriteString(int handle, const char* filename, const std::string& source, int offset)
{
	if (offset >= 0)
		StdioLseek(handle, filename, offset, SEEK_SET);

	//No need to chunk this up like read above - leave it to the OS
	StdioWrite(handle, filename, source.data(), source.length());
}

//********************************************
//Used in editor save processing
int OverwriteStdioFileFromBufferWithEOLs
(int handle, const char* filename, std::vector<std::string>& buff)
{
	//Empty the file out
	StdioChsize(handle, filename, 0);

	//Go back to the start
	StdioLseek(handle, filename, 0, SEEK_SET);

	//Then write it from the buffer
	int longest_line = 0;
	for (size_t x = 0; x < buff.size(); x++) {

		std::string& line = buff[x];

		//The buffer supposedly already has EOLS, so no need to write them
		StdioWrite(handle, filename, line.c_str(), line.length());

		int len = line.length();

		//Most lines have an EOL sequence, although the last one may not
		if (len > 0)
			if (line[len-1] == '\n')
				len--;
		if (len > 0)
			if (line[len-1] == '\r')
				len--;

		if (len > longest_line)
			longest_line = len;
	}

	SetStdioFileUpdatedTime(handle, filename, 0);

	return longest_line;
}

//********************************************
void SetStdioUTim(int handle, const char* filename, struct _utimbuf* u)
{
	if (_futime(handle, u))
		throw Exception(UTIL_STDIO_ERROR,	MakeStdioFileErrorString
			("StdIOSetUTim()", "_futime()", filename, handle, 0, -1, errno));
}

//*****************************************************************************************
void StdioChsize(int handle, const char* filename, int newsize) 
{
	errno = 0;
	if (_chsize(handle, newsize))
		throw Exception(UTIL_STDIO_ERROR, MakeStdioFileDirectoryErrorString
			("StdioChsize()", "_chsize()", filename, handle, errno));
}

//********************************************
void SetStdioFileUpdatedTime(int handle, const char* filename, time_t newtime)
{
	StdioCommit(handle, filename);

	if (newtime == 0)
		SetStdioUTim(handle, filename, NULL); //now
	else {
		//Both values must be populated in __futime, so get the access time to put back in
		struct _stat s;
		GetStdioFStat(handle, filename, &s);

		struct _utimbuf u;
		u.actime = s.st_atime;
		u.modtime = newtime;

		SetStdioUTim(handle, filename, &u);
	}
}












//*****************************************************************************************
//Miscellaneous
//*****************************************************************************************
void StdioCommit(int handle, const char* filename) 
{
	errno = 0;
	if (_commit(handle) == -1)
		throw Exception(UTIL_STDIO_ERROR, MakeStdioFileErrorString
			("StdIOCommit()", "_commit()", filename, handle, 0, -1, errno));
}

//********************************************
void StdioDeleteFile(int& handle, const char* filename) 
{
	//You can't delete an open file using these functions
	if (handle != -1)
		_close(handle);

	int old_handle = handle;
	handle = -1;

	errno = 0;
	if (remove(filename) != 0)
		throw Exception(UTIL_STDIO_ERROR,	MakeStdioFileDirectoryErrorString
			("StdioDeleteFile()", "remove()", 
			filename, old_handle, errno));
}

//********************************************
bool StdioEnsureDirectoryExists(const char* n)
{
	std::string name(n);

	//The user can give a file name if they like.
	size_t dot = name.find_last_of('.');
	size_t slash = name.find_last_of("\\/");

	//A file name was given
	if (dot != std::string::npos) {
		
		//No directory specified - null directory "exists" for this purpose
		if (slash == std::string::npos)
			return false;

		//Strip off the file name and preceding slash
		name.resize(slash);
	}

	//Directory given
	else {

		//Strip a final slash as standard, although _mkdir() will take it either way.
		if (name[name.length()-1] == '\\' || name[name.length()-1] == '/')
			name.resize(name.length()-1);
	}

	errno = 0;
	if (_mkdir(name.c_str()) == 0)
		return true;
	
	//This is OK - we just want to ensure it exists
	if (errno == EEXIST)
		return false;

	if (errno != ENOENT) {
		throw Exception(UTIL_STDIO_ERROR, MakeStdioDirectoryErrorString
			("StdioEnsureDirectoryExists()", "_mkdir()", name, errno));
	}

	//Probably parent directory doesn't exist (mkdir can only create one level at a 
	//time).  Therefore strip back and repeat till we get success.
	std::string dummyfile = std::string(name).append(".dummy");
	util::StdioEnsureDirectoryExists(dummyfile.c_str());

	errno = 0;
	if (_mkdir(name.c_str()) != 0) {
		throw Exception(UTIL_STDIO_ERROR, MakeStdioDirectoryErrorString
			("StdioEnsureDirectoryExists()", "_mkdir() (recursive)", name, errno));
	}

	return true;
}

//********************************************
std::string GetCWD()
{
	char buff[_MAX_PATH]; 
	return _getcwd(buff, _MAX_PATH);
}

//********************************************
//V2.29
void SetCWD(const std::string& file_or_dir)
{
	std::string path = file_or_dir;

	//Allow quoted
	util::DeBlank(path, '\"');

	//Allow a full file path to be entered, and we'll switch to its directory
	size_t dotpos = path.find('.');
	if (dotpos != std::string::npos) {
		size_t slashpos = path.rfind('\\');
		if (slashpos != std::string::npos)
			path.resize(slashpos);
		else
			throw Exception(UTIL_STDIO_ERROR, 
				"SetCWD() called with unqualified or invalid file name");
	}

	if (_chdir(path.c_str()) == -1) {
		throw Exception(UTIL_STDIO_ERROR, MakeStdioDirectoryErrorString
			("SetCWD()", "_chdir()", path, errno));
	}
}

//********************************************
static char swdbuff[_MAX_PATH];
static char* dummy = _getcwd(swdbuff, _MAX_PATH);

std::string StartupWorkingDirectory()
{
	//The VCC incremental builder doesn't seem to like it if we return a reference to a
	//statically allocated string or array here - forces tons of recompiles every build.
	//No matter, this is not a critical function so a string build here is fine.
	return std::string(swdbuff);
}

//Call this before doing anything else
void NoteChangedStartupWorkingDirectory(const std::string& newswd) {
	strncpy(swdbuff, newswd.c_str(), _MAX_PATH);
	swdbuff[_MAX_PATH - 1] = 0;
}


//********************************************
//V3.0
//void StdioCopyFile(int to_handle, const char* to_filename, 
//				   int from_handle, const char* from_filename, bool truncate) 
_int64 StdioCopyFile
(int to_handle, const char* to_filename, int from_handle, const char* from_filename, 
 _int64 from_pos, _int64 copy_len, bool truncate) 
{
	//The caller is assumed to have positioned the destination file pointer 
	//appropriately if they want to append or overwrite somewhere.
	if (truncate) {
		StdioChsize(to_handle, to_filename, 0);
		StdioLseek(to_handle, to_filename, 0, SEEK_SET);
	}

//	StdioLseek(from_handle, from_filename, 0, SEEK_SET);
	StdioLseekI64(from_handle, from_filename, from_pos, SEEK_SET);

	char buff[COPY_MAXCHUNK];
	_int64 bytes_copied = 0;

	//Requested len -1 means keep going till source file is exhausted
	while (bytes_copied != copy_len) {

		//Read whole chunk or whatever little bit we need to.  This is superior to how it
		//worked pre V3.0 (always did a full chunk and discarded some) because now we leave 
		//the source file pointer conveniently placed directly after the copied section.
		int bytes_to_read = COPY_MAXCHUNK;
		if (copy_len != -1) {
			_int64 bytes_left_to_copy = copy_len - bytes_copied;
			if (bytes_left_to_copy < COPY_MAXCHUNK) 
				bytes_to_read = bytes_left_to_copy;
		}

		int bytes_read = StdioRead(from_handle, from_filename, buff, bytes_to_read);

		if (bytes_read > 0) {
			StdioWrite(to_handle, to_filename, buff, bytes_read);
			bytes_copied += bytes_read;
		}

		//That was the last chunk of the source file
		if (bytes_read < bytes_to_read)
			break;
	}

	return bytes_copied;
}












//*****************************************************************************************
//64 bit versions of some things
//*****************************************************************************************
_int64 StdioTellI64(int handle, const char* filename) 
{
	errno = 0;
	_int64 rc = _telli64(handle);
	if (rc == -1)
		throw Exception(UTIL_STDIO_ERROR, MakeStdioFileErrorString
			("StdIOTellI64()", "_telli64()", filename, handle, 0, -1, errno));

	return rc;
}

//********************************************
_int64 StdioFLengthI64(int handle, const char* filename) 
{
	errno = 0;
	_int64 rc = _filelengthi64(handle);
	if (rc == -1)
		throw Exception(UTIL_STDIO_ERROR, MakeStdioFileErrorString
			("StdioFLengthI64()", "_filelengthi64()", filename, handle, 0, -1, errno));

	return rc;
}

//********************************************
_int64 StdioLseekI64(int handle, const char* filename, _int64 pos, int base) 
{
	errno = 0;
	_int64 rc = _lseeki64(handle, pos, base);
	if (rc == -1)
		throw Exception(UTIL_STDIO_ERROR, MakeStdioFileErrorString
			("StdioLseekI64()", "_lseeki64()", filename, handle, pos, rc, errno));

	return rc;
}

//********************************************
void StdioChsizeI64(int handle, const char* filename, _int64 newsize) 
{
	//There is no 64 bit STDIO version of this, but in many cases that's OK
	_int64 temp = INT_MAX; //prevent compiler temporary being truncated
	if (newsize <= temp) {
		StdioChsize(handle, filename, newsize);
		return;
	}

	//If we're around the line this may only have to do a small amount of truncation,
	//so it's better than always resetting to zero and then going up again.
	StdioChsize(handle, filename, INT_MAX);

	//Now seek out there and write a byte
	StdioLseekI64(handle, filename, newsize - 1, SEEK_SET);
	StdioWrite(handle, filename, "\0", 1);
}

//********************************************
//V3.0

#define SEARCH_BUFFER_SIZE 65536

_int64 StdioFindString
(int handle, const char* filename, const std::string& target, _int64 startpos)
{
	if (target.length() > SEARCH_BUFFER_SIZE) {
		throw Exception(UTIL_STDIO_ERROR, 
			"File search func max target string length is 64K");
	}

	char buff[SEARCH_BUFFER_SIZE];
	_int64 buffstart_as_filepos = startpos;

	//We can start at a specific position or the current file pointer position (-1 option)
	if (startpos == -1)
		buffstart_as_filepos = StdioTellI64(handle, filename);
	else
		StdioLseekI64(handle, filename, startpos, SEEK_SET);

	//The first read chunk is for the whole buffer size
	int readchunk = SEARCH_BUFFER_SIZE;

	//And goes to the very start of the buffer
	int retrylen = 0;

	for (;;) {
		int i = StdioRead(handle, filename, buff + retrylen, readchunk);

		//Find string in buffer
		if (i > 0) {
			int findpos = BufferFind(buff, SEARCH_BUFFER_SIZE, target.c_str(), target.length());
			if (findpos != -1)
				return buffstart_as_filepos + findpos;
		}

		//That was the last part-chunk
		if (i < readchunk)
			break;

		//Get ready for the next chunk.  Each time after the first one, retry a small section
		//at the end in case of overlap by moving it back and filereading not quite as much.
		retrylen = target.length() - 1;
		readchunk = SEARCH_BUFFER_SIZE - retrylen;
		memmove(buff, buff + readchunk, retrylen);
		buffstart_as_filepos += readchunk;
	}

	return -1;
}

}} //close namespace
