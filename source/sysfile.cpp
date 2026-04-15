
#include "stdafx.h"

#include "sysfile.h"

//Diagnostics
#include "except.h"
#include "msg_file.h"
#include "msg_util.h"

namespace dpt {

//*****************************************************************************************
SystemFile::SystemFile(const std::string& parm_dd, std::string& parm_dsn, bool lk)
: AllocatedFile(parm_dd, parm_dsn, FILETYPE_SYS)
{
	//System files are not opened and closed dynamically, so we can define their SHR/EXCL
	//nature at creation time.  See doc for more on the difference from the handle lock.
	TryForOpenLock(lk);

	//No sophisticated free/allocate scheme for system files
	pending = false;
}

//*****************************************************************************************
SystemFile::~SystemFile() 
{
	//Some of the system files are closed on different threads to where they were opened.
	//Examples are the audit trail if the last user off is not the same as the first one
	//on, and the input and output files for IODev3 and 99 threads.  Since these are only
	//closed when the thread terminates, there's no real problem with leaving them 
	//enqueued just before destruction.  We will know by that time it's OK to destroy.
	//See similar comment in Destroy() below.
	
	//V2 Jan 07.
	//This is much cleaner.  Resource::Release() now takes a parameter which says
	//to release any lock placed by any user.
//	try {
		ReleaseOpenLock(true);
//	} 
//	catch (Exception& e) {
//		if (e.Code() != UTIL_RESOURCE_ANOTHERS) 
//			throw;
//	}
}

//******************************************************************************************
FileHandle SystemFile::Construct(const std::string& parm_dd, std::string& parm_dsn, bool lk)
{
	FileHandle result;
	{
		//Lock across the *whole* constructor, in case derived part throws
		LockingSentry ls(&allocation_lock);

		//Note that these files are always created and destroyed in special circumstances 
		//(e.g. by the first/last user on).  Therefore always using SHR handles is OK.
		//See doc for more on this.  See also the open lock above.
		result = FileHandle(new SystemFile(parm_dd, parm_dsn, lk), BOOL_SHR);
	}

	result.CommitAllocation();
	return result;
}

//******************************************************************************************
void SystemFile::Destroy(FileHandle& h)
{
	//When handles for e.g. IODev IO files are passed around during thread initiation
	//I've erred on the side of caution in terms of the number of calls to this function,
	//mainly because I've got so bored of this bit and can't be bothered to trace through
	//the excact sequence of handle copies one more time. >8-O
	if (!h.IsEnabled())
		return;

	h.StageForFree();

	//A necessary consequence of the above
	LockingSentry ls(&allocation_lock);

	SystemFile* sf = static_cast<SystemFile*>(h.GetFile());
	delete sf;
}


} //close namespace

