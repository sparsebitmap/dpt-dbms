//******************************************************************************
//The STL stack is derived from its deque and I don't like it for a couple of
//reasons:
//1. top() has an incredible path length and is one of the commonest functions.
//2. the memory is freed every time the stack becomes empty, which is an 
//   undesirable overhead if you have a stack which often has a single element
//   pushed and popped.
//******************************************************************************

#if !defined(BB_STACK)
#define BB_STACK

#include "except.h"
#include "msg_util.h"

namespace dpt { namespace util {

template<class _CLS_> class Stack {
	_CLS_* data;
	int capacity;
	int min_capacity;
	int top;

	void Deldata() {
		if (data) 
			delete[] data; 
		data = NULL;}

	void CopyData(_CLS_* to) const {
		for (int x = 0; x <= top; x++)
			to[x] = data[x];}

	void MakeCopy(const Stack& rhs) {
		Deldata();
		if (!rhs.data)
			return;
		capacity=rhs.capacity; min_capacity=rhs.min_capacity; top=rhs.top;
		data = new _CLS_[capacity];
		rhs.CopyData(data);}

	void Rsv(int newcapacity) {
		_CLS_* temp = NULL;
		try {
			//_CLS_* temp = new _CLS_[newcapacity]; //V3.0
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

	void PrePush() {
		if (top == capacity - 1) {
			int newcapacity = (capacity == 0) ? 1 : capacity * 2;
			Reserve(newcapacity);}
		top++;
	}

	void Pop_S(_CLS_* dest) {
		if (IsEmpty())
			throw Exception(UTIL_STACK_ERROR, "Bug: Empty BB stack (pop)");
		if (dest)
			*dest = data[top];
		data[top] = _CLS_();
		top--;
		//Unlike STL stack we never go below the basic allocation
		if (capacity > min_capacity) {
			if (top == (min_capacity / 2 - 1) )
				Rsv(min_capacity);}}

public:
	//With large objects of if space is more important than speed, use a low i
	explicit Stack(int i) : data(NULL), capacity(0), top(-1), min_capacity(i) {
		Reserve(i);}

	Stack(const Stack& s) : data(NULL) {
		MakeCopy(s);}

	void operator=(const Stack& s) {
		MakeCopy(s);}

	~Stack() {
		Deldata();}

	void Reserve(int newcapacity) {
		if (newcapacity <= capacity)
			return;
		Rsv(newcapacity);}

	//Non-const version in case templatized class has modifying copy operator
	void Push(_CLS_& newtop) {
		PrePush();
		data[top] = newtop;}

	void Push(const _CLS_& newtop) {
		PrePush();
		data[top] = newtop;}

	void Pop(_CLS_& dest) {Pop_S(&dest);}
	void Pop() {Pop_S(NULL);}

	_CLS_& Top() {
		if (IsEmpty())
			throw Exception(UTIL_STACK_ERROR, "Bug: Empty BB stack (top)");
		return data[top];}

	const _CLS_& Peek(int i) {
		if (i > top || i < 0)
			throw Exception(UTIL_STACK_ERROR, "Bug: Invalid BB stack position (peek)");
		return data[i];}

	int Size() {
		return top + 1;}

	bool IsEmpty() {
		return Size() == 0;}

	void Clear() {
		while (!IsEmpty())
			Pop();}
};

}} //close namespace

#endif
