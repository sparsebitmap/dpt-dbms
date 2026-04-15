//******************************************************************************
//The base class for many system-managed resources in Baby204.  It is based on
//the Sharable class, but has extra facilities to allow the system to 
//provide more control over the resources held by various threads. For example 
//we can get a list of users who share a particular resource and how long they
//have done so.
//******************************************************************************

#if !defined(BB_RESOURCE)
#define BB_RESOURCE

#include <map>
#include <list>
#include <vector>
#include <string>

#include "lockable.h"

namespace dpt {

class CoreServices;
class DatabaseServices;

const ThreadID DUMMY_THREAD = 0;

//******************************************************************************
//This structure uses the lower level primitives to bracket all reads or updates 
//to the other data it holds.
//It gives the following extra functionality over that lower level:
// - Threads can see what resources other threads hold or are waiting on 
// - Resources all have a name which is reported in the above
// - Resources will not let the same thread lock them twice.  This is handled
//   as an exception, since it should not happen.
// - (At some point might add a lock reason that could be reported on)
//******************************************************************************
class Resource {

	struct LockInfo;
	friend struct LockInfo; //only ever constructed in thread-safe circumstances
	struct LockInfo {
		ThreadID	thread_id;
		bool		lock_type;
		time_t		start_time;

		LockInfo(bool, bool = false);

		//This is used during list::remove().  A copy is also used, but the default 
		//member-by-member version is acceptable.
		bool operator==(const LockInfo& l);

		//For diagnostics such as MONITOR RESOURCE
		std::string AsString() const;
	};

	Lockable obj_lock;
	std::string name;

	//The following are only accessed within thread-safe functions
	LockInfo* excl_info;
	std::list<LockInfo> sharer_info; //map is overkill for vastly most common cases
	std::list<LockInfo> waiter_info; //""

	//This must be protected during the static functions manipulating it
	static std::map<std::string, Resource*> all_objects;
	static Lockable instances_lock;

//	LockInfo* GetExclInfo() const {return excl_info;}
//	std::list<LockInfo> GetSharerInfo() {return sharer_info;} //list copy - yuk
//	std::list<LockInfo> GetWaiterInfo() {return waiter_info;} //list copy - yuk
//	std::string GetName() const {return name;}

	bool Try_Unlocked(bool lock_type, bool = false);
	void Release_Unlocked(bool = false);

public:
	Resource(const std::string&);
	virtual ~Resource();

	//Try once and return false if no go
	bool Try(bool lock_type, bool = false);
	//Wait as long as necessary (or until bump - see cpp file comments and doc)
	void Get(bool lock_type, CoreServices* = NULL
#ifdef _BBDBAPI
		, DatabaseServices* = NULL //for CFRs
#endif
		, bool = false);

	//If the holder wants to change their lock it has to be done atomically
	bool TryChangeLock(bool new_lock, bool currently_held = false, bool dummy = false);

	//Note: no need for 2 release versions, as only 1 lock possible.  See
	//the base class code for more details on this.
	void Release(bool = false);

	//General enquiry
	static std::vector<std::string> Monitor();
	std::string Sharers();
	ThreadID Locker();

	//Functions mainly here for debugging.  Careful: this is not threadsafe to the
	//instances table, and the pointer itself may get deleted too.
	static Resource* GetPointer(const std::string&);
	static bool AnyDefined() {return (all_objects.size() > 0);}
	void Fix();
};

} //close namespace

#endif
