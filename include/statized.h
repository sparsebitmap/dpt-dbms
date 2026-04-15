//****************************************************************************************
// Base class for all objects using M204-style stats.
// The main stat viewing/manipulating class is StatViewer.
//****************************************************************************************

#if !defined(BB_STATIZED)
#define BB_STATIZED

#include <string>
#include "const_stat.h"
#include "lockable.h"

namespace dpt {

class StatViewer;

//**********************************
class Statisticized {

	friend class StatViewer;
	virtual _int64 ViewStat(const std::string& statname, StatLevel) const = 0;
	virtual void ClearSLStats() = 0;

protected:

	static void StatWrongObject(const std::string& stat);

	void RegisterStat(const std::string&, StatViewer*);
	void RegisterHolder(StatViewer*);

public:
	virtual ~Statisticized() {};
};

//**********************************
class SharedStat64 {
	ThreadSafeI64* value;

	friend class MultiStat;
	SharedStat64(ThreadSafeI64* v = NULL) : value(v) {}

	void Inc() {if (!value) return; value->Inc();}
	void SetHWM(_int64 i) {if (!value) return; value->SetHWM(i);}
	void Add(_int64 i) {if (!value) return; value->Add(i);}
	void Clear() {if (!value) return; value->Set(0);}

	_int64 Value() const {if (!value) return 0; return value->Value();}
};

//**********************************
class Stat64 {
	_int64 value;

	friend class MultiStat;
	Stat64() : value(0) {}

	void Inc() {value++;}
	void SetHWM(_int64 i) {if (i > value) value = i;}
	void Add(_int64 i) {value += i;}
	void Clear() {value = 0;}

	_int64 Value() const {return value;}
};

//**********************************
class MultiStat {
	SharedStat64 sys;
	Stat64 user;
	Stat64 sl;

public:
	MultiStat(ThreadSafeI64* s = NULL) : sys(s) {}
	void SetSys(ThreadSafeI64* s) {sys.value = s;}

	void Inc() {sys.Inc(); user.Inc(); sl.Inc();}
	void SetHWM(_int64 i) {sys.SetHWM(i); user.SetHWM(i); sl.SetHWM(i);}	
	void Add(_int64 i) {sys.Add(i); user.Add(i); sl.Add(i);}
	void ClearSL() {sl.Clear();}

	_int64 Value(StatLevel) const;
};

//**********************************
//V2.07 - Sep 07.  File stats are sometimes updated outside any other lock, so make them
//intrinsically threadsafe.  Was not a serious error but this guarantees accuracy at least.
//V2.08.  Restructured to stop file stats always being zero (oops!).
class FileStat {
	ThreadSafeI64 value;

public:
	FileStat() : value(0) {}

	void Inc() {value.Inc();}
	void SetHWM(_int64 i) {value.SetHWM(i);}
	void Add(_int64 i) {value.Add(i);}
	void Clear() {value.Set(0);}

	_int64 Value() const {return value.Value();}
};

}	//close namespace

#endif
