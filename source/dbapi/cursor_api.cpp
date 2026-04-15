
#include "stdafx.h"

#include "dbapi\cursor.h"
#include "dbcursor.h"

namespace dpt {

void APICursor::GotoFirst()
{
	target->GotoFirst();
}

void APICursor::GotoLast()
{
	target->GotoLast();
}

void APICursor::Advance(int n)
{
	target->Advance(n);
}

bool APICursor::Accessible()
{
	return target->Accessible();
}

void APICursor::SetOptions(CursorOptions o)
{
	target->SetOptions(o);
}



} //close namespace


