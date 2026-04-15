
#include "stdafx.h"

#include "lioshare.h"

#include "share.h"
#include <io.h>				//all the (non_ANSI) _xxxx file access functions

#include "dataconv.h"
#include "except.h"
#include "msg_util.h"
#include "bbstdio.h"
#include "assert.h"

namespace dpt { namespace util {

//*****************************************************************************************
SharableLineIO::SharableLineIO(const std::string& fname, int disp, int sh, int perm) 
: commit_by_line(false), sharemode(sh)
#ifdef _DEBUG_LOCKS
	, read_write_lock(std::string("SHLIO: ").append(fname))
#endif
{
	//MOD would not make sense since we always have to give a position
	if (disp == STDIO_MOD || disp == STDIO_CMOD)
		throw Exception(UTIL_LINEIO_ERROR, 
			"MOD disposition is not supported for SharableLineIO");

	//This is a Line IO class so it should be a read-write file by definition, but 
	//read-only ones are allowed, and any write attempt will fail later.
	if (disp != STDIO_RDONLY)
		disp |= _O_RDWR;

	handle = StdioSopen(fname.c_str(), disp, sh, perm);

#ifdef DEBUG_V221_STDIO
	orig_handle = new int(handle);
	corrupt_flagged = false;
#endif

	//Distill these from the relative path given
	char buff[_MAX_PATH];
	char* b = _fullpath(buff, fname.c_str(), _MAX_PATH);
	assert(b);
	filename_full = buff;
	char* slash = strrchr(buff, '\\');
	filename_short = slash+1;
}

//*****************************************************************************************
SharableLineIO::~SharableLineIO()
{
#ifdef DEBUG_V221_STDIO
	if (handle != -1) {
		CorruptHandleCheck("Close");
		if (!corrupt_flagged)
			_close(handle);
		else if (orig_handle)
			_close(*orig_handle);
	}

	if (orig_handle)
		delete orig_handle;

#else
	//Might be closed by delete earlier
	if (handle != -1)
		_close(handle);
#endif
}

//*****************************************************************************************
//These functions are called by most of the others
//*****************************************************************************************
void SharableLineIO::SetPosition(_int64 byte_pos) 
{
	//Only bother if another user has moved the file pointer.
	_int64 tell = StdioTellI64(handle, filename_full.c_str());
	if (byte_pos == tell) 
		return;

	//Positioning past the end of the file is an error with this class.
	_int64 len = GetFileSize();
	if (byte_pos > len)
		throw Exception(UTIL_LINEIO_ERROR, std::string
			("Error in SharableLineIO - attempt to read/write at position past end of file. ")
			.append("Position: ")
			.append(util::Int64ToString(byte_pos))
			.append(", file length: ")
			.append(util::Int64ToString(len)));

	//Do the reposition
	StdioLseekI64(handle, filename_full.c_str(), byte_pos, SEEK_SET);
}

//*****************************************************************************************
void SharableLineIO::VerifyNotDeleted() 
{
	if (handle == -1)
		throw Exception(UTIL_LINEIO_ERROR, 
		"OS file has been deleted - SharableLineIO object is now useless");
}

//*****************************************************************************************
_int64 SharableLineIO::Tell()
{
	return StdioTellI64(handle, filename_full.c_str());
}

//*****************************************************************************************
int SharableLineIO::ReadLine(_int64 byte_pos, char* dest, int* loglen) 
{
//* * * 

//NB1. I vaguely remember this code has a bug in it ????????

//NB2. Ideally this function would be merged with StdioReadline if possible, as they're
//virtually identical, although there is a slight difference in the meaning of the
//return value.

//* * * 

	LockingSentry s(&read_write_lock);
	VerifyNotDeleted();

#ifdef DEBUG_V221_STDIO
	CorruptHandleCheck("ReadLine");
#endif

	//Reposition in case another user has moved the file pointer
	SetPosition(byte_pos);

	int discards = 0;
	int logical_line_length = 0;
	char prev_char = 0;	
	simulated_eol = false;
	
	//Block up actual calls to the IO system into chunks as a nod toward performance
	int CHUNKSIZE = 100;
	char buffer[100];
	int buffer_offset = 0;
	int chunkread = 0;

	//Got to read byte-by-byte to look for the new line because of the problems mentioned
	//in the header with accuracy of tell().
	for (;;) {

		//Read a chunk every so often
		if (buffer_offset % CHUNKSIZE == 0) {

			errno = 0;
			chunkread = _read(handle, buffer, CHUNKSIZE);
			buffer_offset = 0;

			if (chunkread == -1) {
				//read error - set string to null and throw
				dest[0] = 0;
				throw Exception(UTIL_LINEIO_ERROR, MakeStdioFileErrorString
					("SharableLineIO::ReadLine()", "_read()", 
					filename_full, handle, 1, -1, errno));
			}
			else if (chunkread < CHUNKSIZE) {
				//Was the last chunk in the file - force the scan to find a newline.  EOF
				//in this class is indicated by a zero return code, not LINEIPEOF, because
				//of the different meanings.
				buffer[chunkread] = '\n';
			}
		}

		//Scan the chunk for a newline
		char c = buffer[buffer_offset];
		if (c != '\n')
			dest[logical_line_length] = c;

		else {

			//See header for BINARY mode comments.  Discard return only before newline.
			if (prev_char == '\r') {
				logical_line_length--;
				discards++;
			}

			//Replace the newline with end-of-string
			dest[logical_line_length] = 0;
			break;
		}

		prev_char = c;
		logical_line_length++;
		buffer_offset++;

		if (logical_line_length > 32700) {
			throw Exception(UTIL_LINEIO_ERROR,
				std::string("Physical input line too long in file: ")
				.append(filename_full)
				.append(".  (Calling function SharableLineIO::ReadLine())."));
		}
	}

	//Note that we return the number of bytes in the file actually used.  This 
	//is essential for the caller to know now, rather than by making another call to Tell()
	//since that would not be atomic.  Zero returned here effectively means EOF.  
	int file_bytes_used = logical_line_length + discards;

	//Can't hurt to tell the user this if they want it
	if (loglen)
		*loglen = logical_line_length;

	//Was there actually a newline (only important when dealing with the last line)
	if (buffer_offset != chunkread)
		file_bytes_used++;
	else
		simulated_eol = true;	

	return file_bytes_used;
}

//*****************************************************************************************
int SharableLineIO::WriteLine(_int64 byte_pos, const char* source, unsigned int len) 
{
	LockingSentry s(&read_write_lock);
	VerifyNotDeleted();

#ifdef DEBUG_V221_STDIO
	CorruptHandleCheck("WriteLine");
#endif

	//Reposition if another user has moved the file pointer
	SetPosition(byte_pos);

	//The caller may pass us the string length if they know it (saves a strlen call
	//and also allows the string to contain hex zeroes).
	if (!len) 
		len = strlen(source);

	//Do the file write
	errno = 0;
	if (_write(handle, source, len) == -1) {
		throw Exception(UTIL_LINEIO_ERROR,	MakeStdioFileErrorString
			("SharableLineIO::WriteLine()", "_write()", 
			filename_full, handle, len, -1, errno));
	}

	//Add CRLF (using both to increase compatibility with e.g. notepad)
	errno = 0;
	if (_write(handle, "\r\n", 2) == -1) {
		throw Exception(UTIL_LINEIO_ERROR,	MakeStdioFileErrorString
			("SharableLineIO::WriteLine()", "_write()", 
			filename_full, handle, len, -1, errno));
	}

	//Force a physical commit if requested (usually no need)
	if (commit_by_line)
		StdioCommit(handle, filename_full.c_str());

	//See comment in ReadLine() above for why this is necessary.
	return len+2;
}

//*****************************************************************************************
unsigned int SharableLineIO::WriteNoCRLF(_int64 byte_pos, const char* source, unsigned int len) 
{
	LockingSentry s(&read_write_lock);
	VerifyNotDeleted();

#ifdef DEBUG_V221_STDIO
	CorruptHandleCheck("WriteNoCRLF");
#endif

	//Reposition if another user has moved the file pointer
	SetPosition(byte_pos);

	//Do the file write
	errno = 0;
	if (_write(handle, source, len) == -1) {
		throw Exception(UTIL_LINEIO_ERROR,	MakeStdioFileErrorString
			("SharableLineIO::WriteNoCRLF()", "_write()", 
			filename_full, handle, len, -1, errno));
	}

	//Force a physical commit if requested (usually no need)
	if (commit_by_line)
		StdioCommit(handle, filename_full.c_str());

	//Helps simplify calling code
	return len;
}

//*****************************************************************************************
unsigned int SharableLineIO::ReadNoCRLF(_int64 byte_pos, char* dest, unsigned int len) 
{
	LockingSentry s(&read_write_lock);
	VerifyNotDeleted();

#ifdef DEBUG_V221_STDIO
	CorruptHandleCheck("ReadNoCRLF");
#endif

	//Reposition if another user has moved the file pointer
	SetPosition(byte_pos);

	errno = 0;
//	unsigned int bytes_read = _read(handle, dest, len); //V2.24
	int bytes_read = _read(handle, dest, len);
	if (bytes_read == -1)
		throw Exception(UTIL_LINEIO_ERROR, MakeStdioFileErrorString
			("SharableLineIO::ReadNoCRLF()", "_read()", 
			filename_full, handle, 1, -1, errno));

	return bytes_read;
}

//*****************************************************************************************
_int64 SharableLineIO::GoToEnd() 
{
	LockingSentry s(&read_write_lock);
	VerifyNotDeleted();

#ifdef DEBUG_V221_STDIO
	CorruptHandleCheck("GotoEnd");
#endif

	_int64 end_pos = StdioFLengthI64(handle, filename_full.c_str());
	SetPosition(end_pos);

	return end_pos;
}

//*****************************************************************************************
void SharableLineIO::Truncate(_int64 byte_pos) 
{
	LockingSentry s(&read_write_lock);
	VerifyNotDeleted();

#ifdef DEBUG_V221_STDIO
	CorruptHandleCheck("Truncate");
#endif

	StdioChsizeI64(handle, filename_full.c_str(), byte_pos);
	SetPosition(byte_pos);
}

//*****************************************************************************************
void SharableLineIO::Delete() 
{
	LockingSentry s(&read_write_lock);
	VerifyNotDeleted();

#ifdef DEBUG_V221_STDIO
	CorruptHandleCheck("Delete");
#endif

	StdioDeleteFile(handle, filename_full.c_str());
}

//*****************************************************************************************
void SharableLineIO::Rename(const char* newname) 
{
	LockingSentry s(&read_write_lock);
	VerifyNotDeleted();

#ifdef DEBUG_V221_STDIO
	CorruptHandleCheck("Rename");
#endif

	//Close before renaming
	_close(handle);
	int old_handle = handle;
	handle = -1;

#ifdef DEBUG_V221_STDIO
	if (orig_handle)
		delete orig_handle;
#endif

	//Do the rename
	errno = 0;
	if (rename(filename_full.c_str(), newname) != 0) {
		throw Exception(UTIL_LINEIO_ERROR,	MakeStdioFileDirectoryErrorString
			("SharableLineIO::Rename()", "rename()", 
			filename_full, old_handle, errno));
	}

	//Re-open the newly-named file
	errno = 0;
	handle = _sopen(newname, util::STDIO_OLD, sharemode);
	if (handle == -1) {
		throw Exception(UTIL_LINEIO_ERROR, MakeStdioFileDirectoryErrorString
			("SharableLineIO::Rename()", "_sopen()", newname, handle, errno));
	}

#ifdef DEBUG_V221_STDIO
	NoteStdioFileName(handle, newname);

	orig_handle = new int(handle);
	corrupt_flagged = false;
#endif

	//Change these to match the rename
	char buff[_MAX_PATH];
	char* b = _fullpath(buff, newname, _MAX_PATH);
	assert(b);
	filename_full = buff;
	char* slash = strrchr(buff, '\\');
	filename_short = slash+1;
}

//*****************************************************************************************
//OS file info
//*****************************************************************************************
_int64 SharableLineIO::GetFileSize()
{
	return StdioFLengthI64(handle, filename_full.c_str());
}

//*****************************************************************************************
int SharableLineIO::SaveFromBufferWithEOLs(std::vector<std::string>& buff)
{
	LockingSentry s(&read_write_lock);
	VerifyNotDeleted();

#ifdef DEBUG_V221_STDIO
	CorruptHandleCheck("SaveFromBuffer");
#endif

	return OverwriteStdioFileFromBufferWithEOLs(handle, filename_full.c_str(), buff);
}

//*****************************************************************************************
void SharableLineIO::CopyFrom(SharableLineIO* source, bool olddate, bool truncate)
{
	LockingSentry s(&read_write_lock);
	VerifyNotDeleted();

#ifdef DEBUG_V221_STDIO
	CorruptHandleCheck("CopyFrom B");
#endif

	LockingSentry s2(&source->read_write_lock);
	source->VerifyNotDeleted();

#ifdef DEBUG_V221_STDIO
	source->CorruptHandleCheck("CopyFrom B");
#endif

	//V3.0
//	StdioCopyFile(handle, filename_full.c_str(), 
//				source->handle, source->filename_full.c_str(), truncate);
	StdioCopyFile(handle, filename_full.c_str(), 
				source->handle, source->filename_full.c_str(), 0, -1, truncate);

	//The above will have left the file with an update time of now
	if (olddate) {
		time_t oldtime = GetStdioFileUpdatedTime(source->handle, source->filename_full.c_str());
		SetStdioFileUpdatedTime(handle, filename_full.c_str(), oldtime);
	}
}

}} //close namespace
