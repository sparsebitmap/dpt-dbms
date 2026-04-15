/****************************************************************************************
Functionality shared by all database API cursors
****************************************************************************************/

#if !defined(BB_DBCURSOR)
#define BB_DBCURSOR

#include <set>
#include "apiconst.h"

namespace dpt {

class DBCursor;
class DBCursorHandle;

//************************************************************************************
class DBCursorTarget {
	std::set<DBCursor*> cursors;

protected:
	void RegisterNewCursor(DBCursor* c) {cursors.insert(c);}
	void DeleteCursor(DBCursor* c);

	void RequestRepositionCursors(int = 0);

	virtual ~DBCursorTarget();
public:
	bool AnyCursors() const {return (cursors.size() > 0);}
};

//************************************************************************************
class DBCursor {
	DBCursorTarget* target;

protected:
	friend class DBCursorHandle;
	DBCursorHandle* handle;
	bool needs_reposition;

	//V2.06
	CursorOptions opts;
	void PostAdvance(int);

	DBCursor(DBCursorTarget* t) 
		: target(t), handle(NULL), needs_reposition(true), opts(CURSOR_DEFOPTS) {}

	friend class DBCursorTarget;
	virtual ~DBCursor();
	virtual void RequestReposition(int) {needs_reposition = true;}

public:
	DBCursorTarget* Target() {return target;}

	virtual void GotoFirst() = 0;
	virtual void GotoLast() = 0;
	virtual void Advance(int) = 0;

	virtual bool Accessible() = 0;
	bool CanEnterLoop() {return Accessible();} //more readable sometimes

	void SetOptions(CursorOptions);
};

} //close namespace

#endif
