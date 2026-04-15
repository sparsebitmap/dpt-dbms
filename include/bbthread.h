/****************************************************************************************
Classes for getting a piece of processing done by parallel worker threads:

- WorkerThreadTask
  The overall piece of processing which is going to be split up and have parallel 
  threads work on the parts.

- WorkerThreadSubTask
  The unit into which the overall task is broken.  This must be subclassed with
  a Perform() function.  The calling code creates as many of these as will make up the
  overall task, and adds them to it.  There can be different derived subtask types in 
  the same task.

There is no object representing a thread although the caller gets an OS thread handle
back when calling the spawn function, which might be used for various purposes.

The calling code would usually only start as many worker threads as there were CPUs 
on the machine.  This factor is not handled automatically in any way as the
calling code might as well just check it while it is checking for errors and task
completion.
****************************************************************************************/


#if !defined(BB_BBTHREAD)
#define BB_BBTHREAD

#include "lockable.h"
#include <set>
#include <queue>
#include <vector>
#include <string>

namespace dpt {
namespace util {

class WorkerThreadTask;
class WorkerThreadSubTask;

//***************************************************************************************
class WorkerThreadTask {

	std::queue<WorkerThreadSubTask*> ready_subtasks;
	std::set<WorkerThreadSubTask*> running_subtasks;
	std::vector<WorkerThreadSubTask*> complete_subtasks;
	std::vector<WorkerThreadSubTask*> error_subtasks;
	Lockable queue_lock;

	std::string nospawn_reason;
	ThreadSafeFlag interrupt_all_flag;

	//-------------------------

	static unsigned int __stdcall ThreadProc(void*);

public:
	virtual ~WorkerThreadTask();

	void AddSubTask(WorkerThreadSubTask*);
	static void CleanUp(WorkerThreadTask*);

	unsigned long SpawnThread(unsigned int* thread_id_out = NULL);
	const std::string& NospawnReason() {return nospawn_reason;}

	int NumReadySubTasks();
	int NumRunningSubTasks();

	void RequestInterruptAllSubTasks() {interrupt_all_flag.Set();}
	bool AllSubTasksInterruptRequested() {return interrupt_all_flag.Value();}

	//Zero if all is OK so far, otherwise first error encountered
	int ErrCode();	
	std::string ErrMsg();
	WorkerThreadSubTask* ErrSubTask();
};


//***************************************************************************************
class WorkerThreadSubTask {

	std::string name;
	int error_code;
	std::string error_message;

	ThreadSafeFlag interrupt_flag;

	friend class WorkerThreadTask;
	virtual void Perform() = 0;

protected:
	WorkerThreadSubTask(const std::string& n) : name(n), error_code(0) {}
	WorkerThreadTask* parent_task;

	void ThrowIfInterrupted();

public:
	virtual ~WorkerThreadSubTask() {}

	void RequestInterrupt() {interrupt_flag.Set();}
	const std::string& Name() {return name;}
};


}} //close namespace

#endif
