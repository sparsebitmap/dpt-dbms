
#include "stdafx.h"

#include "cfr.h"

//Utils
#include "resource.h"
//API tiers
#include "dbfile.h"
#include "dbserv.h"
#include "core.h"
//Diagnostics
#include "except.h"


namespace dpt {

//*********************************************************************************
//Overrides
//*********************************************************************************
void CriticalFileResource::Get(DatabaseServices* dbapi, bool required_lock)
{
	int wt = (required_lock == BOOL_EXCL) ? 24 : 25;
	WTSentry ws(dbapi->Core(), wt);

	//This will wait if required.  This function is used when acquiring CFRs
	//during most database work, and we are happy for it to spin.
	Resource::Get(required_lock, dbapi->Core(), dbapi);
}

//*********************************************************************************
bool CriticalFileResource::UpgradeToExcl()
{
	//The resource must be held for this not to throw
	Release();

	//We only take one attempt here - all functions that need to upgrade would not
	//want to wait.  Hence no need to set a WT value.  When releasing the UPDATING
	//CFR we make use of the fact that if the EXCL fails we are left with no lock.
	return Resource::Try(BOOL_EXCL);
}

//*********************************************************************************
/*

  //Is this function ever needed?

void CriticalFileResource::DowngradeToShr()
{
	Release();

  if (!Resource::Try(BOOL_SHR))
		throw Exception(e.Code(),
			"Another user appears to have opened the file (possible bug?)");
}

*/



//*********************************************************************************
//*********************************************************************************
//Sentry class
//*********************************************************************************
//*********************************************************************************
CFRSentry::CFRSentry(DatabaseServices* d, CriticalFileResource* r, bool lock_type)
: dbapi(d), res(r), enabled(false)
{
	//Always wait
	res->Get(dbapi, lock_type);

	enabled = true;
}

//*********************************************************************************
//Sentry constructed when lock is already held - useful for containers of UPDATING
CFRSentry::CFRSentry(DatabaseServices* d, CriticalFileResource* r, CriticalFileResource*)
: dbapi(d), res(r), enabled(true)
{}

//*********************************************************************************
void CFRSentry::Release()
{
	if (!enabled)
		return;
	
	res->Release();
	enabled = false;
}

//*********************************************************************************
void CFRSentry::Get(DatabaseServices* d, CriticalFileResource* r, bool lock_type)
{
	//Can only have one lock
	if (enabled)
		return;

	res = r;
	dbapi = d;

	res->Get(dbapi, lock_type);
	enabled = true;
}

//*********************************************************************************
//Copy function to allow containers full of these things
//*********************************************************************************
void CFRSentry::CopyFrom(const CFRSentry& s)
{
	//Can't hold 2 locked CFRs, so release any currently controlled by this handle
	Release();

	//Adopt responsibility for the lock
	res = s.res;
	dbapi = s.dbapi;
	enabled = s.enabled;

	//The previous owner now no longer has to release it
	s.enabled = false;
}





} //close namespace


