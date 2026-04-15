
#include "stdafx.h"

#include "garbage.h"

//Utils
#ifdef _DEBUG_LOCKS
#include "dataconv.h"
#endif
//Diagnostics
#include "except.h"
#include "msg_core.h"

namespace dpt {

Destroyer GlobalDestroyer::global_destroyer;

//**************************************
Destroyer::Destroyer()
{
#ifdef _DEBUG_LOCKS
	lock.name = std::string("Destroyer: ").append(util::IntToString((int)this));
#endif
}

//**************************************
Destroyer::~Destroyer()
{
	LockingSentry s(&lock);

	std::map<Destroyable*, std::string>::iterator i;
	for (i = children.begin(); i != children.end(); i++)
		Destroyable::Destroy(i->first);
}

//**************************************
void Destroyer::Register(Destroyable* c, bool throwit, const std::string& name)
{
	LockingSentry s(&lock);

	std::pair<Destroyable*, std::string> p(c, name);
	std::pair<std::map<Destroyable*, std::string>::iterator, bool> ins = children.insert(p);
	if (!ins.second && throwit)
		throw Exception(GARBAGE_ERROR, 
			"Bug: destroyable object is already registered");
}

//**************************************
void Destroyer::UnRegister(Destroyable* c, bool throwit)
{
	LockingSentry s(&lock);

	if (children.erase(c) == 0 && throwit)
		throw Exception(GARBAGE_ERROR, 
			"Bug: destroyable object is not registered");
}

//**************************************
bool Destroyer::Owns(Destroyable* d)
{
	LockingSentry s(&lock);

	return (children.find(d) != children.end());
}

//**************************************
//This is only used in the GETG functions and was an afterthought.  Not critical.
Destroyable* Destroyer::ScanForName(const std::string& name)
{
	LockingSentry s(&lock);

	std::map<Destroyable*, std::string>::iterator i;
	for (i = children.begin(); i != children.end(); i++) {
		if (i->second == name)
			return i->first;
	}

	return NULL;
}

//**************************************************************************************
//char* Destroyable::valstring = "Destroy!"; //V2.24. non-const deprecated in gcc now.
const char* Destroyable::valstring = "Destroy!";

Destroyable::Destroyable(void (*i) (Destroyable*), Destroyer* p)
: parent(p), indirect(i)
{
	//A null parent is OK - it means the creating code plans to delete the object
	if (parent)
		parent->Register(this, true);

	SetValBuff(valstring);
}

//**************************************************************************************
void Destroyable::Register(Destroyer* d, bool throwit, const std::string& name)
{
	if (parent)
		parent->UnRegister(this, throwit);

	parent = d;

	if (parent)
		parent->Register(this, throwit, name);
}

} //close namespace


