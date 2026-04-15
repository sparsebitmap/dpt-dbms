
#include "stdafx.h"

#include "iowrappers.h"

//Utils
#include "bbstdio.h"
#include "liostdio.h"
#include "winutil.h"
//Diagnostics
#include "except.h"
#include "msg_util.h"

namespace dpt { namespace util {

//*************************************
BBStdioFile::BBStdioFile(const char* fn, int disp, int sh, int perm)
: handle(-1), delete_file_at_close(false)
{
	Open(fn, disp, sh, perm);
}

//*************************************
BBStdioFile::BBStdioFile() 
: handle(-1), delete_file_at_close(false)
{
#ifdef DEBUG_V221_STDIO
	orig_handle = NULL;
#endif
}

//*************************************
BBStdioFile::BBStdioFile(int h, const char* n) 
: fname(n), handle(h), opened_here(false), delete_file_at_close(false), had_read_eof(false)
{
#ifdef DEBUG_V221_STDIO
	orig_handle = new int(handle); 
	corrupt_flagged = false;
#endif
}

//*************************************
BBStdioFile::~BBStdioFile() 
{
	if (opened_here) 
		Close(); 
	
#ifdef DEBUG_V221_STDIO
	if (orig_handle) 
		delete orig_handle;
#endif
}

//*************************************
void BBStdioFile::Open(const char* fn, int disp, int sh, int perm)
{
	Close();

	if (fn)
		fname = fn;

	handle = StdioSopen(fname.c_str(), disp, sh, perm);

#ifdef DEBUG_V221_STDIO
	orig_handle = new int(handle);
	corrupt_flagged = false;
#endif
	
	opened_here = true;
	had_read_eof = false;
}

//*************************************
void BBStdioFile::Close() 
{
	if (!IsOpen()) 
		return;

#ifdef DEBUG_V221_STDIO
	CorruptHandleCheck("Close");

	if (!corrupt_flagged)
		StdioClose(handle);
	else if (orig_handle)
		StdioClose(*orig_handle);

	handle = -1;

	if (orig_handle) {
		delete orig_handle;
		orig_handle = NULL;
	}
#else
	StdioClose(handle);
	handle = -1;
#endif

	//V2.22 Oct 09.
	if (delete_file_at_close)
		DeleteTheFile();
}

//*************************************
int BBStdioFile::Tell() {
#ifdef DEBUG_V221_STDIO
	CorruptHandleCheck("Tell");
#endif
	return StdioTell(handle, fname.c_str());}

//*************************************
int BBStdioFile::FLength() {
#ifdef DEBUG_V221_STDIO
	CorruptHandleCheck("FLength");
#endif
	return StdioFLength(handle, fname.c_str());}

//*************************************
time_t BBStdioFile::FileUpDatedTime(bool commit) {
#ifdef DEBUG_V221_STDIO
	CorruptHandleCheck("FileUpDatedTime");
#endif
	return GetStdioFileUpdatedTime(handle, fname.c_str(), commit);}

//*************************************
int BBStdioFile::Seek(int pos, int mode) {
#ifdef DEBUG_V221_STDIO
	CorruptHandleCheck("Seek");
#endif
	had_read_eof = false;
	return StdioLseek(handle, fname.c_str(), pos, mode);}

//*************************************
int BBStdioFile::Read(void* buff, unsigned int numbytes) {
#ifdef DEBUG_V221_STDIO
	CorruptHandleCheck("Read");
#endif
	int readlen = StdioRead(handle, fname.c_str(), buff, numbytes);
	had_read_eof = (readlen < (int)numbytes);
	return readlen;
}

//*************************************
bool BBStdioFile::ReadLine(std::string& line) {
#ifdef DEBUG_V221_STDIO
	CorruptHandleCheck("ReadLine");
#endif
	if (had_read_eof)
		return true;
	StdIOLineInput li(handle);
	had_read_eof = li.ReadLine(line);
	return had_read_eof;
}

//*************************************
//64 bit
_int64 BBStdioFile::SeekI64(_int64 pos, int mode) {
#ifdef DEBUG_V221_STDIO
	CorruptHandleCheck("SeekI64");
#endif
	had_read_eof = false;
	return StdioLseekI64(handle, fname.c_str(), pos, mode);}

//*************************************
_int64 BBStdioFile::TellI64() {
#ifdef DEBUG_V221_STDIO
	CorruptHandleCheck("TellI64");
#endif
	return StdioTellI64(handle, fname.c_str());}


//*************************************
void BBStdioFile::Write(const void* buff, unsigned int numbytes) {
#ifdef DEBUG_V221_STDIO
	CorruptHandleCheck("Write");
#endif
	StdioWrite(handle, fname.c_str(), buff, numbytes);}

//*************************************
void BBStdioFile::WriteLine(const char* source, int len) {
#ifdef DEBUG_V221_STDIO
	CorruptHandleCheck("WriteLine A");
#endif
	StdIOLineOutput lo(handle);
	lo.WriteLine(source, len);}

//*************************************
void BBStdioFile::WriteLine(const std::string& s) {
#ifdef DEBUG_V221_STDIO
	CorruptHandleCheck("WriteLine B");
#endif
	StdIOLineOutput lo(handle);
	lo.WriteLine(s);}

//*************************************
void BBStdioFile::Commit() {
#ifdef DEBUG_V221_STDIO
	CorruptHandleCheck("Commit");
#endif
	StdioCommit(handle, fname.c_str());}

//*************************************
void BBStdioFile::Chsize(int newlen) {
#ifdef DEBUG_V221_STDIO
	CorruptHandleCheck("Chsize");
#endif
	had_read_eof = false;
	StdioChsize(handle, fname.c_str(), newlen);}








//***********************************************************************************
//V3.0. For fast load
//***********************************************************************************
LocalBufferedSequentialInputFile::LocalBufferedSequentialInputFile
(const char* fn, int disp)
: handle(INVALID_HANDLE_VALUE), delete_file_at_close(false)
{
	Open(fn, disp);
}

//*************************************
LocalBufferedSequentialInputFile::LocalBufferedSequentialInputFile() 
: handle(INVALID_HANDLE_VALUE), delete_file_at_close(false)
{
}

//*************************************
//LocalBufferedSequentialInputFile::LocalBufferedSequentialInputFile
//(HANDLE h, const char* n)
//: fname(n), handle(h), opened_here(false), 
//delete_file_at_close(false), had_read_eof(false), got_local_buffer(false)
//{
//}

//*************************************
LocalBufferedSequentialInputFile::~LocalBufferedSequentialInputFile() 
{
	if (opened_here) 
		Close(); 
}

//*************************************
void LocalBufferedSequentialInputFile::Open(const char* fn, int idisp)
{
	Close();

	if (fn)
		fname = fn;

	if (idisp != util::STDIO_RDONLY) {
		throw Exception(UTIL_STDIO_ERROR, 
			"Class LocalBufferedSequentialInputFile only supports read only access");
	}

	handle = CreateFile(
		fname.c_str(), 
		GENERIC_READ,
		0, 
		NULL, 
		OPEN_EXISTING, 
		FILE_FLAG_NO_BUFFERING,
		NULL
	);

	if (handle == INVALID_HANDLE_VALUE) {
		throw Exception(UTIL_STDIO_ERROR,
			std::string("Error opening file: ")
			.append(win::GetLastErrorMessage())
			.append(" (fname=")
			.append(fname)
			.append(")")
			);
	}


	opened_here = true;

	ClearLocalBuffer();
}

//*************************************
void LocalBufferedSequentialInputFile::Close() 
{
	if (!IsOpen()) 
		return;

	CloseHandle(handle);
	handle = INVALID_HANDLE_VALUE;

	//V2.22 Oct 09.
	if (delete_file_at_close)
		DeleteTheFile();
}

//*************************************
void LocalBufferedSequentialInputFile::ClearLocalBuffer()
{
	local_buffer_filebasepos = 0;
	local_buffer_readptr = local_buffer;
	local_buffer_endptr = local_buffer;

	reached_file_eof = false;
	had_buffer_eof = false;
}

//*************************************
void LocalBufferedSequentialInputFile::PopulateLocalBuffer()
{
	local_buffer_filebasepos = TellI64();

	unsigned long local_buffer_numbytes;
	BOOL rc = ReadFile(handle, local_buffer, LBSF_BUFFSZ, &local_buffer_numbytes, NULL);

	if (rc == 0) {
		throw Exception(UTIL_STDIO_ERROR,
			std::string("Read error: ")
			.append(win::GetLastErrorMessage())
			.append(" (fname=")
			.append(fname)
			.append(")")
			);
	}
	
	if (local_buffer_numbytes == 0)
		reached_file_eof = true;

	local_buffer_readptr = local_buffer;
	local_buffer_endptr = local_buffer + local_buffer_numbytes;
}

//*************************************
int LocalBufferedSequentialInputFile::Read(void* dest, int reqdbytes) 
{
	int buffer_bytes_available = local_buffer_endptr - local_buffer_readptr;

	//Common case - the local buffer has enough
	if (reqdbytes <= buffer_bytes_available) {

		//Common case with this class is small items so this is worthwhile.  
		//It adds 8 instructions (worst case <20%) to cases where memcpy is needed, and 
		//saves 80-90+% in the common cases. 
		if (reqdbytes == 1)
			*((char*)dest) = *local_buffer_readptr;
		else if (reqdbytes == 2)
			*((short*)dest) = *((short*)local_buffer_readptr);
		else if (reqdbytes == 4)
			*((int*)dest) = *((int*)local_buffer_readptr);
		else if (reqdbytes == 8)
			*((_int64*)dest) = *((_int64*)local_buffer_readptr);
		else
			memcpy(dest, local_buffer_readptr, reqdbytes);

		local_buffer_readptr += reqdbytes;
		return reqdbytes;
	}

	//Take what there is
	if (buffer_bytes_available) {
		memcpy(dest, local_buffer_readptr, buffer_bytes_available);
		local_buffer_readptr = local_buffer_endptr;
	}
	int gotbytes = buffer_bytes_available;

	//And get more data from the file if possible
	if (reached_file_eof) {
		had_buffer_eof = true;
		return gotbytes;
	}

	PopulateLocalBuffer();
	return gotbytes + Read((char*)dest + gotbytes, reqdbytes - gotbytes);
}

//*************************************
bool LocalBufferedSequentialInputFile::ReadLine
(std::string& line, bool throw_if_very_long, char pretermchar) 
{
	static const int OPCHUNK = 32768;
	char linebuff[OPCHUNK];
	char* eolptr = linebuff;
	char* ovfl = linebuff + OPCHUNK;

	line = std::string();

	//Process chunks till EOL
	for (;;) {

		//Look through remainder of this chunk
		while (local_buffer_readptr < local_buffer_endptr) {

			if (*local_buffer_readptr == '\n') {

				//Remove optional preceding CR if present
				if (eolptr > linebuff && *(eolptr-1) == '\r')
					eolptr--;

				line.append(linebuff, eolptr - linebuff);
				local_buffer_readptr++;

				//False rc means "correctly" terminated with CRLF
				return false;
			}

			else {
				*eolptr = *local_buffer_readptr;	
				eolptr++;

				//This is for fast load PAI mode alternate format.  Take back the term char too
				//so as to distinguish the two empty string cases.
				if (pretermchar && *local_buffer_readptr == pretermchar) {
					line.append(linebuff, eolptr - linebuff); 
					local_buffer_readptr++;
					return false;
				}

				//Filled the string-building buffer
				if (eolptr == ovfl) {

					//Unlike most other DPT line IO functions this one can take >32K as it
					//may have to handle BLOBs in PAI mode.  The 32K limit's just a sanity check 
					//elsewhere anyway, since a "text line" would not normally be that long.
					if (throw_if_very_long) {
						throw Exception(UTIL_LINEIO_ERROR, "LocalBufferedSequentialInputFile::ReadLine() "
							"encountered a line > 32K in length");
					}

					//Otherwise build another chunk of output string
					line.append(linebuff, OPCHUNK);
					eolptr = linebuff;
				}

				local_buffer_readptr++;
			}
		}

		//True rc means a string terminated at EOF
		if (reached_file_eof) {
			line.append(linebuff, eolptr - linebuff);
			had_buffer_eof = true;
			return true;
		}

		//Get next chunk
		PopulateLocalBuffer();
	}
}

//*************************************
_int64 LocalBufferedSequentialInputFile::FLengthI64() 
{
	//The neater GetFileSizeEx() requires a DLL load and is W2K+
	DWORD filesizehigh;
	DWORD filesizelow = GetFileSize(handle, &filesizehigh);

	ULARGE_INTEGER result;
	result.HighPart = filesizehigh;
	result.LowPart = filesizelow;

	return result.QuadPart;
}

//*************************************
//time_t LocalBufferedSequentialInputFile::FileUpDatedTime(bool commit) 
//{
//}

//*************************************
_int64 LocalBufferedSequentialInputFile::SeekI64(_int64 inewpos) 
{
	ClearLocalBuffer();

	//Because we are using unbuffered disk IO, we can't do this the regular way
	//using just SetFilePointer otherwise it will mess up the sector-alignment
	//requirement when we come to do later reads from there.
	_int64 chunkoffset = inewpos % LBSF_BUFFSZ;
	_int64 chunkbase = inewpos - chunkoffset;

	//Set file pointer to a sector-aligned point before the desired pos
	ULARGE_INTEGER uli;
	uli.QuadPart = chunkbase;

	SetFilePointer(handle, uli.LowPart, (long*)(&(uli.HighPart)), SEEK_SET);

	//Then do a dummy read
	PopulateLocalBuffer();
	local_buffer_readptr += chunkoffset;

	//Let's not get overcomplicated - assume the above was somewhere inside the file
	return local_buffer_filebasepos + chunkoffset;
}

//*************************************
_int64 LocalBufferedSequentialInputFile::TellI64() 
{
	return local_buffer_filebasepos + (local_buffer_readptr - local_buffer);
}



}} //close namespace


