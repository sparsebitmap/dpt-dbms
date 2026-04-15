
#include "stdafx.h"

#include "dbf_rlt.h"

#include "windows.h"	//for Sleep()
#include "time.h"

//Utils
//API tiers
#include "update.h"
#include "cfr.h"
#include "dbfile.h"
#include "bmset.h"
#include "lockspecial.h"
#include "dbserv.h"
#include "core.h"
//Diagnostics
#include "msgcodes.h"
#include "except.h"
#include "except_rlc.h"

namespace dpt {

//*****************************************************************************************
RecordLock::RecordLock(DatabaseServices* d, bool b, FileRecordSet* s)
: holder(d), frecset(s), is_excl(b)
{
	time(&start_time);
}

//*****************************************************************************************
RecordLock* DatabaseFileRLTManager::ApplyShrSetLock_S
(FileRecordSet* pretender, DatabaseServices* dbapi, bool during_find)
{
	int waits_so_far = 0;
	for (;;) {

//Debugging on a single thread - save having to set up multithread situation
//#ifdef PSEUDO_LOCK
//	RecordLock blocker(dbapi, false, pretender);
//	ThrowRLC(&blocker, dbapi, during_find);
//#endif

		try {
			CFRSentry cs(dbapi, file->cfr_recenq, BOOL_SHR);

			//Here we only have to scan the excl sets.  Note that this is the purpose of 
			//the RECENQ CFR - an idea suggested by M204.
			SerialSearchExclLockTable(pretender, dbapi, during_find);
			SerialSearchRecordUpdateLockTable(pretender, dbapi);

			//OK, place the lock
			LockingSentry ls(&shr_table_lock);
			RecordLock* rl = new RecordLock(dbapi, false, pretender);

			try {
				shr_locks.insert(rl);
				return rl;
			}
			catch (...) {
				delete rl;
				throw;
			}
		}
		catch (Exception_RLC& e) {
			if (EnqRetryWait(dbapi, e.blocker, waits_so_far))
				throw;
		}
	}

	throw Exception(BUG_MISC, "Bug in record locking (S)");
}

//*************************************
RecordLock* DatabaseFileRLTManager::ApplyShrFoundSetLock
(FileRecordSet* s, DatabaseServices* d) 
{
	return ApplyShrSetLock_S(s, d, true);
}

//*************************************
RecordLock* DatabaseFileRLTManager::ApplyPAIRecordLock
(FileRecordSet_PAI* s, DatabaseServices* d) 
{
	return ApplyShrSetLock_S(s, d, false);
}

//*****************************************************************************************
RecordLock* DatabaseFileRLTManager::ApplyExclFoundSetLock
(FileRecordSet* pretender, DatabaseServices* dbapi)
{
	int waits_so_far = 0;
	for (;;) {

		try {
			CFRSentry cs(dbapi, file->cfr_recenq, BOOL_EXCL);

			//Gotta scan all tables here (see comment above)
			SerialSearchShrLockTable(pretender, dbapi, true);
			SerialSearchExclLockTable(pretender, dbapi, true);
			SerialSearchRecordUpdateLockTable(pretender, dbapi);

			//Apply
			LockingSentry ls(&excl_table_lock);
			RecordLock* rl = new RecordLock(dbapi, true, pretender);

			try {
				excl_locks.insert(rl);
				return rl;
			}
			catch (...) {
				delete rl;
				throw;
			}
		}
		catch (Exception_RLC& e) {
			if (EnqRetryWait(dbapi, e.blocker, waits_so_far))
				throw;
		}
	}

	throw Exception(BUG_MISC, "Bug in record locking (E)");
}

//*****************************************************************************************
RecordLock* DatabaseFileRLTManager::ApplyRecordUpdateLock
(FileRecordSet_RecordUpdate* pretender, DatabaseServices* dbapi)
{
	int waits_so_far = 0;
	for (;;) {

		try {
			CFRSentry cs(dbapi, file->cfr_recenq, BOOL_EXCL);

			//If there's another LPU lock on the same thread, that's OK, don't add another.
			//This prevents their build-up when lots of updates are done on the same record,
			//which would be basically OK but unnecessarily inefficient.  Do this bit first
			//as it will be quite common.
			if (KeyedSearchRecordUpdateLockTable(pretender, dbapi))
				return NULL;

			//Clash as normal with these two tables
			SerialSearchShrLockTable(pretender, dbapi, false);
			SerialSearchExclLockTable(pretender, dbapi, false);

			//Apply
			LockingSentry ls(&recupdate_table_lock);
			RecordLock* rl = new RecordLock(dbapi, true, pretender);

			try {
				record_update_locks[pretender->RecNum()] = rl;
				return rl;
			}
			catch (...) {
				delete rl;
				throw;
			}
		}
		catch (Exception_RLC& e) {
			if (EnqRetryWait(dbapi, e.blocker, waits_so_far))
				throw;
		}
	}

	throw Exception(BUG_MISC, "Bug in record locking (R)");
}

//*****************************************************************************************
void DatabaseFileRLTManager::ReleaseNormalLock(RecordLock* rl)
{
	{
		std::set<RecordLock*>* whichset;
		Sharable* plock;

		if (rl->is_excl) {
			whichset = &excl_locks;
			plock = &excl_table_lock;
		}
		else {
			whichset = &shr_locks;
			plock = &shr_table_lock;
		}

		LockingSentry ls(plock);
		std::set<RecordLock*>::iterator it = whichset->find(rl);

		if (it == whichset->end())
			throw Exception(BUG_MISC, "Bug: Missing record locking table entry");

		whichset->erase(it);
		delete rl;
	}

	NotifyWaitersOfLockRelease(rl);
}

//*****************************************************************************************
//Separate function to allow for the map optimization
void DatabaseFileRLTManager::ReleaseRecordUpdateLock(int recnum, RecordLock* rl)
{
	{
		LockingSentry ls(&recupdate_table_lock);

		std::map<int, RecordLock*>::iterator it = record_update_locks.find(recnum);
		if (it == record_update_locks.end())
			throw Exception(BUG_MISC, "Bug: Missing record locking table entry (LPU)");
		if (it->second != rl)
			throw Exception(BUG_MISC, "Bug: LPU lock table entry is corrupt");

		record_update_locks.erase(it);
		delete rl;
	}

	NotifyWaitersOfLockRelease(rl);
}

//*****************************************************************************************
//During a find that requires table B search we start off with a whole-file lock.  Then
//at the start of the scan we trim this down to the initial set to scan.  Then during the
//scan we release segments as we go.  At both those times we're just trimming the set,
//so we don't need to scan the RLTs for conflicts.
//*****************************************************************************************
void DatabaseFileRLTManager::TBSReplaceLockSet(FileRecordSet* currset, FileRecordSet* newset)
{
	LockingSentry s(&shr_table_lock);

	std::set<RecordLock*>::iterator i;
	for (i = shr_locks.begin(); i != shr_locks.end(); i++) {
		if ((*i)->frecset == currset) {
			(*i)->frecset = newset;
			return;
		}
	}

	throw Exception(BUG_MISC, "Bug: RLT info missing at start of TBS");
}

//*****************************************************************************************
void DatabaseFileRLTManager::NotifyWaitersOfLockRelease(RecordLock* releasee)
{
	SharingSentry s(&waiter_table_lock);

	std::multimap<RecordLock*, WaiterInfo*>::iterator it = waiter_table.find(releasee);

	//Several other threads may be waiting on us
	while (it != waiter_table.end()) {
		it->second->release_flag = true;
		it++;
	}
}

//*****************************************************************************************
void DatabaseFileRLTManager::SerialSearchShrLockTable
(FileRecordSet* pretender, DatabaseServices* dbapi, bool during_find)
{
	SharingSentry s(&shr_table_lock);

	std::set<RecordLock*>::const_iterator i;
	for (i = shr_locks.begin(); i != shr_locks.end(); i++) {
		RecordLock* rl = *i;

		DatabaseServices* holder = rl->ClashTest(pretender, dbapi);
		if (holder)
			//V2.04.  Mar 07.  VC2005 did not like this!
			//throw ThrowRLC(rl, holder, during_find);
			ThrowRLC(rl, holder, during_find);
	}
}

//*****************************************************************************************
void DatabaseFileRLTManager::SerialSearchExclLockTable
(FileRecordSet* pretender, DatabaseServices* dbapi, bool during_find)
{
	SharingSentry s(&excl_table_lock);

	std::set<RecordLock*>::const_iterator i;
	for (i = excl_locks.begin(); i != excl_locks.end(); i++) {
		RecordLock* rl = *i;

		DatabaseServices* holder = rl->ClashTest(pretender, dbapi);
		if (holder)
			//V2.04.  Mar 07.  VC2005 did not like this!
			//throw ThrowRLC(rl, holder, during_find);
			ThrowRLC(rl, holder, during_find);
	}
}

//*****************************************************************************************
void DatabaseFileRLTManager::SerialSearchRecordUpdateLockTable
(FileRecordSet* pretender, DatabaseServices* dbapi)
{
	SharingSentry s(&recupdate_table_lock);

	std::map<int, RecordLock*>::const_iterator i;
	for (i = record_update_locks.begin(); i != record_update_locks.end(); i++) {
		RecordLock* rl = i->second;

		DatabaseServices* holder = rl->ClashTest(pretender, dbapi);
		if (holder)
			//V2.04.  Mar 07.  VC2005 did not like this!
			//throw ThrowRLC(rl, holder, true);
			ThrowRLC(rl, holder, true);
	}
}

//*****************************************************************************************
//The purpose of adding the record number key to this table is to speed up the common case
//where lots of records have been updated.  Clearly this is better than a sequential 
//search, especially as it cuts out the recordset intersection calls too.
//*****************************************************************************************
bool DatabaseFileRLTManager::KeyedSearchRecordUpdateLockTable
(FileRecordSet_RecordUpdate* pretender, DatabaseServices* dbapi)
{
	SharingSentry s(&recupdate_table_lock);

	int recnum = pretender->RecNum();
	std::map<int, RecordLock*>::const_iterator it = record_update_locks.find(recnum);

	if (it == record_update_locks.end())
		return false;

	//No need to place another one for the same thread
	if (it->second->holder == dbapi)
		return true;

	//V2.04.  Mar 07.  VC2005 did not like this!
//	throw ThrowRLC(it->second, it->second->holder, false);
	ThrowRLC(it->second, it->second->holder, false);
	return false; //irrelevant after throw
}

//*****************************************************************************************
bool DatabaseFileRLTManager::EnqRetryWait
(DatabaseServices* dbapi, RecordLock* blocker, int& waits_so_far)
{
	waits_so_far++;

	//Regardless of whether we get the lock or throw, WTRLK is incremented (once per FD)
	if (waits_so_far == 1)
		dbapi->IncStatWTRLK();

	if (waits_so_far > dbapi->GetParmENQRETRY())
		return true;

	WTSentry ws(dbapi->Core(), 7);

	//Add an entry to this table which enables us to "wake up" from the 3 second wait
	//if the holder releases the lock which caused us to wait in the first place.
	WaiterInfo* wi = InsertWaiterInfo(dbapi, blocker);

	try {
		//This wait state is both bumpable and interruptable - hence small intervals.
		//Note also that on DPT there is no way that we will find the conflict resolved
		//at the end of 3 seconds, since all lock releasings will notify us.  The 3 seconds
		//is purely here for Model 204 compatibility - i.e. interaction with ENQRETRY.
		for (int i = 0; i < 60; i++) {
			Sleep(50);

			//Bumped?
			dbapi->Core()->Tick("during wait for record lock");

			//Set released?  No need to do threadsafe read of flag.
			if (wi->release_flag)
				break;
		}

		DeleteWaiterInfo(dbapi, blocker);
	}
	catch (...) {
		DeleteWaiterInfo(dbapi, blocker);
		throw;
	}

	return false;
}

//*****************************************************************************************
DatabaseFileRLTManager::WaiterInfo* 
DatabaseFileRLTManager::InsertWaiterInfo(DatabaseServices* dbapi, RecordLock* rl)
{
	LockingSentry s(&waiter_table_lock);

	std::pair<RecordLock*, WaiterInfo*> p;
	p.first = rl;
	p.second = NULL;
	try {
		p.second = new WaiterInfo(dbapi);
		std::multimap<RecordLock*, WaiterInfo*>::iterator it = waiter_table.insert(p);
		return p.second;
	}
	catch (...) {
		if (p.second)
			delete p.second;
		throw;
	}
}

//*****************************************************************************************
void DatabaseFileRLTManager::DeleteWaiterInfo(DatabaseServices* dbapi, RecordLock* rl)
{
	LockingSentry s(&waiter_table_lock);

	std::multimap<RecordLock*, WaiterInfo*>::iterator it = waiter_table.find(rl);

	//Since several of us may be waiting on the same blocker
	while (it != waiter_table.end()) {
		if (it->second->dbapi == dbapi) {
			delete it->second;
			waiter_table.erase(it);
			return;
		}
		it++;
	}

	throw Exception(BUG_MISC, "Bug: Missing RLT waiter table info");
}


//*****************************************************************************************
void DatabaseFileRLTManager::ThrowRLC
(RecordLock* blocker, DatabaseServices* holder, bool during_find)
{
	//No need to try and lock the holder in, as they can't leave till we release RECENQ
	throw Exception_RLC(
		file->GetDDName(), 
		blocker->frecset->RLCRec(), 
		holder->Core()->GetUserID(), 
		holder->Core()->GetUserNo(),
		during_find,
		blocker);
}


//*****************************************************************************************
DatabaseServices* RecordLock::ClashTest(FileRecordSet* pretender, DatabaseServices* d)
{
	//A thread can not cause record locking with itself
	if (d == holder)
		return NULL;

	if (frecset->AnyIntersection(pretender))
		return holder;
	else
		return NULL;
}

} //close namespace


