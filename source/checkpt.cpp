
#include "stdafx.h"

#include "checkpt.h"

#include "share.h"
#include <io.h>				//all the (non_ANSI) _xxxx file access functions
#include <sys\stat.h>

//Utils
#include "bbstdio.h"
#include "rawpage.h"
#include "winutil.h"
#include "dataconv.h"
#include "paged_io.h"
//API tiers
#include "dbstatus.h"
#include "dbfile.h"
#include "dbserv.h"
#include "buffmgmt.h"
#include "pagebase.h"
#include "core.h"
#include "sysfile.h"
#include "msgroute.h"
//Diagnostics
#include "msg_db.h"
#include "except.h"
#include "assert.h"

//For Dump
#ifdef _BBHOST
#include "iodev.h"
#else
#include "lineio.h"
#endif

namespace dpt {

CheckpointFile* CheckpointFile::the_one = NULL;

//**********************************************************************************
CheckpointFile::CheckpointFile() : num_pre_images(-1)
{
	fname = ("checkpoint.ckp");
	alloc_handle = SystemFile::Construct("+CHKP", fname, BOOL_EXCL);

	//Open the file if exists, or create it
	try {
		errno = 0;
		stdio_handle = _sopen(fname.c_str(), util::STDIO_COND | _O_RDWR, 
											_SH_DENYWR, _S_IREAD | _S_IWRITE);
		if (stdio_handle == -1)
			throw Exception(CHKP_FILE_OPEN_FAILED, util::MakeStdioFileDirectoryErrorString
				("CheckpointFile()", "_sopen()", fname.c_str(), stdio_handle, errno));

#ifdef DEBUG_V221_STDIO
		util::NoteStdioFileName(stdio_handle, fname);
#endif

	}
	catch (...) {
		SystemFile::Destroy(alloc_handle);
		throw;
	}

	the_one = this;
}

//**********************************************************************************
CheckpointFile::~CheckpointFile()
{
	SystemFile::Destroy(alloc_handle);

	//If this destructor's being called a checkpoint should have been taken at
	//system closedown and worked.  But just in case here check that's true.
	if (num_pre_images == 0)
		util::StdioDeleteFile(stdio_handle, fname.c_str());
	else
		_close(stdio_handle);
}


//**********************************************************************************
_int64 CheckpointFile::FLength()
{
	return util::StdioFLengthI64(stdio_handle, fname.c_str());
}

//**********************************************************************************
_int64 CheckpointFile::Tell()
{
	return util::StdioTellI64(stdio_handle, fname.c_str()); 
}

//**********************************************************************************
void CheckpointFile::Rewind()
{
	util::StdioLseek(stdio_handle, fname.c_str(), 0, SEEK_SET);
}

//**********************************************************************************
CheckpointFileRecord* CheckpointFile::ReadIn(void* preimage_buffer)
{
	return CheckpointFileRecord::ReadIn(stdio_handle, fname, preimage_buffer);
}

//**********************************************************************************
int CheckpointFile::WriteRecord_S(DatabaseServices* dbapi, CheckpointFileRecord* rec)
{
	LockingSentry ls(&cplock);
	WTSentry ws(dbapi->Core(), 16);

	//OK to use the shared buffer as under lock now
	int total_len = rec->FormatBuffer();

	char temp[10];
	memcpy(temp, rec->Buffer(), 9);
	temp[9] = 0;
//	TRACE("CCCCCCCCCCCCCCP buff before write: %s \n", temp);

	util::StdioWrite(stdio_handle, fname.c_str(), rec->Buffer(), total_len);

	return total_len;
}

//**********************************************************************************
int CheckpointFile::WriteAllocatedFileInfo
(DatabaseServices* dbapi, const char* dd, const char* dsn)
{
	CheckpointAllocatedFileRecord af(dd, dsn);
	return WriteRecord_S(dbapi, &af);
}

//**********************************************************************************
void CheckpointFile::EndOfAllocatedFileInfo()
{
	//No sense to keep committing when there are many files allocated.
	//* * 
	//On reflection, this isn't necessary at all.  The checkpoint file only
	//needs to be guaranteed on disk once it starts getting pre-images in it.  With
	//a long string of allocates, typically at system start up, this commit makes for
	//a significant start-up delay.
	//Note that file allocation info in the current release is always all at the
	//start of the checkpoint file, so this function will never be called when there
	//are any pre-images.
	//* * 
	//util::StdioCommit(stdio_handle, fname.c_str());

	//	TRACE("CCCCCCCCCCCCCCP End of alloc file info \n");
}

//**********************************************************************************
int CheckpointFile::WritePreImage
(DatabaseServices* dbapi, RawPageData* data, const char* ddname, int pagenum)
{
	num_pre_images++;
	CheckpointPreImageRecord pi(data, ddname, pagenum);
	int i = WriteRecord_S(dbapi, &pi);

	//The whole point of checkpointing is that we can rely on data being on disk
	util::StdioCommit(stdio_handle, fname.c_str());
//	TRACE("CCCCCCCCCCCCCCP Pre-image committed \n");

	return i;
}




//**********************************************************************************
//This clears down the file and rewrites a header record at checkpoint time
//**********************************************************************************
void CheckpointFile::Reinitialize(DatabaseServices* dbapi, time_t now)
{
	//Clear down the file (system is quiescent so don't need file lock here)
	WTSentry ws(dbapi->Core(), 16);

	util::StdioChsize(stdio_handle, fname.c_str(), 0);

	//During a long sequence of allocates or frees, this commit introduces a 
	//significant delay.  As per the comment above, the chkp file only needs to
	//be committed when there are pre-images involved.  In this case there are
	//by definition going to be none afterwardsd, but skip the commit if
	//there were none before either.
	if (num_pre_images != 0)
		util::StdioCommit(stdio_handle, fname.c_str());
	num_pre_images = 0;

	util::StdioLseek(stdio_handle, fname.c_str(), 0, SEEK_SET);
//	TRACE("CCCCCCCCCCCCCCP emptied file \n");

	CheckpointHeaderRecord h(now);
	WriteRecord_S(dbapi, &h);
}





//**********************************************************************************
//**********************************************************************************
//**********************************************************************************
//Record types
//**********************************************************************************
//**********************************************************************************
//**********************************************************************************

//**********************************************************************************
//Base class
//**********************************************************************************
char CheckpointFileRecord::buff[DBPAGE_SIZE + 100];
const int CheckpointFileRecord::RECTYPE_LEN = 4;

const char CheckpointHeaderRecord::RECTYPE = 'H';
const char CheckpointAllocatedFileRecord::RECTYPE = 'A';
const char CheckpointPreImageRecord::RECTYPE = 'P';

char* CheckpointFileRecord::Initialize(int l, char r) 
{
	memset(buff, 0, l); 

	buff[0] = '['; 
	buff[1] = r;  
	buff[2] = ']';  
	buff[3] = ' ';
	
	return buff + RECTYPE_LEN;
}

//**********************************************************************************
CheckpointFileRecord* CheckpointFileRecord::ReadIn
(int handle, const std::string& fname, void* preimage_buffer)
{
	if (util::StdioRead(handle, fname.c_str(), buff, RECTYPE_LEN) == 0)
		return NULL;

	CheckpointFileRecord* result;

	result = CheckpointHeaderRecord::Construct(handle, fname);
	if (result)
		return result;
	
	result = CheckpointAllocatedFileRecord::Construct(handle, fname);
	if (result)
		return result;

	try {
		result = CheckpointPreImageRecord::Construct(handle, fname, preimage_buffer);
		if (result)
			return result;
	}
	catch (CheckpointPreImageRecord*) {
		return NULL;
	}

	throw Exception(CHKP_MISC_BUG, "Bug: Unknown record type in checkpoint file");
}

//**********************************************************************************
//Just one of these - always the first record
//**********************************************************************************
const int CheckpointHeaderRecord::TIMESTAMP_LEN = 10;
const int CheckpointHeaderRecord::TOTAL_LEN = 
  RECTYPE_LEN
+ TIMESTAMP_LEN + 1
+ 2;

//*********************
int CheckpointHeaderRecord::FormatBuffer()
{
	char* ptr = Initialize(TOTAL_LEN, RECTYPE);

	unsigned int uitime = timestamp; //V2.24 gcc is worried about our %d
//	sprintf(ptr, "%d", timestamp);
	sprintf(ptr, "%d", uitime);
	ptr += (TIMESTAMP_LEN + 1);

	//CRLF for readability
	memcpy(ptr, "\r\n", 2);
	ptr += 2;

	return TOTAL_LEN;
}

//*********************
CheckpointHeaderRecord* CheckpointHeaderRecord::Construct
(int handle, const std::string& fname)
{
	if (buff[1] != RECTYPE)
		return NULL;

	util::StdioRead(handle, fname.c_str(), buff, TOTAL_LEN - RECTYPE_LEN);
	return new CheckpointHeaderRecord(atoi(buff));
}

//*********************
std::string CheckpointHeaderRecord::Info()
{
	std::string result("Header         : Checkpoint time = ");
	result.append(win::GetCTime(timestamp));
	result.append(" (UTC ");
	result.append(util::IntToString(timestamp));
	result.append(1, ')');

	return result;
}

//**********************************************************************************
//There's a set of these at the start of the file, but never anywhere else in the
//current release, since ALLOCATE causes a checkpoint.
//**********************************************************************************
const int CheckpointAllocatedFileRecord::DDNAME_LEN = 8;
//const int CheckpointAllocatedFileRecord::TOTAL_LEN = 
//  RECTYPE_LEN
//+ DDNAME_LEN + 1 
//+ _MAX_PATH + 1
//+ 2
//+ 2;


//*********************
int CheckpointAllocatedFileRecord::FormatBuffer()
{
	int dsnlen = strlen(dsname);

	int total_len = 
		RECTYPE_LEN
		+ DDNAME_LEN + 1
		+ dsnlen + 1
		+ 2;

	char* ptr = Initialize(total_len, RECTYPE);

	strcpy(ptr, ddname);
	ptr += (DDNAME_LEN + 1);

	strcpy(ptr, dsname);
	ptr += (dsnlen + 1);

	//CRLF for readability
	memcpy(ptr, "\r\n", 2);
	ptr += 2;

	return total_len;
}

//*********************
CheckpointAllocatedFileRecord* CheckpointAllocatedFileRecord::Construct
(int handle, const std::string& fname)
{
	if (buff[1] != RECTYPE)
		return NULL;

	//Dummy parms to satisfy this semi-kluge function
	bool dummy_simeol;
	char dummy_buff[MAX_PATH * 2];
	util::StdioReadLine(handle, fname.c_str(), buff, dummy_buff, dummy_simeol);

	return new CheckpointAllocatedFileRecord(buff, buff + DDNAME_LEN+1);
}

//*********************
std::string CheckpointAllocatedFileRecord::Info()
{
	std::string result("Allocated file : ");
	result.append(ddname);
	result.append(" (");
	result.append(dsname);
	result.append(1, ')');

	return result;
}

//**********************************************************************************
//The main data of the checkpoint file: page pre-images.
//**********************************************************************************
const int CheckpointPreImageRecord::FILENAME_LEN = 8;
const int CheckpointPreImageRecord::PAGENUM_LEN = 10;
const int CheckpointPreImageRecord::TOTAL_LEN = 
  RECTYPE_LEN
+ FILENAME_LEN + 1
+ PAGENUM_LEN + 1
+ 2
+ DBPAGE_SIZE 
+ 2;

//*********************
int CheckpointPreImageRecord::FormatBuffer()
{
	char* ptr = Initialize(TOTAL_LEN, RECTYPE);

	strcpy(ptr, filename);
	ptr += (FILENAME_LEN + 1);

	sprintf(ptr, "%d", pagenum);
	ptr += (PAGENUM_LEN + 1);

	//CRLF for readability
	memcpy(ptr, "\r\n", 2);
	ptr += 2;

	//Page data portion
	memcpy(ptr, data, DBPAGE_SIZE);
	ptr += DBPAGE_SIZE;

	//Another CRLF for readability
	memcpy(ptr, "\r\n", 2);
	ptr += 2;

	return TOTAL_LEN;
}

//*********************
CheckpointPreImageRecord* CheckpointPreImageRecord::Construct
(int handle, const std::string& fname, void* preimage_buffer)
{
	if (buff[1] != RECTYPE)
		return NULL;

	int required_len = TOTAL_LEN - RECTYPE_LEN;
	int record_len = util::StdioRead(handle, fname.c_str(), buff, required_len);

	//Partial records are OK - just discarded, since if the CP file write had failed
	//the file page itself would have never been updated.
	if (record_len != required_len)
		throw (CheckpointPreImageRecord*) NULL;

	//In the second pass we need to prepare a memory-aligned buffer containing the data,
	//so we can use unbuffered file IO to apply it back to the file.
	if (preimage_buffer)
		memcpy(preimage_buffer, buff + FILENAME_LEN+1 + PAGENUM_LEN+1 + 2, DBPAGE_SIZE);
	return new CheckpointPreImageRecord
		((RawPageData*)preimage_buffer, buff, atoi(buff + FILENAME_LEN+1));
}

//*********************
std::string CheckpointPreImageRecord::Info()
{
	std::string result("Pre-image      : ");
	result.append(filename);
	result.append(" page ");
	result.append(util::IntToString(pagenum));

	//Grab some info from the page data itself.  No aligned buffer copy is created
	//to do this (see Construct() above).
	result.append(" (type ");

	char pt = buff[FILENAME_LEN+1 + 
							PAGENUM_LEN+1 + 
							2 + 
							DBPAGE_PAGETYPE];

	if (pt == 0)
		result.append("unknown");
	else
		result.append(1, pt);
	
	result.append(1, ')');
	return result;
}





//**********************************************************************************
//**********************************************************************************
//**********************************************************************************
//Diagnostics
//**********************************************************************************
//**********************************************************************************
//**********************************************************************************
#ifdef _BBHOST
void CheckpointFile::Dump(IODev* op)
#else
void CheckpointFile::Dump(util::LineOutput* op)
#endif
{
	LockingSentry ls(&cplock);

	util::StdioLseek(stdio_handle, fname.c_str(), 0, SEEK_SET);

	CheckpointFileRecord* rec = NULL;
	try {
		for (;;) {
			rec = CheckpointFileRecord::ReadIn(stdio_handle, fname, NULL);
			if (!rec)
				break;

			op->WriteLine(rec->Info());
			delete rec;
			rec = NULL;
		}

		util::StdioLseek(stdio_handle, fname.c_str(), 0, SEEK_END);
	}
	catch (...) {
		if (rec)
			delete rec;

		util::StdioLseek(stdio_handle, fname.c_str(), 0, SEEK_END);
		throw;
	}

}

} //close namespace
