/****************************************************************************************
Backout objects for atomic updates.  Usually a 1-for-1 correspondence but not always.
****************************************************************************************/

#if !defined(BB_ATOMBACK)
#define BB_ATOMBACK

namespace dpt {

class SingleDatabaseFileContext;

//**************************************************************************************
class AtomicBackout {
	bool activated;

protected:
	SingleDatabaseFileContext* context;

public:
	AtomicBackout(SingleDatabaseFileContext* c) : activated(false), context(c) {}
	virtual ~AtomicBackout() {}

	void Activate() {activated = true;}
	bool IsActive() {return activated;}

	virtual void Perform() = 0;
};


} //close namespace

#endif
