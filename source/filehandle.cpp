
#include "stdafx.h"

#include "file.h"

//Diagnostics
#include "except.h"
#include "msg_file.h"

namespace dpt {

//******************************************************************************************
FileHandle::FileHandle(AllocatedFile* af, bool t)
: afile(af), type(t), enabled(false)
{
	if (!afile)
		return;

//	TRACE ("$$ HANDLE $$ Lock: %s %s \n", 
//		af->GetDDName().c_str(), (t == BOOL_EXCL) ? "EXCL" : "SHR");

	bool result;
	if (type == BOOL_EXCL)
		result = afile->alloc_lock.AttemptExclusive();
	else
		result = afile->alloc_lock.AttemptShared();

	if (!result) {
		std::string msg("File is in use");
		if (af->GetDDName().length() > 0 && af->GetDDName()[0] != '+')
			msg.append(": ").append(af->GetDDName());

		throw Exception(SYSFILE_OPEN_FAILED, msg);
	}

	enabled = true;
}

//******************************************************************************************
int FileHandle::FileID()
{
	return afile->file_id;
}

//******************************************************************************************
void FileHandle::Release()
{
	if (!enabled)
		return;

//	TRACE ("$$ HANDLE $$ Release: %s %s \n", 
//		afile->GetDDName().c_str(), (type == BOOL_EXCL) ? "EXCL" : "SHR");

	if (type == BOOL_EXCL)
		afile->alloc_lock.ReleaseExclusive();
	else
		afile->alloc_lock.ReleaseShared();

	enabled = false;
}

//******************************************************************************************
void FileHandle::CopyFrom(const FileHandle& h)
{
	//Can't hold 2 locked files, so release any currently controlled by this handle
	Release();

	//Adopt responsibility for the lock
	afile = h.afile; 
	type = h.type;
	enabled = h.enabled;

	//The previous owner now no longer has to release it
	h.enabled = false;
}

//******************************************************************************************
const std::string& FileHandle::GetDD() const {return afile->dd;}
const std::string& FileHandle::GetDSN() const {return afile->dsn;}
const std::string& FileHandle::GetAlias() const {return afile->alias;}
FileType FileHandle::GetType() const {return afile->type;}

//******************************************************************************************
void FileHandle::CommitAllocation()
{
	afile->CommitAllocation();

//	TRACE ("$$ HANDLE $$ Commit alloc: %s \n", afile->GetDDName().c_str());

	//In case of backing off a free
	enabled = true;
}

//******************************************************************************************
void FileHandle::StageForFree()
{
	afile->StageForFree();

//	TRACE ("$$ HANDLE $$ Stage free: %s \n", afile->GetDDName().c_str());

	enabled = false;
}

} //close namespace


