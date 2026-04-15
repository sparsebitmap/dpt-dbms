
#include "stdafx.h"

#include "seqfile.h"

#include "windows.h"

//Utils
#include "lioshare.h"
#include "dataconv.h"
#include "charconv.h"
#include "winutil.h"
//API Tiers
#include "seqserv.h"
#include "dbserv.h"
#include "core.h"
//Diagnostics
#include "except.h"
#include "msg_seq.h"
#include "msg_file.h"

namespace dpt {

//*****************************************************************************************
SequentialFile::SequentialFile(const std::string& parm_dd, std::string& parm_dsn, 
	short parm_lrecl, char p, unsigned int m, FileDisp parm_disp, bool& cflag, 
	const std::string& alias, bool t, bool nc)
: AllocatedFile(parm_dd, parm_dsn, FILETYPE_SEQ, alias), 
  thefile(NULL),
  maxsize(m),
  pad(p),
  open_append(false),
  lrecl(parm_lrecl),
  tempdsn(t),
  nocrlf(nc)
{
	//Give a bit more range - nobody would want to set it lower than 1K surely
	maxsize <<= 10;

	//I think this is probably a worthwhile check
	size_t dotpos = parm_dsn.rfind('.');
	std::string extension;
	if (dotpos != std::string::npos && dotpos != (parm_dsn.length() - 1))
		extension = parm_dsn.substr(dotpos+1);

	util::ToUpper(extension);
	if (extension == "DPT")
		throw Exception(SEQ_BAD_DSN, "Sequential file extensions can not be '.dpt'");

	//V3.0
	if (nocrlf && lrecl == -1)
		throw Exception(SEQ_BAD_OPEN_MODE, 
			"NOCRLF sequential file option requires LRECL to be given");

	//See if it exists
	WIN32_FIND_DATA dir_data;
	HANDLE handle = FindFirstFile(parm_dsn.c_str(), &dir_data);
	bool exists = !(handle == INVALID_HANDLE_VALUE);
	bool isdir = (dir_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? true : false;
	FindClose(handle);

	//The "disp" parameter passed in is one of old, new etc., as defined in file.h.  Here
	//we convert this into one of the open mode flags defined in lineio.h.  Note that if 
	//the caller wanted "no append", this actually means overwrite each time the file is
	//opened to user language, not to Windows, so we don't use the low-level MOD 
	//functionality here.  That is handled during Open() for write.
	FileDisp disp = parm_disp;
	if (disp == FILEDISP_MOD) {
		disp = FILEDISP_OLD;
		open_append = true;
	}

	int stdio_mode;

	if (exists && isdir) {
		throw Exception(SEQ_IS_DIR, 
			std::string("Path is a directory: ").append(parm_dsn));
	}
	else if (disp == FILEDISP_OLD || disp == FILEDISP_READONLY) {
		if (!exists)
			throw Exception(SEQ_NONEXISTENT, 
				std::string("File does not exist: ").append(parm_dsn));
		if (disp == FILEDISP_OLD)
			stdio_mode = util::STDIO_OLD;
		else
			stdio_mode = util::STDIO_RDONLY;
		cflag = false;
	}
	else if (disp == FILEDISP_NEW) {
		if (exists)
			throw Exception(SEQ_ALREADY_EXISTS, 
				std::string("File already exists: ").append(parm_dsn));
		stdio_mode = util::STDIO_NEW;
		cflag = true;
	}
	else { //COND
		if (exists) {
			stdio_mode = util::STDIO_OLD;
			cflag = false;
		}
		else {
			stdio_mode = util::STDIO_NEW;
			cflag = true;
		}
	}

	thefile = new util::SharableLineIO(parm_dsn.c_str(), stdio_mode);
}

//***********************************
SequentialFile::~SequentialFile() 
{
	if (thefile)
		delete thefile;
}

//***********************************
FileHandle SequentialFile::Construct
(const std::string& parm_dd, std::string& parm_dsn, short lrecl, char pad, 
 unsigned int max, FileDisp disp, bool& cflag, const std::string& alias, bool tempdsn, bool nocrlf)
{
	//Lock across the *whole* constructor, in case derived part throws
	LockingSentry ls(&allocation_lock);

	SequentialFile* sf = new SequentialFile
		(parm_dd, parm_dsn, lrecl, pad, max, disp, cflag, alias, tempdsn, nocrlf);

	//As with procs the enqueue type at this stage is irrelevant.
	return FileHandle(sf, BOOL_SHR);
}

//***********************************
bool SequentialFile::Destroy(FileHandle& h)
{
	LockingSentry ls(&allocation_lock);
	SequentialFile* sf = static_cast<SequentialFile*>(h.GetFile());
	bool t = sf->tempdsn;
	delete sf;
	return t;
}

//***********************************
_int64 SequentialFile::Open(bool lk, bool dummy)
{
	TryForOpenLock(lk, false, dummy);

	try {
		//Opening for write
		if (lk == BOOL_EXCL) {

			//Append to the file
			if (open_append)
				return thefile->GoToEnd();

			//Empty the file and start again
			thefile->Truncate(0);
			return 0;
		}
	}
	catch (...) {
		ReleaseOpenLock();
		throw;
	}

	//Opening for read - append mode is irrelevant - we always read from the start
	return 0;
}

//***********************************
void SequentialFile::MaxSizeCheck()
{
	if (maxsize == 0)
		return;
	
	//Add current memory and file part sizes
	unsigned _int64 cursize = thefile->GetFileSize();

	if (cursize > maxsize)
		throw Exception(SEQ_FILE_TOO_BIG,
			std::string("File has exceeded the specified MAXSIZE: ")
			.append(thefile->GetFileNameShort()));
}








//*****************************************************************************************
SequentialFileView::SequentialFileView
(SequentialFileServices* ss, SequentialFile* sf, bool lk) 
: 
#ifdef _BBHOST
 Imagable(sf->BaseIO()->GetFileNameShort(), (lk==BOOL_SHR), (lk==BOOL_EXCL)),
#else
 LineInput(sf->BaseIO()->GetFileNameShort()), LineOutput(sf->BaseIO()->GetFileNameShort()),
 reading(lk==BOOL_SHR),
#endif
 seqfile(sf), seqserv(ss)
{
	pos = seqfile->Open(lk); 
}

//***********************************
void SequentialFileView::CloseAndDestroy() 
{
	seqserv->CloseSeqFile(this);
}

//***********************************
int SequentialFileView::ReadLine_D(char* c) 
{
	WTSentry s(seqserv->DBAPI()->Core(), 22);
	int lrecl = seqfile->Lrecl();

	//V3.0.  NOCRLF-style processing.
	if (seqfile->NoCRLF()) {

		//No issues with PAD here, we know we got the right amount
		if (ReadNoCRLF(c, lrecl))
			return util::LINEIPEOF;

		seqserv->DBAPI()->Core()->IncStatSEQI();
		return lrecl;
	}

	//Read a line, ensuring that the file is positioned where we last left it, and updating
	//our local pointer in one atomic operation.
	int logical_line_length;
	int file_bytes_read = seqfile->BaseIO()->ReadLine(pos, c, &logical_line_length);

	simulated_eol = seqfile->BaseIO()->SimulatedEOL();

	//Zero bytes returned from that class means EOF
	if (file_bytes_read == 0)
		return util::LINEIPEOF;

	//Simulate mainframe-style "fixed length" records if required
	if (lrecl != -1) {
		if (lrecl > logical_line_length)
			memset(c + logical_line_length, seqfile->Pad(), lrecl - logical_line_length);
		else
			c[lrecl] = 0;

		logical_line_length = lrecl;
	}

	pos += file_bytes_read;
	seqserv->DBAPI()->Core()->IncStatSEQI();

	return logical_line_length;
}

//***********************************
void SequentialFileView::NewLine_D(std::string& line) 
{
	WTSentry s(seqserv->DBAPI()->Core(), 22);
	int lrecl = seqfile->Lrecl();

	seqfile->MaxSizeCheck();

	//Truncate or pad as required for "fixed length" output
	if (lrecl != -1)
		line.resize(lrecl, seqfile->Pad());

	//V3.0.  NOCRLF option
	if (seqfile->NoCRLF())
		WriteNoCRLF(line.c_str(), lrecl);
	else
		pos += seqfile->BaseIO()->WriteLine(pos, line.c_str(), line.length());

	seqserv->DBAPI()->Core()->IncStatSEQO();
}

#ifdef _BBHOST
//***********************************
void SequentialFileView::ThrowIfEOF() 
{
	if (ReadAtEOF())
		throw Exception(SEQ_PAST_EOF, 
			std::string("Attempt to read past end of sequential file ")
			.append(seqfile->BaseIO()->GetFileNameShort()));
}

#else
//***********************************************************************************
//These 3 are almost itendical to the Imagable behaviour.
int SequentialFileView::LineInputPhysicalReadLine(char* c)
{
	//With variable-length records we could get into a mess!
	if (!reading)
		throw Exception(SEQ_BAD_OPEN_MODE, 
			"Sequential file is open for writing, not reading");

	return ReadLine_D(c);
}

//***********************************
void SequentialFileView::LineOutputPhysicalWrite(const char* c, int len) 
{
	if (reading)
		throw Exception(SEQ_BAD_OPEN_MODE, 
			"Sequential file is open for reading, not writing");

	write_buffer.append(c, len);
}

//***********************************
void SequentialFileView::LineOutputPhysicalNewLine() 
{
	if (reading)
		throw Exception(SEQ_BAD_OPEN_MODE, 
			"Sequential file is open for reading, not writing");

	std::string s = write_buffer;
	write_buffer = std::string();

	NewLine_D(s);
}


#endif

//*************************************
//COPY DATASET - see comments in the command handler and the user doc
void SequentialFileView::CopyFrom(const SequentialFileView* source)
{
	seqfile->BaseIO()->CopyFrom(source->seqfile->BaseIO(), false, !seqfile->OpenAppend());
}

//*************************************
//Used in fixed length deferred updates
bool SequentialFileView::ReadNoCRLF(char* dest, unsigned int len)
{
	//V2.28 July 2010.  Don't even bother going to the file for a zero length read.
	if (len == 0)
		return false;

	_int64 prepos = pos;
	pos += seqfile->BaseIO()->ReadNoCRLF(pos, dest, len);

	unsigned int reclen = pos - prepos;
	if (reclen == 0)
		return true;

	if (reclen != len)
		throw Exception(SEQ_PAST_EOF, std::string(
			"Error reading sequential file in bytewise mode: partial record at end, length ")
			.append(util::IntToString(reclen)));

	return false;
}

//*************************************
//Used in database loads so we can put extra buffering on there
unsigned int SequentialFileView::ReadNoCRLFAnyLength(char* dest, unsigned int len)
{
	_int64 prepos = pos;
	pos += seqfile->BaseIO()->ReadNoCRLF(pos, dest, len);
	return pos - prepos;
}

//*************************************
//Put this in for API code to use if desired
void SequentialFileView::WriteNoCRLF(const char* source, unsigned int len)
{
	pos += seqfile->BaseIO()->WriteNoCRLF(pos, source, len);
}


} //close namespace

