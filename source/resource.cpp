
#include "stdafx.h"

#include "resource.h"

#include "windows.h"
#include <time.h>
#include <algorithm>

//Constants
#include "winutil.h"
//Utils
#include "dataconv.h"
//API Tiers
#include "core.h"
//Diagnostics
#include "except.h"
#include "msg_util.h"

//CFR waits are neater if we integrate them with the code at this level
#ifdef _BBDBAPI
#include "dbserv.h"
#endif

namespace dpt {

//Define static objects
std::map<std::string, Resource*> Resource::all_objects;

#ifdef _DEBUG_LOCKS
Lockable Resource::instances_lock = Lockable("Resource instances");
#else
Lockable Resource::instances_lock;
#endif

//#define BBRESOURCE_TRACE_CONSTRUCT_DESTRUCT

//******************************************************************************************
//Constructors and destructors
//******************************************************************************************
Resource::Resource(const std::string& n) 
: name(n), excl_info(NULL)
#ifdef _DEBUG_LOCKS
, obj_lock(std::string("Resource: ").append(n))
#endif
{
#ifdef BBRESOURCE_TRACE_CONSTRUCT_DESTRUCT
	char buff[512];
	strcpy(buff, n.c_str());
	TRACE("RRRRRRRRRRRRRRRRRRRRRRRRRRRRR   New Resource( %x ) name=%s \n", this, buff);
	TRACE("                                    %d objects prior to construction\n", all_objects.size());
#endif

	LockingSentry s(&instances_lock);

	//This guards against the situation where we have derived objects which do creation
	//time checks in their constructors, specifically for duplicate name.  If we allowed
	//that it would mean that attempts to create dupe name objects would delete the 
	//original name from our table here when the Resource sub-object got deleted - bad.
	if (all_objects.find(n) != all_objects.end())
		throw Exception(UTIL_RESOURCE_EXISTENCE, "Resource already exists - internal bug?");
	
	all_objects[name] = this;
}

//******************************************************************************************
Resource::~Resource() 
{
#ifdef BBRESOURCE_TRACE_CONSTRUCT_DESTRUCT
	TRACE("RRRRRRRRRRRRRRRRRRRRRRRRRRRRR   Resource::~Resource( %x ) name=%s \n", this, name.c_str());
	TRACE("                                    %d objects prior to deletion\n", all_objects.size());
#endif

	LockingSentry s(&obj_lock);

	//If anybody else thinks they hold the resource, they're going to be in trouble.
	if (excl_info) 
		delete excl_info;

	LockingSentry s1(&instances_lock); //ensure stable directory during delete
	all_objects.erase(name);
}

//******************************************************************************************
//Nested lock info structure functions
//******************************************************************************************
Resource::LockInfo::LockInfo(bool b, bool dummy) : lock_type(b)
{
	time(&start_time);

	//The dummy lock will block everyone, but anyone can release it.  Used by some
	//system resources that anyone can create or delete.
	thread_id = (dummy) ? DUMMY_THREAD : GetCurrentThreadId();
}

//******************************************************************************************
//This is used by std::list::remove when removing a waiting record, as stored during Get().
//A thread can only ever wait for the same resource once (since Get does not return until
//the resource is acquired), so testing thread ID alone here is sufficient.
//******************************************************************************************
bool Resource::LockInfo::operator==(const LockInfo& l) {
	if (l.thread_id == thread_id) 
		return true;
	else
		return false;
}

//******************************************************************************************
std::string Resource::LockInfo::AsString() const
{
	std::string result;
	result = (lock_type == BOOL_EXCL) ? "EXCL/" : "SHR/";
	result.append("Tid=").append(util::IntToString(thread_id)).append(1, '/');

	tm t = win::GetDateAndTime_tm(start_time);
	result.append(util::IntToString(t.tm_hour)).append(1, ':');
	result.append(util::IntToString(t.tm_min)).append(1, ':');
	result.append(util::IntToString(t.tm_sec));
	return result;
}

//******************************************************************************************
//Resource acquisition and waiting functions
//******************************************************************************************
//Attempt once but don't wait
bool Resource::Try(bool required_lock, bool dummy)
{
	LockingSentry s(&obj_lock); //ensure we have a clear run at this object
	return Try_Unlocked(required_lock, dummy);
}

//Used in upgrade too
bool Resource::Try_Unlocked(bool required_lock, bool dummy)
{
	if (excl_info != NULL) 
		return false;

	if (required_lock == BOOL_EXCL) {
		if (sharer_info.size()) 
			return false;

		excl_info = new LockInfo(BOOL_EXCL, dummy);
		return true;
	}
	
	//Might be nice to fail if we already have the lock, but unnecessarily complicated,
	//and this case would probably show up with other nasty behaviour anyway.  Actually on
	//occasion it's useful to be able to share a resource with oneself, and I have used that
	//e.g. in group sharing, so it would be non trivial to change now.
	sharer_info.push_back(LockInfo(BOOL_SHR, dummy));
	return true;
}

//******************************************************************************************
//Wait until necessary
//******************************************************************************************
void Resource::Get(bool required_lock, CoreServices* core
#ifdef _BBDBAPI
   , DatabaseServices* dbapi
#endif
   , bool dummy)
{
	//Make a single initial attempt to see if we will have to wait at all
	if (Try(required_lock, dummy)) return;

	//Join the queue of waiters
	LockInfo li(required_lock);
	{
		LockingSentry ls(&obj_lock);
		waiter_info.push_back(li);

#ifdef _BBDBAPI
		//CFRs have the extra requirement that we should increment stats
		if (dbapi) {
			//This says we had to wait
			dbapi->IncStatWTCFR();

			//This says the locking user made us wait
			if (excl_info) {
				ThreadID locking_thread = excl_info->thread_id;
				int locking_userno = CoreServices::GetUsernoOfThread(locking_thread);

				//The locker shouldn't have logged off, but this is standard code so use it
				DBAPILockInSentry s(locking_userno);
				DatabaseServices* holder_dbapi = s.GetPtr();
				if (holder_dbapi)
					holder_dbapi->IncStatBLKCFRE();
			}
		}
#endif
	}

	//Now sleep intermittently until we can get the resource
	do {
		//Aggressive spinning here - some resources are very heavily used.
		win::Cede();
		
		//This is mainly so the =RESGET command can be bumpable and also join the waiters
		//list which makes it clearer what's going on.  CFR waits aren't bumpable.
		if (core
#ifdef _BBDBAPI
				&& !dbapi
#endif
							)
		{
			core->Tick("trying to acquire resource explicitly");
		}

	} while (!Try(required_lock));

	//Finally remove the waiting record
	LockingSentry ls(&obj_lock);
	waiter_info.remove(li);
}

//******************************************************************************************
bool Resource::TryChangeLock(bool new_lock, bool currently_held, bool dummy)
{
	LockingSentry s(&obj_lock); //ensure we have a clear run at this object

	//The caller should be sure that they currently hold it...
	bool prev_lock;
	if (currently_held) {
		if (excl_info != NULL)
			prev_lock = BOOL_EXCL;
		else
			prev_lock = BOOL_SHR;

		//...otherwise this will throw
		Release_Unlocked();
	}

	//If parm 2 is false, this function is identical to just plain Try()
	if (Try_Unlocked(new_lock, dummy))
		return true;
	
	//The revert shouldn't be able to fail as we are locking this whole function
	if (currently_held)
		Try_Unlocked(prev_lock, dummy);
	return false;
}

//******************************************************************************
//This function releases a held lock.  Because only one type of lock can ever
//be held (see Try() above), no lock type parameter is necessary - it will 
//release whichever is held.  The function has no return code, but might throw
//an exception indicating a sequence error if the resource is not held at all.
//******************************************************************************
void Resource::Release(bool anylocker)
{
	LockingSentry s(&obj_lock);
	Release_Unlocked(anylocker);
}

//Used in the upgrade function too
void Resource::Release_Unlocked(bool anylocker)
{
	ThreadID t = GetCurrentThreadId();

	if (excl_info != NULL) {

		//Anybody can release the dummy lock
		if (excl_info->thread_id != t && excl_info->thread_id != DUMMY_THREAD) {
			//V2 Jan 07.  Or any other lock if they call release with dummy parm.
			if (!anylocker) {
				throw Exception(UTIL_RESOURCE_ANOTHERS, 
					"Attempt to release resource held by another thread");
			}
		}

		delete excl_info;
		excl_info = NULL;
		return;
	}

	for (std::list<LockInfo>::iterator i = sharer_info.begin(); i != sharer_info.end(); i++) {
		//assume thread only has one entry in list, or at least if it has more than one
		//we only want to remove one (see earlier comment).
		//V2 Jan 07.  See above.
//		if (i->thread_id == t || i->thread_id == DUMMY_THREAD) {
		if (i->thread_id == t || i->thread_id == DUMMY_THREAD || anylocker) {
			sharer_info.erase(i);
			return;
		}
	}

	if (sharer_info.size()) {
		throw Exception(UTIL_RESOURCE_ANOTHERS, 
			"Attempt to release resource that is not held, but held by other thread(s)");
	}
	else
		throw Exception(UTIL_RESOURCE_NOT_HELD, 
			"Attempt to release resource that is not held");
}

//******************************************************************************
//Used by MONITOR RESOURCE
//******************************************************************************
std::vector<std::string> Resource::Monitor()
{
	LockingSentry s(&instances_lock);
	std::vector<std::string> result;

	std::map<std::string, Resource*>::const_iterator i;
	for (i = all_objects.begin(); i != all_objects.end(); i++) {
		Resource* r = i->second;

		//Lock whole object to give consistent view
		LockingSentry s(&r->obj_lock);

		std::string info(util::PadRight(r->name, ' ', 22));
		info.append(1, ' ');

		bool any = false;
		std::list<LockInfo>::const_iterator lki;

		info.append("Lockers: ");
		if (r->excl_info) {
			any = true;
			info.append(r->excl_info->AsString());
		}
		else {
			for (lki = r->sharer_info.begin(); lki != r->sharer_info.end(); lki++) {
				any = true;
				info.append(lki->AsString()).append(1, ' ');
			}
		}
		if (!any) info.append("none");

		any = false;
		info.append("  Waiters: ");
		for (lki = r->waiter_info.begin(); lki != r->waiter_info.end(); lki++) {
			any = true;
			info.append(lki->AsString().append(1, ' '));
		}

		if (!any) info.append("none");
		result.push_back(info);
	}

	std::sort(result.begin(), result.end());
	return result;
}

//******************************************************************************
//Debugging functions
//******************************************************************************
Resource* Resource::GetPointer(const std::string& n)
{
	//NB: always call under the protection of instances_lock
	std::map<std::string, Resource*>::const_iterator i = all_objects.find(n);
	if (i != all_objects.end())
		return i->second;
	else
		throw Exception(UTIL_RESOURCE_EXISTENCE, 
			std::string("Resource does not exist: ").append(n));
}

//******************************************************************************
//This would be called if a thread ends with locks still held - could save
//bouncing the system immediately.
//******************************************************************************
void Resource::Fix()
{
	LockingSentry sr(&obj_lock);
	if (excl_info) {
		delete excl_info;
		excl_info = NULL;
	}
	sharer_info.clear();
	waiter_info.clear();
}

//******************************************************************************
std::string Resource::Sharers()
{
	LockingSentry se(&obj_lock);
	std::string result;

	bool any = false;
	std::list<LockInfo>::const_iterator lki;
	for (lki = sharer_info.begin(); lki != sharer_info.end(); lki++) {
		if (any)
			result.append(1, '/');
			
		result.append(util::IntToString(lki->thread_id));
		any = true;
	}

	return result;
}

//******************************************************************************
ThreadID Resource::Locker()
{
	LockingSentry se(&obj_lock);
	if (excl_info)
		return excl_info->thread_id;
	else
		return 0;
}


} // close namespace
