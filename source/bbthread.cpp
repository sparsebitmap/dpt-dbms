
#include "stdafx.h"
#include "bbthread.h"

#include <process.h>

#include "winutil.h"
#include "except.h"
#include "msg_util.h"

namespace dpt { namespace util {

//*******************************************************************************
WorkerThreadTask::~WorkerThreadTask() 
{
	LockingSentry s(&queue_lock);

	while (!ready_subtasks.empty()) {
		delete ready_subtasks.front();
		ready_subtasks.pop();
	}

	while (!running_subtasks.empty()) {
		delete *(running_subtasks.begin());
		running_subtasks.erase(running_subtasks.begin());
	}

	for (size_t c = 0; c < complete_subtasks.size(); c++)
		delete complete_subtasks[c];
	complete_subtasks.clear();

	for (size_t e = 0; e < error_subtasks.size(); e++)
		delete error_subtasks[e];
	error_subtasks.clear();

}

//*******************************************************************************
void WorkerThreadTask::CleanUp(WorkerThreadTask* task) 
{
	//Caller should check this first
	if (task->NumRunningSubTasks())
		throw Exception(UTIL_THREAD_ERROR, "Worker thread subtasks are still running");

	delete task;
}

//*******************************************************************************
void WorkerThreadTask::AddSubTask(WorkerThreadSubTask* st)
{
	LockingSentry s(&queue_lock);
	ready_subtasks.push(st);

	st->parent_task = this;
}

//***************************************
int WorkerThreadTask::NumReadySubTasks() 
{
	LockingSentry s(&queue_lock);
	return ready_subtasks.size();
}

//***************************************
int WorkerThreadTask::NumRunningSubTasks() 
{
	LockingSentry s(&queue_lock);
	return running_subtasks.size();
}

//***************************************
int WorkerThreadTask::ErrCode() 
{
	LockingSentry s(&queue_lock);

	if (error_subtasks.size() == 0) 
		return 0; 

	return error_subtasks[0]->error_code;
}

//***************************************
std::string WorkerThreadTask::ErrMsg() 
{
	LockingSentry s(&queue_lock);

	if (error_subtasks.size() == 0) 
		return std::string(); 

	return error_subtasks[0]->error_message;
}	

//***************************************
WorkerThreadSubTask* WorkerThreadTask::ErrSubTask() 
{
	LockingSentry s(&queue_lock);

	if (error_subtasks.size() == 0) 
		return NULL; 

	return error_subtasks[0];
}	


//*******************************************************************************
unsigned long WorkerThreadTask::SpawnThread(unsigned int* thread_id_out) 
{
	LockingSentry s(&queue_lock);

	if (ready_subtasks.size() == 0) {
		nospawn_reason = "No subtasks to perform";
		return 0;
	}

	WorkerThreadSubTask* subtask = ready_subtasks.front();

	unsigned int thread_id;
	unsigned long thread_handle = _beginthreadex(NULL, 0, ThreadProc, subtask, 0, &thread_id);
	if (thread_id_out)
		*thread_id_out = thread_id;

	if (thread_handle == 0) {
		nospawn_reason = win::GetLastErrorMessage(true);
//		TRACE("%s \n", nospawn_reason.c_str());
		return 0;
	}

//	TRACE("Just spawned thread with thread ID %d, handle %d \n", thread_id, thread_handle);

	ready_subtasks.pop();
	running_subtasks.insert(subtask);

	return thread_handle;
}

//*******************************************************************************
unsigned int __stdcall WorkerThreadTask::ThreadProc(void* vst)
{
	WorkerThreadSubTask* subtask = static_cast<WorkerThreadSubTask*>(vst);

	try {
		subtask->Perform();
	}
	catch (Exception& e) {
		subtask->error_code = e.Code();
		subtask->error_message = e.What();
	}
	catch (...) {
		subtask->error_code = UTIL_THREAD_ERROR;
		subtask->error_message = "Unknown thread exception";
	}

	WorkerThreadTask* t = subtask->parent_task;

	LockingSentry s(&t->queue_lock);

	t->running_subtasks.erase(subtask);

	if (subtask->error_code == 0 && subtask->error_message.length() == 0)
		t->complete_subtasks.push_back(subtask);
	else
		t->error_subtasks.push_back(subtask);

	return subtask->error_code;
}






//*******************************************************************************
//*******************************************************************************
void WorkerThreadSubTask::ThrowIfInterrupted()
{
	if (interrupt_flag.Value() || parent_task->AllSubTasksInterruptRequested())
		throw Exception(UTIL_THREAD_ERROR, "Interrupted");
}


}} //close namespace
