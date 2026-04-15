/********************************************************************************************
File IO class which provides for the reading and writing of data in fixed-size pages/blocks.

Originally started off as a general-purpose class but I've cut it down a bit as the only
place it's going to be used at the moment is for database files.  Some of the old code
(e.g. variable page size) is still there commented out or just in stub form.

The class is based on the Windows API file IO functions, as the easiest way to achieve
unbuffered (at least as far as Windows is concerned) file access.  The Windows file caching
system degraded performance for our kind of file access, plus Baby204 does its own buffering 
anyway.

********************************************************************************************/

#if !defined(BB_PAGED_IO)
#define BB_PAGED_IO

#include <string>

#include "lockable.h"
#include "progress.h"
#include "bbstdio.h"
#include "windows.h"

namespace dpt {

enum PagedFileDisp {
	PFD_OLD		= OPEN_EXISTING, 
	PFD_NEW		= CREATE_NEW, 
	PFD_COND	= OPEN_ALWAYS 
};

class PagedFileCopyOperation;
class RawPageData;

class PagedFile {
	std::string m_name;
	HANDLE m_handle;
	int m_page_size;
	int sector_size;

	//Extra level of physical buffering for when SEQOPT is in use
	int seqopt;
	RawPageData* seqopt_buffer;
	unsigned int seqopt_lo_page; //V2.24 Now unsigned.
	unsigned int seqopt_hi_page; //""
	void DeleteSeqoptBuffer();

	//CFRs won't be held when dirty pages are reused (and maybe flushed too)
	Lockable iolock;

//	HANDLE GetHandle() const {return m_handle;}
	void SetPageSize(int, bool = true);

	//Utility
	void Seek_U(unsigned _int32 page_num, bool extend = false);
	unsigned _int64 GetFilePointerByte_U() const;
	unsigned _int32 GetFilePointerPage_U() const {return GetFilePointerByte_U() / m_page_size;}
	unsigned _int64 GetNumBytes_U() const; 
	unsigned _int32 GetNumPages_U() const {return GetNumBytes_U() / m_page_size;}
	void SetNumPages_U(unsigned _int32 new_num_pages);	

	friend class PagedFileCopyOperation;
	void Throw_WINAPI(const std::string&) const;
	void Throw_Logic(const std::string&) const;
	int ReadBasic(void* dest, int);	
	void WriteBasic(void* source, int);

public:
	PagedFile(const std::string&, PagedFileDisp = PFD_COND);
	~PagedFile();

	const std::string& GetName() const {return m_name;}

//NB. the following functions apply a lock.

	//File size
	_int64 GetNumBytes() const; //V2.27
	unsigned _int32 GetSize() const;
	void SetSize(unsigned _int32 new_num_pages);	
	void IncreaseSize(unsigned _int32 diff);
	void DecreaseSize(unsigned _int32 diff);
	
	//Paged IO
	void ReadPage(unsigned _int32, void*);	
	void WritePage(unsigned _int32, void*);

	void SetSeqopt(int);
	bool ReadPageWithSeqopt(unsigned _int32, void*);	

//	void CopyTo(const std::string&, PagedFileDisp, 
//		ProgressFunction = SimplyContinue, int chunk = 65536, int interval = 1);
};

//*************************************************************************************
class PagedFileCopyOperation : public ProgressReportableActivity {
	PagedFile* source;
	PagedFile* destination;
	void* io_buffer;

	unsigned int pages_per_block;
	unsigned int total_pages;
	unsigned int pages_copied;

public:
	PagedFileCopyOperation(PagedFile*, PagedFile*, 
		ProgressFunction = NULL, void* = NULL, int = 128);
	~PagedFileCopyOperation();

	void Perform();

	//Reporting
	int NumSteps() const {return total_pages;}
	int StepsComplete() const {return pages_copied;}
	double PercentComplete() const {
		double n=NumSteps(); double c=StepsComplete(); return c/n*100;}
};

} //close namespace

#endif
