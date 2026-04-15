/*****************************************************************************************
  C stdio specialisation of line IO, specifically built to handle situations where the
file pointer might need repositioning.  This is particularly relevant in multi-user
situations, since even if a reposition is not requested explicitly, it might be
necessary to do so because another thread has moved the pointer.  This is why the
operations here all require the required position to be passed in, and the file pointer
is repositioned if appropriate. (Therefore not derived from LineInput/LineOutput).

  Note that text translation is turned off (see open mode constants), in all cases,
since to correctly reposition during multi-thread access requires accurate ftell().
From a performance point of view, when reading using this class the byte-by-byte 
scanning is done in custom code, which is probably slower than that provided by the
standard library.

  We always open for read and write as far as the OS is concerned.  We then control 
reading and writing by different threads with DPT-style locks.

  Also note that all operations handle files up to 2^63 in size.
*****************************************************************************************/

#if !defined(BB_LIOSHARE)
#define BB_LIOSHARE

#include "bbstdio.h"
#include "lockable.h"
#include "share.h"
#include <vector>
#include <string>

namespace dpt {
namespace util {

class SharableLineIO
{
#ifdef DEBUG_V225_OKTECH
	friend class ProcDirectoryContext;
#endif

	std::string filename_short;
	std::string filename_full;
	int handle;
	Lockable read_write_lock;
	bool commit_by_line;
	int sharemode; //retained for use in rename

	//See comments in normal lineio
	bool simulated_eol;

#ifdef DEBUG_V221_STDIO
	int* orig_handle;
	mutable bool corrupt_flagged;
	void CorruptHandleCheck(const char* s) const {if (handle != *orig_handle) 
		HandleCheckError("SharableLineIO", s, handle, *orig_handle, corrupt_flagged, 
		filename_short, filename_full);}
#endif

public:
	SharableLineIO(const std::string& name, int disp = STDIO_COND, 
		int shmode = _SH_DENYWR, int = _S_IREAD | _S_IWRITE);
	~SharableLineIO();
	void SetCommitByLine() {commit_by_line = true;}

	const char* GetFileNameShort() const {return filename_short.c_str();}
	const char* GetFileNameFull() const {return filename_full.c_str();}

	//These functions reposition the file pointer if necessary.  Note that in Read(..)
	//the value returned is the number of bytes used, (not necessarily the line length)
	//which can by the caller to maintain an accurate thread-local file position.
	int ReadLine(_int64, char*, int* = NULL);
	//This would not usually be used in a shared situation.  It's up to the caller though.
	int WriteLine(_int64, const char*, unsigned int = 0);
	//See comments in normal lineio
	bool SimulatedEOL() {return simulated_eol;}

	//---------------------------------------------------------------------------
	//Dodgy if called generally, as the info would be instantly out of date.
	//These would only be used if there is a higher level of locking in place.
	void SetPosition(_int64);
	_int64 Tell();
	void VerifyNotDeleted();
	void Truncate(_int64);
	_int64 GoToEnd();
	void Delete();
	void Rename(const char*);
	_int64 GetFileSize();
	time_t GetUpdatedTime() {return GetStdioFileUpdatedTime(handle, filename_full.c_str());}
	unsigned int WriteNoCRLF(_int64, const char*, unsigned int); 
	unsigned int ReadNoCRLF(_int64, char*, unsigned int); 
	//---------------------------------------------------------------------------

	//Used during editor SAVE
	int SaveFromBufferWithEOLs(std::vector<std::string>&);

	//Used during COPY PROC and COPY DATASET
	void CopyFrom(SharableLineIO*, bool olddate, bool truncate);
};

}} //close namespace

#endif
