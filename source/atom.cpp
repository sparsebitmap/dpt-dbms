
#include "stdafx.h"

#include "atom.h"

//Utils
//API Tiers
#include "atomback.h"
#include "update.h"
#include "dbctxt.h"
#include "dbserv.h"
//Diagnostics
#include "assert.h"

namespace dpt {

//**************************************************************************************
void AtomicUpdate::RegisterAtomicUpdate(bool skip_file_register)
{
	DatabaseServices* dbapi = context->DBAPI();

	UpdateUnit* uu = dbapi->GetUU();
	assert(uu);

	//This mainly ensures that the file in question is a part of the current update unit
	//so it gets flushed.  
	//V2.03 Feb 07.  There are occasional times when we may as well skip it (e.g. during
	//field adds after a molecular store).
	if (!skip_file_register)
		uu->RegisterAtomicUpdateFile(this, context->GetDBFile());

	//No need to store TBO log entries if TBO is off
	if (dbapi->TBOIsOn())
		uu->LogCompensatingUpdate(this);
	else
		//This can be used by atoms to avoid doing certain work.  For example
		//FILE RECORDS need not build a backout set as it does its inverted list work.
		tbo_is_off = true;
}

} //close namespace


