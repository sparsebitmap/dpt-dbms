
#include "stdafx.h"
#include "dbstatus.h"
#include <time.h>

//Utils
//API tiers
#include "recread.h"
#include "dbfile.h"
//Diagnostics
#include "except.h"
#include "msg_core.h"

namespace dpt {

std::map<std::string, FileStatusInfo*> FileStatusInfo::systable;
#ifdef _DEBUG_LOCKS
Lockable FileStatusInfo::syslock = Lockable("FileStatusInfo syslock");
#else
Lockable FileStatusInfo::syslock;
#endif

//******************************************************************************************
void FileStatusInfo::SetAllocTime() 
{
	if (file) 
		time(&alloc_time); 
	else 
		alloc_time = 0;
}

//******************************************************************************************
void FileStatusInfo::DestroyStatusInfo()
{
	LockingSentry ls(&syslock);

	std::map<std::string, FileStatusInfo*>::iterator i;
	for (i = systable.begin(); i != systable.end(); i++)
		delete i->second;

	systable.clear(); //V2.16
}

//******************************************************************************************
void FileStatusInfo::Attach(const std::string& dd, DatabaseFile* f)
{
	LockingSentry ls(&syslock);

	//I hate this rignarole but still prefer it to a double search with operator[]
	std::pair<std::string, FileStatusInfo*> newtableitem;
	newtableitem = std::make_pair<std::string, FileStatusInfo*>(dd, NULL);
	
	std::pair<std::map<std::string, FileStatusInfo*>::iterator, bool> ins;
	ins = systable.insert(newtableitem);

	//The file may be there if it participated in recovery, in which case we just update
	//the file pointer.  Actually this would also happen if a file is manually freed and 
	//another file allocated with the same name later.  Who cares.
	if (ins.second)
		ins.first->second = new FileStatusInfo;

	ins.first->second->SetFile(f);

	//The only time the file pointer would be null is at the start of recovery
	if (f == NULL)
		ins.first->second->recovery_required = true;
}

//******************************************************************************************
void FileStatusInfo::Detach(const std::string& dd)
{
	LockingSentry ls(&syslock);

	std::map<std::string, FileStatusInfo*>::iterator i = systable.find(dd);

	if (i == systable.end())
		throw Exception(BUG_MISC, "Bug: File status information has gone!");

	//The status info will not now be attached to a currently-allocated file
	i->second->SetFile(NULL);
}

//******************************************************************************************
void FileStatusInfo::Dump(std::vector<DumpItem>& result)
{
	LockingSentry ls(&syslock);

	std::map<std::string, FileStatusInfo*>::iterator mi;
	for (mi = systable.begin(); mi != systable.end(); mi++) {

		DumpItem dumpee;

		dumpee.filename = mi->first;
		FileStatusInfo* info = mi->second;

		dumpee.alloc_time = info->alloc_time;
		dumpee.recovery_required = info->recovery_required;
		if (dumpee.recovery_required)
			dumpee.recovery_failure_reason = info->recovery_failure_reason;

		//The following info is dynamically acquired - must be careful.
		DatabaseFile* f = info->file;

		if (!f) {
			dumpee.open = false;
			dumpee.updated_ever = false;
			dumpee.updated_since_checkpoint = false;
		}
		else {
			//No deadlock risk just taking a single stab at CFR_FILE.
			dumpee.open = f->AnybodyHasOpen();

			//These are also non-waiting - see docs for more on use of the UPDATING lock.
			dumpee.updated_ever = f->IsPhysicallyUpdatedEver();
			dumpee.updated_since_checkpoint = f->IsPhysicallyUpdatedSinceCheckpoint();
		}

		//This is just cached now for the calling code and we can release the lock
		result.push_back(dumpee);
	}
}

} //close namespace


