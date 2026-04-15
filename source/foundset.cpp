
#include "stdafx.h"

#include "foundset.h"

//Utils
//API Tiers
#include "dbctxt.h"
//Diagnostics
#include "except.h"
#include "msg_db.h"

namespace dpt {

//***************************************************************************************
void FoundSet::AppendFileSet(int groupix, BitMappedFileRecordSet* set)
{
	std::pair<int, BitMappedFileRecordSet*> newfileset;
	newfileset = std::make_pair<int, BitMappedFileRecordSet*>(groupix, set);

	std::pair<std::map<int, BitMappedFileRecordSet*>::iterator, bool> insflag;
	insflag = data.insert(newfileset);

	//The find engine should never add the same file twice.  It will generally be
	//processing the files in a group in order - hence the function name "append...".
	if (insflag.second == false)
		throw Exception(DB_MRO_MGMT_BUG, 
			"Bug: group find engine processed the same file twice");
}

//***************************************************************************************
void FoundSet::DirtyDeleteAdhocLockExcl(DatabaseServices* dbapi)
{
	try {
		std::map<int, BitMappedFileRecordSet*>::iterator i;
		for (i = data.begin(); i != data.end(); i++)
			i->second->LockExcl(dbapi);

		is_unlocked = false;
	}
	catch (...) {
		//A failure in any group member means we release all
		Unlock();
		throw;
	}
}

//***************************************************************************************
void FoundSet::Unlock()
{
	std::map<int, BitMappedFileRecordSet*>::iterator i;
	for (i = data.begin(); i != data.end(); i++)
		i->second->Unlock();

	is_unlocked = true;
}

//***************************************************************************************
void FoundSet::NotifyOfDirtyDelete(BitMappedRecordSet* s)
{
	//See comments by the deleted code in bmset.cpp.  Set the flag if it's an unlocked
	//set on any thread, or any set on this thread.
	if (is_unlocked || s->Context()->DBAPI() == Context()->DBAPI())
		RecordSet::NotifyOfDirtyDelete(s);
}

} //close namespace


