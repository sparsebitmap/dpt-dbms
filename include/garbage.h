//******************************************************************************************
// Heap object garbage collector facility.  
// Used by some standard $functions like $LSTPROC and also available for use by API code.
//******************************************************************************************

#if !defined BB_GARBAGE
#define BB_GARBAGE

#include <map>
#include "lockable.h"

namespace dpt {

class Destroyable;
class CoreServices;



//*****************************************************
// Destroys all managed objects during its destructor.
class Destroyer {
	//V2 - Dec 06. Associate optional name.
	Lockable lock; 
	std::map<Destroyable*, std::string> children;

	friend class Destroyable;
	void Register(Destroyable*, bool, const std::string& = std::string());
	void UnRegister(Destroyable*, bool);

public:
	Destroyer();
	virtual ~Destroyer();

	bool Owns(Destroyable*);
	Destroyable* ScanForName(const std::string&); //see comments in cpp
};


//*****************************************************
class GlobalDestroyer : public Destroyer {
	friend class CoreServices;
	static Destroyer global_destroyer;
};



//**************************************************************************************
// Garbage base class managed by the above.
// Derived classes need either:
//	A. a usable destructor
//	B. an accessible function of the form "static void f(Destroyable*)" that can be
//		called to cleanly destroy them (see LSTPROC cursors for example)
//**************************************************************************************
class Destroyable {
	Destroyer* parent;

	void (*indirect) (Destroyable*);

	friend class Destroyer;
	static void Destroy(Destroyable* d) {
		if (d->indirect)
			d->indirect(d);		//Method B
		else
			delete d;}			//Method A

	static const char* valstring;
	char valbuff[8];
	void SetValBuff(const char* c) {memcpy(valbuff, c, 8);}

protected:
	Destroyable(void (*) (Destroyable*) = NULL, Destroyer* = NULL); 
	virtual ~Destroyable() {SetValBuff("Invalid!");} //Avoid possible later havoc.

public:
	//If not supplied on constructor, or to reassign ownership
	void Register(Destroyer* d, bool throwit = true, const std::string& = std::string());

	//Use if calling code finishes cleanly and can destroy the object itself
	void UnRegister(bool throwit = true) {Register(NULL, throwit);}

	//Can be handy to move them around between owners and/or threads
	Destroyer* Parent() {return parent;}
	bool ValidObject() {return (memcmp(valbuff, valstring, 8) == 0);}
};

}	//close namespace

#endif

