
#include "stdafx.h"

#include "dbcursor.h"
#include "handles.h"

#include "except.h"
#include "msg_db.h"

namespace dpt {

//****************************
DBCursorTarget::~DBCursorTarget()
{
	std::set<DBCursor*>::iterator i;
	for (i = cursors.begin(); i != cursors.end(); i++)
		delete *i;
	cursors.clear();
}

//****************************
void DBCursorTarget::DeleteCursor(DBCursor* c)
{
	if (cursors.erase(c) == 0)
		throw Exception(MRO_NONEXISTENT_CHILD, 
			"Bug: trying to close cursor twice?");
	delete c;
}

//***************************************************************************************
//This is a general facility intended to allow some functions on an object such as
//a record set to force cursors on that object to relocate.  An example is adding
//records to a UL list.  Another is releasing a found set.  No locking is involved, 
//so it's only going to work properly for objects owned by a single thread.
//***************************************************************************************
void DBCursorTarget::RequestRepositionCursors(int param)
{
	//NB. DVCs are not always registered at the moment - see OpenDirectValueCursor
	std::set<DBCursor*>::iterator i;
	for (i = cursors.begin(); i != cursors.end(); i++)
		(*i)->RequestReposition(param);
}

//****************************
DBCursor::~DBCursor()
{
	if (handle)
		handle->cursor_deleted = true;
}

//****************************
void DBCursor::SetOptions(CursorOptions o)
{
	int combo = 0;
	combo += (o & CURSOR_POSFAIL_INVALIDATE) ? 1 : 0;
	combo += (o & CURSOR_POSFAIL_REMAIN) ? 1 : 0;
	combo += (o & CURSOR_POSFAIL_NEXT) ? 1 : 0;
	combo += (o & CURSOR_POSFAIL_PREV) ? 1 : 0;
	if (combo > 1)
		throw Exception(DB_API_BAD_PARM, "Conflicting cursor options given");

	combo = 0;
	combo += (o & CURSOR_POS_FROM_CURRENT) ? 1 : 0;
	combo += (o & CURSOR_POS_FROM_FIRST) ? 1 : 0;
	combo += (o & CURSOR_POS_FROM_LAST) ? 1 : 0;
	if (combo > 1)
		throw Exception(DB_API_BAD_PARM, "Conflicting cursor options given");

	opts = o;
}

//****************************
void DBCursor::PostAdvance(int delta)
{
	//V2.06.  Don't go into no-man's-land if option is set.
	if (opts & CURSOR_ADV_NO_OVERRUN) {
		if (!Accessible()) {
			if (delta >= 0)
				GotoLast();
			else
				GotoFirst();
		}
	}
}



} //close namespace


