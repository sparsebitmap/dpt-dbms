
#include "stdafx.h"

#include "paged_io.h"

//Utils
#include "winutil.h"
#include "dataconv.h"
#include "rawpage.h"
//Diagnostics
#include "except.h"
#include "msg_util.h"

namespace dpt {

//*********************************************************************************************
PagedFile::PagedFile(const std::string& fname, PagedFileDisp disp)
: m_name(fname), m_handle(INVALID_HANDLE_VALUE)
{
	//Here is where we open the file, and specify to Windows not to buffer our IO
	m_handle = CreateFile(
		m_name.c_str(), 
		GENERIC_READ | GENERIC_WRITE,
		0, 
		NULL, 
		disp, 
		FILE_FLAG_WRITE_THROUGH | FILE_FLAG_NO_BUFFERING,
		NULL
	);

	if (m_handle == INVALID_HANDLE_VALUE) {
		throw Exception(UTIL_PAGEDIO_ERROR,
			std::string("Error allocating file: ")
			.append(win::GetLastErrorMessage())
			.append(" (Dsn=")
			.append(m_name)
			.append(")")
			);
	}

	//See comment in header file
	m_page_size = DBPAGE_SIZE;
//	SetPageSize(pgsz);

	seqopt = 0;
	seqopt_buffer = NULL;
	seqopt_lo_page = UINT_MAX;
	seqopt_hi_page = UINT_MAX;
}

//*********************************************************************************************
PagedFile::~PagedFile()
{
	if (!CloseHandle(m_handle)) {
		throw Exception(UTIL_PAGEDIO_ERROR,
			std::string("Error closing OS file handle on file ")
			.append(m_name)
			.append("!: ")
			.append(win::GetLastErrorMessage()));
	}

	DeleteSeqoptBuffer();
}	

//*********************************************************************************************
void PagedFile::Throw_WINAPI(const std::string& msg) const
{
	throw Exception(UTIL_PAGEDIO_ERROR,
		std::string(msg)
		.append(" on file ")
		.append(m_name)
		.append(", OS message = ")
		.append(win::GetLastErrorMessage()));
}
//*******************************
void PagedFile::Throw_Logic(const std::string& msg) const
{
	throw Exception(UTIL_PAGEDIO_ERROR,
		std::string("Internal logic error processing file ")
		.append(m_name)
		.append(", specifically: ")
		.append(msg));
}

/*
// *********************************************************************************************
//Validate the page size (a key thing is that it must be a multiple of the disk sector
//size.  Otherwise there is no real upper limit on the page size
//as read/write speed increases (albeit with decreasing gains) as you go up.  Also it 
//seems to be more the read performance which is improved, since nearly the same 
//improvement can be got using SEQOPT.  Perhaps WriteFile is limited to 64K chunks or
//something.  Certainly the WINAPI CopyFile function delivers equal performance as 
//if we do a simple sequential read/write at a page size of 64K.  
// *********************************************************************************************
void PagedFile::SetPageSize(int pgsz, bool must_be_integral)
{
	//Use splitpath() to get the drive the file is being allocated on.
	char drive[_MAX_DRIVE];
	char dir[_MAX_DIR];
	char prefix[_MAX_FNAME];
	char ext[_MAX_EXT];
	_splitpath(m_name.c_str(), drive, dir, prefix, ext);
	//Prepend the current working directory path for completeness
	if (!strcmp(drive, "")) {
		char cwd[_MAX_PATH];
		_getcwd(cwd, _MAX_PATH);
		m_name = std::string(cwd).append(1, '\\').append(m_name);
	}

	DWORD spc; //sectors per cluster
	DWORD bps; //bytes per sector - what we're interested in here
	DWORD fc;  //free clusters
	DWORD tc;  //total clusters

	//This api call should work on C: according to the doc, but it doesn't, so make it C:\
	strcat(drive, "\\");
	if (!GetDiskFreeSpace(drive, &spc, &bps, &fc, &tc)) {
		throw Exception(UTIL_PAGEDIO_ERROR,
			std::string("Error checking disk attributes for drive ")
			.append(drive)
			.append(" ... ")
			.append(win::GetLastErrorMessage()));
	}

	if (pgsz % bps) {
		throw Exception(UTIL_PAGEDIO_ERROR,
			std::string("Requested page size of ")
			.append(util::IntToString(pgsz))
			.append(" not a multiple of disk sector size (")
			.append(util::IntToString(bps))
			.append(").  File name=")
			.append(m_name));
	}
		
	if (pgsz <= 0 ) {
		throw Exception(UTIL_PAGEDIO_ERROR,
			std::string("Requested page size of ")
			.append(util::IntToString(pgsz))
			.append(" invalid - opening file ")
			.append(m_name));
	}

	//Check to see if the current file size is a multiple of the requested page size.
	//This is waived during the copy function, to let us use larger blocking factors,
	//although the above sector multiple test is still important.
	if (must_be_integral) {
		unsigned _int64 fsize = GetNumBytes();
		if (fsize % pgsz) {
			throw Exception(UTIL_PAGEDIO_ERROR,
				std::string("File size must be a multiple of page size, on file ")
				.append(m_name)
				.append(".  File size = ")
				.append(util::Int64ToString(fsize))
				.append(", requested page size = ")
				.append(util::IntToString(pgsz)));
		}
	}

	m_page_size = pgsz;
}
*/

//*********************************************************************************************
unsigned _int64 PagedFile::GetNumBytes_U() const 
{
	ULARGE_INTEGER fsize;
	fsize.LowPart = GetFileSize(m_handle, &fsize.HighPart);

	if (fsize.LowPart == (DWORD)-1 && GetLastError() != NO_ERROR)
		Throw_WINAPI("Error checking file size");

	return fsize.QuadPart;
}

//*********************************************************************************************
unsigned _int64 PagedFile::GetFilePointerByte_U() const 
{
	LARGE_INTEGER quad;
	quad.QuadPart = 0;

	quad.LowPart = SetFilePointer(m_handle, 0, &quad.HighPart, FILE_CURRENT);

	if (quad.LowPart == (DWORD)-1 && GetLastError() != NO_ERROR)
		Throw_WINAPI("Error querying file pointer position");

	return quad.QuadPart;
}

//*********************************************************************************************
void PagedFile::Seek_U(unsigned _int32 pagenum, bool extend)
{
	//Check to see that this page exists.  When extending a file we can skip this check
	//because we do actually want to put the pointer past the end of the file.
	if (!extend) {
		if (pagenum >= GetNumPages_U()) {
			Throw_Logic(std::string("Page number outside file (")
				.append(util::IntToString(pagenum)).append(") "));
		}
	}

	if (pagenum == GetFilePointerPage_U()) 
		return;

	LARGE_INTEGER quad;
	_int64 pagesize64 = m_page_size;          //to get 64 bit multiply result
	quad.QuadPart = pagenum * pagesize64;
	quad.LowPart = SetFilePointer(m_handle, quad.LowPart, &quad.HighPart, FILE_BEGIN);

	if (quad.LowPart == (DWORD)-1 && GetLastError() != NO_ERROR)
		Throw_WINAPI("Error setting file pointer position");
}

//*********************************************************************************************
//This function positions the file pointer and simply changes Windows' notion of what area of
//disk space is occupied by the file.  In other words unlike the C runtime _chsize() the 
//contents are undetermined when the file is extended, instead of filled with zeroes.
//This is more what we want for database files as it's quicker and fresh pages will be
//initialized as they're used anyway.
//*********************************************************************************************
void PagedFile::SetNumPages_U(unsigned _int32 new_numpages)
{
	//Assume the caller knows what they're doing so allow past-the-end set.  Usually with
	//M204-style files we will use Increase and Decrease.
	Seek_U(new_numpages, true);
	if (!SetEndOfFile(m_handle))
		Throw_WINAPI("Error setting file size to absolute value");
}

//*********************************************************************************************
//Externally-callable versions of the above
//*********************************************************************************************
void PagedFile::SetSize(unsigned _int32 newsize)
{
	LockingSentry ls(&iolock);
	SetNumPages_U(newsize);
}

//*********************************************************************************************
unsigned _int32 PagedFile::GetSize() const
{
	LockingSentry ls(&iolock);
	return GetNumPages_U();
}

//*********************************************************************************************
_int64 PagedFile::GetNumBytes() const
{
	LockingSentry ls(&iolock);
	return GetNumBytes_U();
}

//*********************************************************************************************
void PagedFile::IncreaseSize(unsigned _int32 diff)
{
	LockingSentry ls(&iolock);

	//Set file position past end of file
	Seek_U(GetNumPages_U() + diff, true);
	if (!SetEndOfFile(m_handle))
		Throw_WINAPI("Error increasing file size");
}

//*********************************************************************************************
void PagedFile::DecreaseSize(unsigned _int32 diff)
{
	LockingSentry ls(&iolock);

	Seek_U(GetNumPages_U() - diff);
	if (!SetEndOfFile(m_handle))
		Throw_WINAPI("Error decreasing file size");
}

//*********************************************************************************************
//The caller is responsible for locking and positioning the file pointer in these 2 versions.
//*********************************************************************************************
int PagedFile::ReadBasic(void* dest, int numpages)
{
	DWORD bytes_read = 0;
	DWORD bytes_required = m_page_size * numpages;
	BOOL rc = ReadFile(m_handle, dest, bytes_required, &bytes_read, NULL);

	if (!rc)
		Throw_WINAPI("Physical read error");

	if (bytes_read == 0)
		Throw_Logic("Physical read started past EOF");

	if (bytes_read % m_page_size)
		Throw_Logic("Non-integral page size or file pointer position");

	return bytes_read / m_page_size;
}

//*********************************************************************************************
void PagedFile::WriteBasic(void* source, int numpages)
{
	DWORD bytes_written = 0;
	BOOL rc = WriteFile(m_handle, source, m_page_size * numpages, &bytes_written, NULL);

	if (!rc) 
		Throw_WINAPI("Physical write error");
}

//*********************************************************************************************
//But here we do random access
//*********************************************************************************************
void PagedFile::ReadPage(unsigned _int32 pagenum, void* dest)
{
	LockingSentry ls(&iolock);

	Seek_U(pagenum, false);
	ReadBasic(dest, 1);
}

//*********************************************************************************************
bool PagedFile::ReadPageWithSeqopt(unsigned _int32 pagenum, void* dest)
{
	if (seqopt == 0) {
		ReadPage(pagenum, dest);
		return true;
	}

	LockingSentry ls(&iolock);

	//Can we satisfy the read from the seqopt buffer
	if (seqopt != 0 && pagenum >= seqopt_lo_page && pagenum <= seqopt_hi_page) {
		int sbix = pagenum - seqopt_lo_page;
		memcpy(dest, seqopt_buffer + sbix, DBPAGE_SIZE);
		return false;
	}

	seqopt_lo_page = UINT_MAX;
	seqopt_hi_page = UINT_MAX;

	Seek_U(pagenum, false);

	//SEQOPT is the number of *extra* pages to read in
	int got_pages = ReadBasic(seqopt_buffer, seqopt+1);
	
	//Towards the end of the file we might not get the full requested amount
	seqopt_lo_page = pagenum;
	seqopt_hi_page = seqopt_lo_page + got_pages - 1;

	//Take the first one straight away
	memcpy(dest, seqopt_buffer, DBPAGE_SIZE);
	return true;
}

//*********************************************************************************************
void PagedFile::WritePage(unsigned _int32 pagenum, void* source)
{
	LockingSentry ls(&iolock);
	Seek_U(pagenum, false);

	WriteBasic(source, 1);
}

//******************************************************************************************
void PagedFile::SetSeqopt(int s)
{
	LockingSentry ls(&iolock);

	if (s == seqopt)
		return;
	
	DeleteSeqoptBuffer();

	//Allocate new intermediate buffer (make sure we get aligned pages so use WINAPI)
	if (s != 0) {
		seqopt_buffer = static_cast<RawPageData*>(
			VirtualAlloc(NULL, (s+1) * DBPAGE_SIZE, MEM_COMMIT, PAGE_READWRITE));

		if (seqopt_buffer == NULL)
			throw Exception(UTIL_PAGEDIO_ERROR, std::string
				("Error reserving SEQOPT multibuffer, OS message was: ")
				.append(win::GetLastErrorMessage()));
	}

	seqopt = s;
}

//***************************************************************************************
void PagedFile::DeleteSeqoptBuffer()
{
	if (seqopt_buffer)
		VirtualFree(seqopt_buffer, 0, MEM_RELEASE);
	seqopt_buffer = NULL;
}





/*
// *********************************************************************************************
//Copy function with progress reporting
// *********************************************************************************************
void PagedFile::CopyTo(const std::string& copyname, PagedFileDisp copydisp,
					ProgressFunction progfunc, int chunksize, int interval)
{
	LockingSentry ls(&iolock);

	//This constructor is used for convenience in checking the parms, but we'll not
	//be using the writing function on it to do the copy.
	PagedFile thecopy(copyname, m_page_size, copydisp);
	thecopy.SetNumPages_U(GetNumPages());
	thecopy.Seek_U(0);

	//The copy is done in user-specified increments, with a default of 64K.  This is what
	//the WINAPI CopyFile does it in.  In tests 128K gave 5/8 the time, and 640K about 1/3.
	//There is obviously a memory trade-off, in terms of what else will get paged out, but 
	//in a reorg, say, we might use a pretty large value like 1Mb when making the backup copy.
	unsigned int num_chunks = GetNumBytes() / chunksize;
	unsigned int remainder = GetNumBytes() % chunksize;
	void* copy_buffer;
	copy_buffer = VirtualAlloc(NULL, chunksize, MEM_COMMIT, PAGE_READWRITE);
	if (copy_buffer == NULL)
		Throw_WINAPI("Error allocating copy buffer");

	//To save repeating code here we temporarily set the page size of both files to be
	//the chunk size
	int old_pagesize = m_page_size;

	//Note the time for progress reporting
	time_t interval_start;
	time(&interval_start);
	progfunc(0, "Start of file copy.");

	try {
		SetPageSize(chunksize, false);

		//This may do an undersized chunk at the end of the file, which is OK for Read(),
		//but not for Write(), as it would extend the file, so inline WriteFile() call here.
		for (int chunk = 0; chunk <= num_chunks; ++chunk) {
			if (chunk == num_chunks) if (remainder == 0) break;

			int bytes_read = Read(chunk, copy_buffer);
			DWORD bytes_written = 0;
			if (0 == WriteFile(thecopy.m_handle, copy_buffer, bytes_read, &bytes_written, NULL))
				Throw_WINAPI("Write error");

			time_t now;
			time(&now);
			if (now != interval_start) {
				double fraction = chunk;
				fraction /= num_chunks;
				progfunc(fraction, "Copying...");
				interval_start = now;
			}
		}

		progfunc(1, "End of file copy.");

		//Restore previous settings
		VirtualFree(copy_buffer, NULL, MEM_RELEASE);
		SetPageSize(old_pagesize);
	}
	catch (...) {
		VirtualFree(copy_buffer, NULL, MEM_RELEASE);
		SetPageSize(old_pagesize);
		throw;
	}
}
*/

//*********************************************************************************************
//Take 2 :-)
//*********************************************************************************************
PagedFileCopyOperation::PagedFileCopyOperation
(PagedFile* s, PagedFile* d, ProgressFunction pf, void* pc, int ppb)
: ProgressReportableActivity(
		std::string("Pagewise copy of file ")
			.append(s->GetName()).append(" to ").append(d->GetName()),
			pf, pc, NULL),
  source(s), destination(d), io_buffer(NULL), pages_per_block(ppb)
{
	if (source->m_page_size != destination->m_page_size)
		s->Throw_Logic("Incompatible page sizes to copy");

	//See comments in Take 1 above
	int buffersize = pages_per_block * source->m_page_size;
	io_buffer = VirtualAlloc(NULL, buffersize, MEM_COMMIT, PAGE_READWRITE);
	if (io_buffer == NULL)
		s->Throw_WINAPI("Error allocating buffer for copy ");
}

//*********************************************************************************************
PagedFileCopyOperation::~PagedFileCopyOperation()
{
	if (io_buffer)
//		VirtualFree(io_buffer, NULL, MEM_RELEASE); //V2.24 fair play to ya gcc.
		VirtualFree(io_buffer, 0, MEM_RELEASE);
}

//*********************************************************************************************
void PagedFileCopyOperation::Perform()
{
	total_pages = source->GetSize();
	pages_copied = 0;

	//Start off with a zero-pages-copied message
	ProgressReport(PROGRESS_PROCEED);

	//Lock and load
	LockingSentry lss(&source->iolock);
	LockingSentry lsd(&destination->iolock);

	destination->SetNumPages_U(0);
	destination->Seek_U(0, true);
	source->Seek_U(0);

	for (;;) {
		if (pages_copied >= total_pages)
			break;

		int got_pages = source->ReadBasic(io_buffer, pages_per_block);
		destination->WriteBasic(io_buffer, got_pages);

		pages_copied += got_pages;
		if (ProgressReport(PROGRESS_PROCEED | PROGRESS_CANCEL_ACTIVITY) 
							== PROGRESS_CANCEL_ACTIVITY)
			throw Exception(UTIL_PAGEDIO_ERROR, 
				"File copy operation cancelled by user");
	}
}


} //close namespace
