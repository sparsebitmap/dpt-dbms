//*************************************************************************************
//Some IO management code used by the new V2 $functions.  We need to ensure correct
//object deletion and resource release even if the User Language programmer is sloppy.
//This class is also a nice simple auto-close wrapper and is generallly used throughout.
//*************************************************************************************

#if !defined BB_IOWRAPPERS
#define BB_IOWRAPPERS

#include "garbage.h"
#include "bbstdio.h"
#if defined(_BBHOST)
#include "sockets.h"
#endif
#include "windows.h"

namespace dpt { namespace util {

//************************************************
class BBStdioFile : public Destroyable {
	std::string fname;
	int handle;

	bool opened_here;
	bool delete_file_at_close;
	bool had_read_eof;

#ifdef DEBUG_V221_STDIO
	int* orig_handle;
	mutable bool corrupt_flagged;
	void CorruptHandleCheck(const char* s) const {if (handle != *orig_handle) 
		HandleCheckError("BBStdioFile", s, handle, *orig_handle, corrupt_flagged, fname);}
#endif

public:
	BBStdioFile(const char* fname, int, int sh = _SH_DENYWR, int perm = _S_IREAD | _S_IWRITE);
	BBStdioFile();
	~BBStdioFile();

	//For when we have a file already open.
	BBStdioFile(int handle, const char* fname); 

	void Open(const char*, int, int sh = _SH_DENYWR, int perm = _S_IREAD | _S_IWRITE);
	void Close();
	bool IsOpen() {return (handle != -1);}

	//Allow close and reopen without giving a new name
	void Open(int disp) {Open(NULL, disp);}

	int FLength();
	time_t FileUpDatedTime(bool commit = true);
	int Tell();
	const std::string& Name() {return fname;}
	const int Handle() {return handle;}

	int Seek(int, int = SEEK_SET);	
	int Read(void*, unsigned int);	
	bool ReadLine(std::string&);
	bool HadEOF() {return had_read_eof;}

	void Write(const void*, unsigned int);	
	void WriteLine(const char*, int);	
	void WriteLine(const std::string&);	
	void Commit();
	void Chsize(int);

	_int64 TellI64();
	_int64 SeekI64(_int64, int = SEEK_SET);

	void DeleteTheFile() {if (!IsOpen()) StdioDeleteFile(handle, fname.c_str());}
	void SetDeleteAtCloseFlag(bool b = true) {delete_file_at_close = b;}
};

//************************************************
#if defined(_BBHOST)

class DestroyableSocket : public Socket, public Destroyable {
public:
	DestroyableSocket(Destroyer* d = NULL) : Destroyable(NULL, d) {}
	~DestroyableSocket() {}
};

#endif




//**************************************************************************************
//V3.0. Largely with BBStdioFile but:
//- Highly tuned for sequential read in small chunks
//- Only supports 64 bit tell/seek/flength functions
//**************************************************************************************

//This needs to be a multiple of sector size
#define LBSF_BUFFSZ (2 << 16)
//#define LBSF_BUFFSZ 512

class LocalBufferedSequentialInputFile {
	std::string fname;
	HANDLE handle;

	bool opened_here;
	bool delete_file_at_close;
	bool reached_file_eof;
	
	_int64 local_buffer_filebasepos;
	char local_buffer[LBSF_BUFFSZ];
	const char* local_buffer_readptr;
	const char* local_buffer_endptr;
	bool had_buffer_eof;

	void ClearLocalBuffer();
	void PopulateLocalBuffer();

public:
	LocalBufferedSequentialInputFile(const char* fname, int = util::STDIO_RDONLY);
	LocalBufferedSequentialInputFile();
	~LocalBufferedSequentialInputFile();

	//For when we have a file already open.
//	LocalBufferedSequentialInputFile(HANDLE handle, const char* fname); 

	void Open(const char*, int);
	void Close();
	bool IsOpen() {return (handle != INVALID_HANDLE_VALUE);}

	//Allow close and reopen without giving a new name
//	void Open(int disp) {Open(NULL, disp);}

	const std::string& Name() {return fname;}

//	time_t FileUpDatedTime(bool commit = true);
//	const HANDLE Handle() {return handle;}

	int Read(void*, int);	
	bool ReadLine(std::string&, bool throw_if_very_long = false, char pretermchar = 0);
//	bool HadEOF() {return had_buffer_eof;}

	_int64 FLengthI64();
	_int64 TellI64();
	_int64 SeekI64(_int64);

	void DeleteTheFile() {if (!IsOpen()) StdioDeleteFile(fname.c_str());}
	void SetDeleteAtCloseFlag(bool b = true) {delete_file_at_close = b;}
};

}}	//close namespace

#endif

