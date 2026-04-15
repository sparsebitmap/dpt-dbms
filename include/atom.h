/****************************************************************************************
The base class for all the different types of atomic database updates.
****************************************************************************************/

#if !defined(BB_ATOM)
#define BB_ATOM

namespace dpt {

class SingleDatabaseFileContext;
class AtomicBackout;

//**************************************************************************************
class AtomicUpdate {

protected:
	SingleDatabaseFileContext* context;
	AtomicBackout* backout;
	bool tbo_is_off;

	void RegisterAtomicUpdate(bool = false);

public:
	AtomicUpdate(SingleDatabaseFileContext* c) 
		: context(c), backout(NULL), tbo_is_off(false) {}
	virtual ~AtomicUpdate() {}

	virtual bool IsBackoutable() {return true;}
	virtual AtomicBackout* CreateCompensatingUpdate() = 0;

	virtual bool IsARealUpdate() {return true;}
};	

//**************************************************************************************
class AtomicUpdate_NonBackoutable : public AtomicUpdate {
	bool real;

public:
	AtomicUpdate_NonBackoutable(SingleDatabaseFileContext* c, bool r) 
		: AtomicUpdate(c), real(r) {}

	void Start() {RegisterAtomicUpdate();}
	bool IsARealUpdate() {return real;}

	bool IsBackoutable() {return false;}
	AtomicBackout* CreateCompensatingUpdate() {return NULL;}
};

} //close namespace

#endif
