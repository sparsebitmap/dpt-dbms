
#include "stdafx.h"

#include "buffhandle.h"

//Utils
//API tiers
#include "buffmgmt.h"
//Diagnostics
#include "except.h"
#include "assert.h"

namespace dpt {

//******************************************************************************************
BufferPageHandle::BufferPageHandle
(DatabaseServices* d, BufferedFileInterface* f, int p, bool noread)
: buff(BufferPage::Request(d, f, p, noread)), dbapi(d), enabled(false)
{
	enabled = true;
}

//******************************************************************************************
void BufferPageHandle::Release()
{
	if (!enabled)
		return;

	BufferPage::Release(dbapi, buff);
	enabled = false;
}

//******************************************************************************************
void BufferPageHandle::Copy(const BufferPageHandle& hfrom)
{
	//Can't hold 2 locked pages, so release any currently controlled by this handle
	Release();
	
	//Adopt responsibility for the lock
	buff = hfrom.buff; 
	dbapi = hfrom.dbapi;
	enabled = hfrom.enabled;
	
	//The previous owner now no longer has to release it
	hfrom.enabled = false;
}

//******************************************************************************************
//V2.12. April 2008.
void BufferPageHandle::BuffAPINoteFreshFormattedPage(char t)
{
	buff->NoteFreshFormattedPage(t);
}

//******************************************************************************************
//int BufferPageHandle::AbsolutePageNumber()
//{
//	return buff->filepage;
//}

//******************************************************************************************
//Decided this was not worth it - might return later
//******************************************************************************************
/*
BufferPageHandle BufferPageHandle::Clone()
{
	BufferPageHandle clone;

	clone.enabled = enabled;

	if (enabled) {
		clone.buff = buff;
		clone.dbapi = dbapi;

		//So when either handle is destroyed the page remains marked "in use" 
			//nb got to lock buffer system for this, which is why I decided 
			//the savings weren't good enough.
		buff->IncrementUseCountForHandleClone();
	}

	return clone;
}
*/

} //close namespace
