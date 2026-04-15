
#include "stdafx.h"

#include "lockable.h"

#include "windows.h"

//Utils
#include "winutil.h"
//Diagnostics
#include "except.h"
#include "msg_util.h"

#ifdef _DEBUG_LOCKS
#include "liostdio.h"
#include "dataconv.h"
#endif

static long one = 1;
static long zero = 0;

namespace dpt { 

//******************************************************************************
//Lockable
//******************************************************************************
#ifndef _DEBUG_LOCKS
bool Lockable::AttemptExclusive() const
{
	//Using this function as only the simpler ones like this are valid back to W95
	if (InterlockedExchange(&held_flag, one) == 0)
		return true; //was not held
	else
		return false; //was already held
}

//*********************************
void Lockable::AcquireExclusive() const
{
	while (!AttemptExclusive())
		win::Cede();
}

//*********************************
void Lockable::ReleaseExclusive()  const
{
	if (InterlockedExchange(&held_flag, zero) == 0)
		throw Exception(UTIL_LOCK_ERROR, "No exclusive lock was held");
}
#endif

//*********************************
bool Lockable::IsHeld() const
{
	//I now think this is OK so long as the updaters use interlocking functions
	//and no aliasing compiler options are used.
	if (held_flag == 0)
		return false;
	else
		return true;
}










	
//******************************************************************************
//A sharable object
//This used to be derived from the above, but isn't now
//******************************************************************************
#ifndef _DEBUG_LOCKS
bool Sharable::AttemptExclusive()  const
{
	//Anyone else got it EXCL right now?
	if (InterlockedExchange(&excl_held, one) == 1)
		return false;

	//No but what about sharers?
	if (shr_counter == 0)
		return true;

	//Yes so back off
	InterlockedExchange(&excl_held, zero);
	return false;
}

//*********************************
bool Sharable::AttemptShared() const
{
	InterlockedIncrement(&shr_counter);

	//Check for EXCL holders
	if (excl_held == 0)
		return true;

	//Oops - better back off
	InterlockedDecrement(&shr_counter);
	return false;
}

//*********************************
void Sharable::AcquireExclusive() const
{
	while (!AttemptExclusive())
		win::Cede();
}

//*********************************
void Sharable::AcquireShared()  const
{
	while (!AttemptShared())
		win::Cede();
}

//*********************************
void Sharable::ReleaseShared()  const
{
	if (shr_counter == 0)
		throw Exception(UTIL_LOCK_ERROR, "No shared lock is held");
	
	InterlockedDecrement(&shr_counter);
}

//*********************************
void Sharable::ReleaseExclusive()  const
{
	if (excl_held == 0)
		throw Exception(UTIL_LOCK_ERROR, "No exclusive lock is held");
	
	InterlockedExchange(&excl_held, zero);
}
#endif











//******************************************************************************
//Sentry objects for guaranteed release
//******************************************************************************
LockingSentry::LockingSentry(const Lockable* r, bool single_shot) 
: lockable_lockee(NULL), sharable_lockee(NULL)
{
//	TRACE("Locking sentry (L) construct\n");

	while (!r->AttemptExclusive()) {
		if (single_shot)
			return;
		
//		TRACE ("Sleeping Tid %d while trying to acquire %x (%s) \n"
//				,(int) GetCurrentThreadId(), r, r->name.c_str());
		win::Cede();
	}

	lockable_lockee = r;
}

//*********************************
LockingSentry::LockingSentry(const Sharable* r, bool single_shot) 
: lockable_lockee(NULL), sharable_lockee(NULL)
{
//	TRACE("Locking sentry (S) construct\n");

	while (!r->AttemptExclusive()) {
		if (single_shot)
			return;

//		TRACE ("Sleeping Tid %d while trying to acquire %x (%s) \n"
//				,(int) GetCurrentThreadId(), r, r->name.c_str());
		win::Cede();
	}

	sharable_lockee = r;
}

//*********************************
LockingSentry::~LockingSentry() 
{
	if (lockable_lockee) {
//		TRACE("Locking sentry (L) destruct\n");
		lockable_lockee->ReleaseExclusive();
		lockable_lockee = NULL;
	}
	if (sharable_lockee) {
//		TRACE("Locking sentry (S) destruct\n");
		sharable_lockee->ReleaseExclusive();
		sharable_lockee = NULL;
	}
}

//*********************************
bool LockingSentry::HasALock() 
{
	if (lockable_lockee || sharable_lockee)
		return true;
	else
		return false;
}

//******************************************************************************
void LockingSentry::Try(const Lockable* r) 
{
	if (HasALock())
		throw Exception(UTIL_LOCK_ERROR, "Sentry lock already held");

	if (!r->AttemptExclusive())
		return;

	lockable_lockee = r;
}

//*********************************
/*void LockingSentry::GiveLock(const Sharable* s)
{
	if (HasALock())
		throw Exception(UTIL_LOCK_ERROR, "Sentry already holds a lock");

	sharable_lockee = s;
}
*/

//*********************************
SharingSentry::SharingSentry(Sharable* r) : sharee(r)
{
	sharee->AcquireShared();
}

//*********************************
SharingSentry::~SharingSentry() 
{
	sharee->ReleaseShared();
}











//******************************************************************************
//Flag object
//Used to use a Lockable to control access to a simple bool.
//******************************************************************************
bool ThreadSafeFlag::Set() 
{
	LONG prev = InterlockedExchange(&flag, one);
	return (prev == one) ? true : false;
}

//*********************************
bool ThreadSafeFlag::Reset() 
{
	LONG prev = InterlockedExchange(&flag, zero);
	return (prev == one) ? true : false;
}

//*********************************
bool ThreadSafeFlag::Value() const
{
	//See earlier comment 
	if (flag == 0)
		return false;
	else
		return true;
}











//******************************************************************************
//Integer object
//******************************************************************************
long ThreadSafeLong::Inc() 
{
	return InterlockedIncrement(&value);
}

//*********************************
long ThreadSafeLong::Dec() 
{
	return InterlockedDecrement(&value);
}

//*********************************
long ThreadSafeLong::Add(long i) 
{
	return InterlockedExchangeAdd(&value, i);
}

//*********************************
long ThreadSafeLong::Set(long v) 
{
	return InterlockedExchange(&value, v);
}











//******************************************************************************
//This is mainly used for system stats
//******************************************************************************
_int64 ThreadSafeI64::Inc() 
{
	LockingSentry ls(&lock);
	value++;
	return value;
}

//*********************************
_int64 ThreadSafeI64::Dec() 
{
	LockingSentry ls(&lock);
	value--;
	return value;
}

//*********************************
_int64 ThreadSafeI64::Set(_int64 v) 
{
	LockingSentry ls(&lock);

	//Note for compatibility with the long version above, return prev value
	_int64 prev = value;
	value = v;
	return prev;
}

//*********************************
_int64 ThreadSafeI64::SetHWM(_int64 v) 
{
	LockingSentry ls(&lock);

	if (v > value)
		value = v;

	//In this case return the new high water mark
	return value;
}

//*********************************
_int64 ThreadSafeI64::Add(_int64 v) 
{
	LockingSentry ls(&lock);

	value += v;
	return value;
}

//******************************************************************************
_int64 ThreadSafeI64::Value() const
{
	LockingSentry ls(&lock);
	return value;
}










//******************************************************************************
//Added in V3.0 for fast load.  Perhaps a generally useful object though.
//******************************************************************************
double ThreadSafeDouble::Inc() 
{
	LockingSentry ls(&lock);
	value++;
	return value;
}

//*********************************
double ThreadSafeDouble::Dec() 
{
	LockingSentry ls(&lock);
	value--;
	return value;
}

//*********************************
double ThreadSafeDouble::Set(double v) 
{
	LockingSentry ls(&lock);

	//Note for compatibility with the long version above, return prev value
	double prev = value;
	value = v;
	return prev;
}

//*********************************
double ThreadSafeDouble::Add(double v) 
{
	LockingSentry ls(&lock);

	value += v;
	return value;
}

//******************************************************************************
double ThreadSafeDouble::Value() const
{
	LockingSentry ls(&lock);
	return value;
}










//******************************************************************************
//String object
//This is how the integer and flag above used to work before i switched to
//InterlockedIncrement etc.
//******************************************************************************
ThreadSafeString::ThreadSafeString(const std::string& v) 
{
	value = std::string(v.c_str(), v.length()); 
}

//*********************************
std::string ThreadSafeString::Value() const
{
	LockingSentry s(&lock); 

	//If we're not careful the STL will try to use the same underlying instance, 
	//which is what we're trying to avoid here.  Build a new one from the data.
	//In fact on closer inspection of the STL it appears to lock itself anyway,
	//so this class is really overkill with its current implementation using an
	//STL string.  Could easily switch to a char[].
	return std::string(value.c_str(), value.length());
}

//*********************************
void ThreadSafeString::Set(const std::string& v) 
{
	LockingSentry s(&lock);

	//Same comment as above
	value = std::string(v.c_str(), v.length()); 
}

//*********************************
//Separate for efficiency of the vanilla Set()
std::string ThreadSafeString::SetAndShowPrev(const std::string& v) 
{
	LockingSentry s(&lock);

	std::string prev = value;

	//Same comment as above
	value = std::string(v.c_str(), v.length());

	return prev;
}





//******************************************************************************
/*
#pragma optimize("a", off)

* * * This is untested - not even sure how I'd go about testing it either!

int SureRead(int* addr)
{
	int attempt = *addr;
	int confirm;

	//Call till we get a stable value in 2 consecutive time slices
	for (;;) {
		win::Cede();
		confirm = *addr;
		if (confirm == attempt)
			break;
		else
			attempt = confirm;
	}

	return confirm;
}
*/












//******************************************************************************
//Diagnostics
//******************************************************************************
#ifdef _DEBUG_LOCKS

util::LineOutput* Lockable::log = 
	new util::StdIOLineOutput("locklogs/lockable.txt", util::STDIO_CCLR, 5); 
Lockable Lockable::loglock;

//*******************************
void Lockable::WriteLogLine(const std::string& line)
{
	if (!log)
		return;

	while (!loglock.AttemptExclusiveNaked())
		win::Cede();

	static std::string lastline;
	if (line != lastline) {
		log->WriteLine(line);
		lastline = line;
	}
	else {
		log->WriteLine("dupeline");
	}

	loglock.ReleaseExclusiveNaked();
}

//*******************************
Lockable::Lockable(const std::string& n) 
: name(n), held_flag(0) 
{
	WriteLogLine(
		std::string("OBJECT,")
		.append(util::IntToString((int)this))
		.append(1,',').append(n));
};

//*******************************
Lockable::~Lockable()
{
	if (name.length() == 0)
		return;

	WriteLogLine(
		std::string("-OBJX-,")
		.append(util::IntToString((int)this))
		.append(1,',').append(name));
}

//*******************************
void Lockable::CreateNewThreadLog(const std::string& n)
{
	WriteLogLine(
		std::string("THREAD,")
		.append(util::IntToString(GetCurrentThreadId()))
		.append(1,',').append(n));
}




//******************************************************************************
void Lockable::AcquireExclusive() const
{
	std::string info;
	if (name.length() != 0) {
		info = std::string(util::IntToString((int)this))
			.append(1, ',').append(name)
			.append(1, ',').append(util::IntToString(GetCurrentThreadId()));

		WriteLogLine(std::string("Lk GET,").append(info));
	}

	while (!AttemptExclusiveNaked()) {
//		TRACE ("Sleeping while trying to acquire Lockable %x on thread %d\n", 
//			this, (int) GetCurrentThreadId());
		Sleep(500);
		win::Cede();
	}

	if (name.length() != 0)
		WriteLogLine(std::string("--LgOK,").append(info));
}

//*******************************
bool Lockable::AttemptExclusiveNaked() const
{
	if (InterlockedExchange(&held_flag, one) == 0)
		return true;
	else
		return false;
}

//*******************************
bool Lockable::AttemptExclusive() const
{
	std::string info;
	if (name.length() != 0) {
		info = std::string(util::IntToString((int)this))
			.append(1, ',').append(name)
			.append(1, ',').append(util::IntToString(GetCurrentThreadId()));

		WriteLogLine(std::string("Lk TRY,").append(info));
	}

	if (InterlockedExchange(&held_flag, one) == 0) {
//		TRACE("Got it (%x)\n", this);
		if (name.length() != 0)
			WriteLogLine(std::string("--LtOK,").append(info));

		return true; //was not held
	}
	else {
//		TRACE("It was already held (%x)\n", this);
		if (name.length() != 0) 
			WriteLogLine(std::string("--LTXX,").append(info));
		return false; //was already held
	}
}

//*********************************
void Lockable::ReleaseExclusive()  const
{
	std::string info;
	if (name.length() != 0) {
		info = std::string(util::IntToString((int)this))
			.append(1, ',').append(name)
			.append(1, ',').append(util::IntToString(GetCurrentThreadId()));

		WriteLogLine(std::string("Lk REL,").append(info));
	}

	if (InterlockedExchange(&held_flag, zero) == 0)
		throw Exception(UTIL_LOCK_ERROR, "No exclusive lock was held");
}

//*********************************
void Lockable::ReleaseExclusiveNaked()  const
{
	//Just for the log file.
	if (InterlockedExchange(&held_flag, zero) == 0)
		throw Exception(UTIL_LOCK_ERROR, "No exclusive lock was held");
}







//******************************************************************************
util::LineOutput* Sharable::log =
	new util::StdIOLineOutput("locklogs/sharable.txt", util::STDIO_CCLR, 5); 
Lockable Sharable::loglock;

//*******************************
void Sharable::WriteLogLine(const std::string& line)
{
	if (!log)
		return;

	while (!loglock.AttemptExclusiveNaked())
		win::Cede();

	static std::string lastline;
	if (line != lastline) {
		log->WriteLine(line);
		lastline = line;
	}
	else {
		log->WriteLine("dupeline");
	}

	loglock.ReleaseExclusiveNaked();
}

//*******************************
Sharable::Sharable(const std::string& n) 
: name(n), shr_counter(0), excl_held(0) 
{
	WriteLogLine(
		std::string("OBJECT,")
		.append(util::IntToString((int)this))
		.append(1,',').append(n));
}

//*******************************
Sharable::~Sharable()
{
	if (name.length() == 0)
		return;

	WriteLogLine(
		std::string("-OBJX-,")
		.append(util::IntToString((int)this))
		.append(1,',').append(name));
}

//*******************************
void Sharable::CreateNewThreadLog(const std::string& n)
{
	WriteLogLine(
		std::string("THREAD,")
		.append(util::IntToString(GetCurrentThreadId()))
		.append(1,',').append(n));
}





//******************************************************************************
bool Sharable::AttemptExclusive()  const
{
	std::string info;
	if (name.length() != 0) {
		info = std::string(util::IntToString((int)this))
			.append(1, ',').append(name)
			.append(1, ',').append(util::IntToString(GetCurrentThreadId()));

		WriteLogLine(std::string("Sh TEX,").append(info));
	}

	//Anyone else got it EXCL right now?
	if (InterlockedExchange(&excl_held, one) == 1) {
		if (name.length() != 0)
			WriteLogLine(std::string("--STeX,").append(info));
		return false;
	}

	//No but what about sharers?
	if (shr_counter == 0) {
		if (name.length() != 0)
			WriteLogLine(std::string("-STeOk,").append(info));
		return true;
	}

	//Yes so back off
	InterlockedExchange(&excl_held, zero);
	if (name.length() != 0)
		WriteLogLine(std::string("--STeX,").append(info));
	return false;
}

//*********************************
bool Sharable::AttemptExclusiveNaked()  const
{
	if (InterlockedExchange(&excl_held, one) == 1)
		return false;
	if (shr_counter == 0)
		return true;
	InterlockedExchange(&excl_held, zero);
	return false;
}

//*********************************
bool Sharable::AttemptShared() const
{
	std::string info;
	if (name.length() != 0) {
		info = std::string(util::IntToString((int)this))
			.append(1, ',').append(name)
			.append(1, ',').append(util::IntToString(GetCurrentThreadId()));

		WriteLogLine(std::string("Sh TSH,").append(info));
	}

	InterlockedIncrement(&shr_counter);

	//Check for EXCL holders
	if (excl_held == 0) {
		if (name.length() != 0)
			WriteLogLine(std::string("-STsOk,").append(info));
		return true;
	}

	//Oops - better back off
	InterlockedDecrement(&shr_counter);
	if (name.length() != 0)
		WriteLogLine(std::string("--STsX,").append(info));
	return false;
}

//*********************************
bool Sharable::AttemptSharedNaked() const
{
	InterlockedIncrement(&shr_counter);
	if (excl_held == 0)
		return true;
	InterlockedDecrement(&shr_counter);
	return false;
}

//*********************************
void Sharable::AcquireExclusive() const
{
	std::string info;
	if (name.length() != 0) {
		info = std::string(util::IntToString((int)this))
			.append(1, ',').append(name)
			.append(1, ',').append(util::IntToString(GetCurrentThreadId()));

		WriteLogLine(std::string("Sh GEX,").append(info));
	}

	while (!AttemptExclusiveNaked()) {
//		TRACE ("Sleeping while trying to acquire Sharable %x EXCL on thread %d\n", 
//			this, (int) GetCurrentThreadId());
		Sleep(500);
		win::Cede();
	}

	if (name.length() != 0)
		WriteLogLine(std::string("-SGeOk,").append(info));
}

//*********************************
void Sharable::AcquireShared()  const
{
	std::string info;
	if (name.length() != 0) {
		info = std::string(util::IntToString((int)this))
			.append(1, ',').append(name)
			.append(1, ',').append(util::IntToString(GetCurrentThreadId()));

		WriteLogLine(std::string("Sh GSH,").append(info));
	}

	while (!AttemptShared()) {
//		TRACE ("Sleeping while trying to acquire Sharable %x SHR on thread %d\n", 
//			this, (int) GetCurrentThreadId());
		Sleep(500);
		win::Cede();
	}

	if (name.length() != 0)
		WriteLogLine(std::string("-SGsOk,").append(info));
}

//*********************************
void Sharable::ReleaseShared()  const
{
	std::string info;
	if (name.length() != 0) {
		info = std::string(util::IntToString((int)this))
			.append(1, ',').append(name)
			.append(1, ',').append(util::IntToString(GetCurrentThreadId()));

		WriteLogLine(std::string("Sh RSH,").append(info));
	}

	if (shr_counter == 0)
		throw Exception(UTIL_LOCK_ERROR, "No shared lock is held");
	
	InterlockedDecrement(&shr_counter);
}

//*********************************
void Sharable::ReleaseExclusive()  const
{
	std::string info;
	if (name.length() != 0) {
		info = std::string(util::IntToString((int)this))
			.append(1, ',').append(name)
			.append(1, ',').append(util::IntToString(GetCurrentThreadId()));

		WriteLogLine(std::string("Sh REX,").append(info));
	}

	if (excl_held == 0)
		throw Exception(UTIL_LOCK_ERROR, "No exclusive lock is held");
	
	InterlockedExchange(&excl_held, zero);
}

#endif



} //close namespace
