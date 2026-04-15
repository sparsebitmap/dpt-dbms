//****************************************************************************************
//Stuff used with BB code that uses stdio file IO
//There are quite a lot of classes that use these functions, and they expose the
//ones they need to.  I really ought to get round to packaging them into a base class.
//The closest such thing at the moment is StdioLineIO which has filename based 
//construction and auto-close at destruction etc.  No big deal really.
//****************************************************************************************

#if !defined(BB_STDIO)
#define BB_STDIO

#include <string>
#include <vector>
#include <fcntl.h>
#include <sys/types.h>
#include <sys\stat.h>
#include "sys\utime.h"
#include "share.h"
#include "errno.h"

#include "lineio.h"

namespace dpt {
namespace util {

const int STDIO_OLD  = _O_RDWR | _O_BINARY;			// have to code explicitly for nocreate
const int STDIO_CLR  = _O_RDWR | _O_BINARY | _O_TRUNC;		//made-up value
const int STDIO_MOD  = _O_RDWR | _O_BINARY | _O_APPEND;
const int STDIO_NEW  = _O_RDWR | _O_BINARY | _O_CREAT | _O_EXCL;
const int STDIO_COND = _O_RDWR | _O_BINARY | _O_CREAT;
const int STDIO_CMOD = _O_RDWR | _O_BINARY | _O_CREAT | _O_APPEND;	//made-up value
const int STDIO_CCLR = _O_RDWR | _O_BINARY | _O_CREAT | _O_TRUNC;	//made-up value
const int STDIO_RDONLY = _O_RDONLY | _O_BINARY;

//Could possibly have a distinct SHR value, although at the moment the STDIO
//file is always opened in _SH_DENYWR - in other words SHR.

//****************************************************************************************
//Standard error text formatting functions
//****************************************************************************************
//1. Directory errors (e.g. creating a directory)
std::string MakeStdioDirectoryErrorString(
	const std::string& local_function,
	const std::string& lowlevel_function,
	const std::string& directory_name,
	int err);

//2. File/directory errors (e.g. creating a file)
std::string MakeStdioFileDirectoryErrorString(
	const std::string& local_function,
	const std::string& lowlevel_function,
	const std::string& file_name,
	int file_handle,
	int err);

//3. File IO errors
std::string MakeStdioFileErrorString(
	const std::string& local_function,
	const std::string& lowlevel_function,
	const std::string& file_name,
	int file_handle,
	_int64 expected_value,
	_int64 actual_value,
	int err);

//4. Extract final bracketed part which has less info but often is enough
std::string StdioFileErrorBriefText(const std::string&);

#ifdef DEBUG_V221_STDIO
void HandleCheckError(const char*, const char*, int, 
					  int, bool&, const std::string&, const std::string& = std::string());
void NoteStdioFileName(int, const std::string&);
std::string GetStdioFileName(int);
#endif

//****************************************************************************************
//All handle based ones as it stands - file name is passed in too for diagnostics, and
//could be used to process non-open files with a little rejigging.
//****************************************************************************************
time_t GetStdioFileUpdatedTime(int, const char*, bool = true);
int StdioFLength(int, const char*);
void GetStdioFStat(int, const char*, struct _stat*); //NB. contains both the above

int StdioSopen(const char*, int, int sh = _SH_DENYWR, int perm = _S_IREAD | _S_IWRITE);
void StdioClose(int);

int StdioLseek(int, const char*, int, int);
int StdioRead(int, const char*, void*, unsigned int);
int StdioReadLine(int, const char*, char*, char*, bool&); //see comments in .cpp
int StdioReadAppendString(int, const char*, std::string&, int len = -1, int offset = 0);
int StdioTell(int, const char*);

void StdioWrite(int, const char*, const void*, unsigned int);
void StdioWriteString(int, const char*, const std::string&, int offset = 0);
void StdioChsize(int, const char*, int);
int OverwriteStdioFileFromBufferWithEOLs(int, const char*, std::vector<std::string>& buff);
void SetStdioUTim(int, const char*, struct _utimbuf*);
void SetStdioFileUpdatedTime(int, const char*, time_t);

void StdioCommit(int, const char*);
_int64 StdioCopyFile(int toh, const char* tofn, int fromh, const char* fromfn, 
				   _int64 frompos = 0, _int64 len = -1, bool truncate = false);
void StdioDeleteFile(int&, const char*);
inline void StdioDeleteFile(const char* fn) {int dummy(-1); StdioDeleteFile(dummy, fn);}

bool StdioEnsureDirectoryExists(const char*); //give file name or just a directory
std::string GetCWD();
void SetCWD(const std::string&); //give file name or directory
//V2.22.  Generalize the V2.15 enhancement for if WD changes throughout run
std::string StartupWorkingDirectory();
//Allow to change after process start but before DPT startup
void NoteChangedStartupWorkingDirectory(const std::string&);

_int64 StdioLseekI64(int, const char*, _int64, int);
_int64 StdioTellI64(int, const char*);
_int64 StdioFLengthI64(int, const char*);
void StdioChsizeI64(int, const char*, _int64);
_int64 StdioFindString(int, const char*, const std::string&, _int64 = 0);


}}	//close namespace

#endif
