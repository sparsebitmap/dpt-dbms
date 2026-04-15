
#include "stdafx.h"

#include "lockspecial.h"

//Utils
//API Tiers
#include "dbfile.h"
#include "dbf_rlt.h"
#include "frecset.h"
//Diagnostics

namespace dpt {

//*****************************************************************************
FileRecordSet_LockSpecialSingle::FileRecordSet_LockSpecialSingle(DatabaseFile* f, int rn)
: FileRecordSet(f), recnum(rn)
{
	SegmentRecordSet* s = new SegmentRecordSet_SingleRecord(recnum);

	try {
		data[s->SegNum()] = s;
	}
	catch (...) {
		delete s;
		throw;
	}
}




//***************************************************************************************
bool FileRecordSet_RecordUpdate::ApplyLock(DatabaseServices* d)
{
	//This will not take a new lock if the thread has the record LPU locked already
	lock = file->GetRLTMgr()->ApplyRecordUpdateLock(this, d);
	return (lock) ? true : false;
}

//***************************************************************************************
void FileRecordSet_RecordUpdate::Unlock()
{
	//Pass in record number for optimization in the RLT
	if (lock)
		file->GetRLTMgr()->ReleaseRecordUpdateLock(RecNum(), lock);
	lock = NULL;
}




//***************************************************************************************
void FileRecordSet_PAI::ApplyLock(DatabaseServices* d)
{
	lock = file->GetRLTMgr()->ApplyShrFoundSetLock(this, d);
}

//***************************************************************************************
void FileRecordSet_PAI::Unlock()
{
	if (lock)
		file->GetRLTMgr()->ReleaseNormalLock(lock);
	lock = NULL;
}



} //close namespace


