
#include "stdafx.h"

#include "except_rlc.h"

//Utils
//API Tiers
//Diagnostics
#include "msg_db.h"

namespace dpt {

Exception_RLC::Exception_RLC
(std::string f, int r, std::string ui, int un, bool df, RecordLock* bl)
: Exception(DML_RECORD_LOCK_FAILED, "Record locking conflict"),
	filename(f), recnum(r), userid(ui), usernum(un), during_find(df), blocker(bl)
{}

} // close namespace
