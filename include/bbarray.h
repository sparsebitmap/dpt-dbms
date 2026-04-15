//******************************************************************************
//Just an abstraction of a C style array that automatically frees its memory.
//Cribbed and cut down from bbstack.
//******************************************************************************

#if !defined(BB_ARRAY)
#define BB_ARRAY

#include "except.h"
#include "msg_util.h"

namespace dpt { namespace util {

template<class _CLS_> class Array {
	_CLS_* data;
	int capacity;

	void Deldata() {
		if (data) 
			delete[] data; 
		data = NULL;}

	void CopyData(_CLS_* to) const {
		for (int x = 0; x < capacity; x++)
			to[x] = data[x];}

	void MakeCopy(const Array& rhs) {
		Deldata();
		if (!rhs.data)
			return;
		capacity=rhs.capacity;
		data = new _CLS_[capacity];
		rhs.CopyData(data);}

	void Rsv(int newcapacity) {
		_CLS_* temp = NULL;
		try {
			temp = new _CLS_[newcapacity];
			CopyData(temp);
			if (data)
				delete[] data;
			data = temp;
			capacity = newcapacity;
		} catch (...) {
			if (temp)
				delete[] temp;
			throw;}}

	void ChkEntry(int e) {
		if (e < 0 || e >= capacity)
			throw Exception(UTIL_ARRAY_ERROR, "Bug: Requesting element outside array");}

	_CLS_& EntryUnchecked(int e) {
		return data[e];}
	_CLS_* PEntryUnchecked(int e) {
		return data + e;}

public:
	explicit Array(int i) : data(NULL), capacity(0) {
		Reserve(i);}

	explicit Array(int i, const _CLS_& d) : data(NULL), capacity(0) {
		Reserve(i);
		for (int x = 0; x < i; x++) 
			data[x] = d;}

	Array(const Array& a) : data(NULL) {
		MakeCopy(a);}

	void operator=(const Array& a) {
		MakeCopy(a);}

	~Array() {
		Deldata();}

	void Reserve(int newcapacity) {
		if (newcapacity <= capacity)
			return;
		Rsv(newcapacity);}

	//*****************************

	_CLS_* Data() {
		return data;}

	//Checked access
	_CLS_& Entry(int e) {
		ChkEntry(e);
		return EntryUnchecked(e);}

	_CLS_* PEntry(int e) {
		ChkEntry(e);
		return PEntryUnchecked(e);}

	//Unchecked access, as if it were a regular array
	_CLS_& operator [](int e) {
		return EntryUnchecked(e);}
};

}} //close namespace

#endif
