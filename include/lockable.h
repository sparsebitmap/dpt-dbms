//******************************************************************************
// Low level objects used to ensure thread safety.
// NB.  
// May be recoded in future to explicitly use WinAPI Critical sections which
// work the same way but are better documented.
//******************************************************************************

#if !defined(BB_LOCKABLE)
#define BB_LOCKABLE

#include <string>

namespace dpt {

typedef unsigned int ThreadID;

const bool BOOL_EXCL = true;
const bool BOOL_SHR	= false;

#ifdef _DEBUG_LOCKS
namespace util {class LineOutput;}
#endif

//********************************
class Lockable {
	mutable long held_flag;

public:
	Lockable() : held_flag(0) {};

	bool AttemptExclusive() const; //no wait
	void AcquireExclusive() const; //wait
	void ReleaseExclusive() const; //throws if not held

	bool IsHeld() const;

#ifdef _DEBUG_LOCKS
	std::string name;
	Lockable(const std::string&);
	~Lockable();

	bool AttemptExclusiveNaked() const;
	void ReleaseExclusiveNaked() const;

	static util::LineOutput* log;
	static Lockable loglock;
	static void WriteLogLine(const std::string&);

	static void CreateNewThreadLog(const std::string&);
#endif
};

//********************************
class Sharable {
	mutable long excl_held;
	mutable long shr_counter;

public:
	Sharable() : excl_held(0), shr_counter(0) {}

	bool AttemptExclusive() const;
	void AcquireExclusive() const;
	void ReleaseExclusive() const;

	bool AttemptShared() const;
	void AcquireShared() const;
	void ReleaseShared() const;

#ifdef _DEBUG_LOCKS
	std::string name;
	Sharable(const std::string&);
	~Sharable();

	bool AttemptExclusiveNaked() const;
	bool AttemptSharedNaked() const;

	static util::LineOutput* log;
	static Lockable loglock;
	static void WriteLogLine(const std::string&);

	static void CreateNewThreadLog(const std::string&);
#endif
};





//******************************************************************************
//Utility classes
//******************************************************************************
class LockingSentry {
	const Lockable* lockable_lockee;
	const Sharable* sharable_lockee;

public:
	LockingSentry(const Sharable*, bool = false);
	LockingSentry(const Lockable*, bool = false);
	~LockingSentry();

	void Try(const Lockable*); //V2.14 Jan 09.

	bool HasALock();
};

//*************************************
class SharingSentry {
	Sharable* sharee;

public:
	SharingSentry(Sharable*);
	~SharingSentry();
};




//******************************************************************************
class ThreadSafeFlag {
	long flag;
public:
	ThreadSafeFlag(bool b = false) : flag(b ? 1 : 0) {}
	bool Set();
	bool Reset();
	bool Value() const;
};


//*************************************
class ThreadSafeLong {
	long value;

public:
	ThreadSafeLong(long v = 0) : value(v) {}

	long Inc();
	long Dec();
	long Add(long);
	long Set(long);
	long Value() const {return value;}
};

//*************************************
class ThreadSafeI64 {
	Lockable lock;
	volatile _int64 value;

public:
	ThreadSafeI64(_int64 v = 0) : value(v) {}

	_int64 Inc();
	_int64 Dec();
	_int64 Value() const;
	_int64 Set(_int64);
	_int64 SetHWM(_int64); //a few stats like DKSKIP
	_int64 Add(_int64); //a few stats like UPDTTIME
};

//*************************************
class ThreadSafeDouble {
	Lockable lock;
	volatile double value;

public:
	ThreadSafeDouble(double v = 0) : value(v) {}

	double Inc();
	double Dec();
	double Value() const;
	double Set(double);
	double Add(double);
};

//*************************************
class ThreadSafeString {
	Lockable lock;
	std::string value;

public:
	ThreadSafeString(const std::string& = std::string());
	std::string Value() const;
	void Set(const std::string&);
	std::string SetAndShowPrev(const std::string&);
};

//*************************************
//int SureRead(int*);

} //close namespace

#endif
