
#include "stdafx.h"

#include "update.h"

//Utils
#include "dataconv.h"
#include "winutil.h"
//API Tiers
#include "atom.h"
#include "atomback.h"
#include "lockspecial.h"
#include "dbfile.h"
#include "dbserv.h"
#include "core.h"
#include "msgroute.h"
//Diagnostics
#include "msg_db.h"
#include "except.h"

namespace dpt {

ThreadSafeLong UpdateUnit::sys_hi_id = 0;

std::set<unsigned int> UpdateUnit::sys_constrained_recnums;
Lockable UpdateUnit::sys_constraints_lock;

//*****************************************************************************
void UpdateUnit::RegisterAtomicUpdateFile(AtomicUpdate* au, DatabaseFile* file)
{
	AcquireTBOReservedMemory();

	//Start a new transaction if required
	if (!AnyUpdates()) {
		id = sys_hi_id.Inc();
		backoutable = au->IsBackoutable();
	}

	//We set a flag to prevent the mixing of backoutable and non-backoutable
	//updates in a single transaction.  This is here to create EOT behaviour similar
	//to M204's, which is partially for compliance purposes and partially because 
	//there are good things about M204's behaviour (e.g. no commits during a long
	//sequence of DEFINE FIELD commands).
	else {
		if (backoutable != au->IsBackoutable())
			throw Exception(DB_ALGORITHM_BUG, 
				"Bug: Trying to mix backoutable and non-backoutable updates");
	}

	std::map<DatabaseFile*, _int64>::iterator i = files.find(file);

	//Add the file in question to the current update unit's set of files.
	if (i == files.end()) {
		file->BeginUpdate(dbapi);

		std::pair<DatabaseFile*, _int64> nf = std::make_pair<DatabaseFile*, _int64>(file, 0);
		std::pair<std::map<DatabaseFile*, _int64>::iterator, bool> ins = files.insert(nf);
		i = ins.first;
	}

	//This flag is usually true but false when opening a file, which is registered
	//as an update for buffer flushing purposes, but we don't want to see "END OF UPDATE"
	//messages if this is all that an update consists of.
	if (au->IsARealUpdate()) {
		real_updates = true;

		//Make a baseline so we can work out updttime when the transaction ends
		if (updttime_base == 0) {
			_int64 now = win::GetWinSysTime();
//			TRACE("**------Txn BASE: %s \n", util::Int64ToString(now).c_str());
			updttime_base = now;
		}

		if (i->second == 0) {
			_int64 now = win::GetWinSysTime();
//			TRACE("**------File BASE: %s \n", util::Int64ToString(now).c_str());
			i->second = now; 
		}
	}
}

//*****************************************************************************
void UpdateUnit::LogCompensatingUpdate(AtomicUpdate* au)
{
	AtomicBackout* ab = NULL;

	try {
		ab = au->CreateCompensatingUpdate();

		//Some atomic updates are non-backoutable - NULL here
		if (ab)
			backout_log.push_back(ab);
	}
	catch (...) {
		if (ab)
			delete ab;

		//Out of memory here is OK as far as attempting backout is concerned
		throw Exception(TXN_BENIGN_ATOM, "Problem storing backout log entries");
	}
}

//*****************************************************************************
void UpdateUnit::InsertConstrainedRecNum(int recnum)
{
	LockingSentry ls(&sys_constraints_lock);
	sys_constrained_recnums.insert(recnum);
	//Not the end of the world if memory here and the sys table is left with extra entry
	my_constrained_recnums.insert(recnum);
}

//*****************************************************************************
void UpdateUnit::RemoveConstrainedRecNum(int recnum)
{
	LockingSentry ls(&sys_constraints_lock);
	my_constrained_recnums.erase(recnum);
	sys_constrained_recnums.erase(recnum);
}

//*****************************************************************************
bool UpdateUnit::FindConstrainedRecNum(int recnum)
{
	LockingSentry ls(&sys_constraints_lock);
	bool present = (sys_constrained_recnums.find(recnum) != sys_constrained_recnums.end());
	return present;
}

//*****************************************************************************
bool UpdateUnit::Commit(bool if_backoutable, bool if_nonbackoutable)
{
	if (!AnyUpdates())
		return false;

	//The caller sometimes wishes to only commit if the txn is of the other type
	if (backoutable && !if_backoutable)
		return false;
	if (!backoutable && !if_nonbackoutable)
		return false;

	//Discard all backout info
	int txid = ClearBackoutInfo();
	bool significant = (txid != -1); //see elsewhere

	///And flush updates if we're the last updater of any files
	ReleaseOrFlushFiles(false, true, significant);

	//Don't issue this message if updates were "insignificant" (see comments elsewhere)
	if (significant) {
		dbapi->Core()->GetRouter()->Issue(TXN_UPDATE_COMPLETE, std::string("End of update ")
		.append(util::IntToString(txid)));
	}

	return significant;
}

//*****************************************************************************
bool UpdateUnit::Backout(bool discreet)
{
	int num_backouts = 0;
	ReleaseTBOReservedMemory();

	//Undo the atomic updates in reverse order.  Note that we don't need to create
	//MROs such as Record* and RecordSet*, since the relevant record numbers and 
	//bitmaps respectively get recorded in the AtomicBackout log entries which
	//we call Perform() for here.  In terms of record locking, assuming TBO/LPU
	//is on, the LPU lock will remain from the forward updates.  Otherwise TBO
	//here may well fail with record locking conflicts, and that is fair enough.
	try {
		backing_out = true;

		for (int x = backout_log.size() - 1; x >= 0; x--) {
			AtomicBackout* bo = backout_log[x];
			if (bo == NULL)
				continue;
			if (bo->IsActive())
				bo->Perform();

			num_backouts++;
		}
	}

	//If these happen we just clean up and rethrow.  The calling thread can do what
	//it wants, but the BB session level will do a soft restart here.
	catch (Exception& e) {
		backing_out = false;
		dbapi->Core()->GetRouter()->Issue(TXN_BACKOUT_ERROR, std::string
			("Error during backout: ").append(e.What()));

		AbruptEnd();
		throw;
	}
	catch (...) {
		backing_out = false;
		dbapi->Core()->GetRouter()->Issue(TXN_BACKOUT_ERROR, 
			"Serious unknown error during backout");

		AbruptEnd();
		throw;
	}

	int txid = ClearBackoutInfo();
	bool significant = (txid != -1); //see elsewhere

	//There is an effective commit here
	//V2.27.  Correct typo.  It crops up trying to open a blank/otherwise unopenable file
//	ReleaseOrFlushFiles(false, false, true);
	ReleaseOrFlushFiles(false, false, significant);

	if (num_backouts > 0 && significant) {
		std::string msg("Transaction ");
		msg.append(util::IntToString(txid));
		msg.append(" has been backed out (");
		msg.append(util::IntToString(num_backouts));
		msg.append(" atomic updates)");

		dbapi->Core()->GetRouter()->Issue(TXN_BACKOUT_COMPLETE, msg);
	}
	else if (!discreet)
		dbapi->Core()->GetRouter()->Issue(TXN_BACKOUT_NO_UPDATES, 
			"There were no updates for backout to process");

	AcquireTBOReservedMemory();
	return (num_backouts > 0);
}

//******************************************************************************************
void UpdateUnit::AcquireTBOReservedMemory()
{
	//This is a possibly futile attempt to handle the situation where TBO was invoked 
	//purely because of a minor memory problem somewhere.  Clearly the moment we release 
	//this memory anybody in the system can get hold of it, but never mind.
	if (!tbo_reserved_memory) {
		try {
			tbo_reserved_memory = new char[4096];
		}
		//No problem if we fail - we'll just have to take our chances if TBO is needed later
		catch (...) {}
	}
}

//******************************************************************************************
void UpdateUnit::ReleaseTBOReservedMemory()
{
	if (tbo_reserved_memory) {
		delete[] tbo_reserved_memory;
		tbo_reserved_memory = NULL;
	}
}

//*****************************************************************************
int UpdateUnit::ClearBackoutInfo()
{
	//Any compensating updates
	if (backout_log.size() > 0) {
		for (size_t x = 0; x < backout_log.size(); x++)
			if (backout_log[x])
				delete backout_log[x];

		backout_log.clear();
	}

	//Any constraints info
	LockingSentry ls(&sys_constraints_lock);
	std::set<unsigned int>::const_iterator i;
	for (i = my_constrained_recnums.begin(); i != my_constrained_recnums.end(); i++)
		sys_constrained_recnums.erase(*i);
	my_constrained_recnums.clear();

	//Unlock updated records if there were any
	ReleaseRecordsBeingUpdated();

	if (!AnyUpdates())
		return -1;

	int temp = id; 
	id = -1; 

	//Use the same ID again next time if there were no real updates (see above)
	if (real_updates)
		real_updates = false;
	else {
		sys_hi_id.Dec();
		return -1;
	}

	return temp;
}


//*****************************************************************************
void UpdateUnit::ReleaseOrFlushFiles(bool aborting, bool commit, bool significant)
{
	_int64 now = win::GetWinSysTime();
//	TRACE("**------Flush Now = %s \n", util::Int64ToString(now).c_str());

	//Here's where we perform the buffer flush if we're the last updater of 
	//each file, or just forget the file if we're not.
	std::map<DatabaseFile*, _int64>::iterator i;
	for (i = files.begin(); i != files.end(); i++) {

		//Try to ensure each file gets a turn even if there are flush problems at EOT.
		try {
			DatabaseFile* f = i->first;

			if (aborting)
				f->MarkLogicallyBroken(dbapi, true);
			else if (significant)
				f->IncUpdateStat(commit);

			_int64 updttime_file = (i->second == 0) ? 0 : now - i->second;
//			TRACE("**------UPDTTIME File = %s \n", util::Int64ToString(updttime_file).c_str());
			f->CompleteUpdateAndFlush(dbapi, significant, updttime_file);			
		}
		//It's bad news if this doesn't work, but try all files to minimize the damage.
		//If pages aren't flushed the file will probably be left physically inconsistent.
		//In one or two cases during OPEN it is however not a problem, so I left the
		//message quite soft.
		catch (...) {
			dbapi->Core()->GetRouter()->Issue(TXN_EOT_FLUSH_FAILED, std::string
				("Warning: Pages were not flushed at EOT for file ")
				.append((FileHandle(i->first, BOOL_SHR)).GetDD()));
		}
	}
	files.clear();

	_int64 updttime_txn = (updttime_base == 0) ? 0 : now - updttime_base;
//	TRACE("**------UPDTTIME Txn = %s \n", util::Int64ToString(updttime_txn).c_str());
	dbapi->AddToStatUPDTTIME(updttime_txn);
	updttime_base = 0;

	//If the last checkpoint timed out, have a quick stab at it now
	if (dbapi->GetNumTimedOutChkps()) {

		//But don't worry if it fails
		try {
			dbapi->Checkpoint(-1);
		}
		catch (Exception& e) {
			dbapi->Core()->GetRouter()->Issue(CHKP_ABORTED_NOTERM, e.What());
		}
		catch (...) {
			dbapi->Core()->GetRouter()->Issue(CHKP_ABORTED_NOTERM, 
				"Implied checkpoint failed (unknown reason)");
		}
	}
}

//*****************************************************************************
//This is called during a restart at the point where DatabaseServices dies, and
//also if backout fails above.
//*****************************************************************************
void UpdateUnit::AbruptEnd()
{
	if (!AnyUpdates())
		return;

	//The updates are just left as they are - discard backout info.  NB. even if
	//TBO is off this function is important to flush files and mark them logically broken.
	int txid = ClearBackoutInfo();
	bool significant = (txid != -1); //see elsewhere

	//No need to report this if files have just been opened
	if (significant) {
		dbapi->Core()->GetRouter()->Issue(TXN_ABORTED, std::string
			("Update aborted: ").append(util::IntToString(files.size()))
			.append(" file(s) may now be logically inconsistent"));
	}

	ReleaseOrFlushFiles(true, false, significant);
}


//**********************************************************************************************
void UpdateUnit::PlaceRecordUpdatingLock(DatabaseFile* f, int recnum)
{
	FileRecordSet_RecordUpdate* set = new FileRecordSet_RecordUpdate(f, recnum);

	try {
		//Don't hold multiple locks for the same record in the same update unit - no need.
		//Failed lock throws here - false return code means this thread has the record locked.
		if (set->ApplyLock(dbapi))
			records_being_updated.push_back(set);
		else
			delete set;
	}
	catch (...) {
		delete set;
		throw;
	}
}

//**********************************************************************************************
void UpdateUnit::ReleaseRecordsBeingUpdated()
{
	for (size_t x = 0; x < records_being_updated.size(); x++)
		delete records_being_updated[x];
	records_being_updated.clear();
}

//**********************************************************************************************
void UpdateUnit::EndOfMolecule(bool force_commit)
{
	//Some updates like INITIALIZE force a commit.
	if (force_commit)
		dbapi->Commit(true, true);

	//The commit includes release of LPU locks on updated records.  When TBO is off
	//these locks are still placed prior to updating records (it's a key plank of the 
	//data integrity strategy of the system) but can be released now that the record update
	//is complete.
	else if (!dbapi->TBOIsOn())
		ReleaseRecordsBeingUpdated(); //will only therefore be 1 record max
}

//**********************************************************************************************
//As it says on the tin
//**********************************************************************************************
void UpdateUnit::MoleculeExceptionHandler(Exception* e, bool memory, DatabaseFile* f)
{
	std::string err;
	if (e)
		err = e->What();
	else if (memory)
		err = "Out of memory during file update";
	else
		err = "Unknown error during file update";

	if (e) {

		//This is where an atomic update had not touched anything before failing
		if (e->Code() == TXN_BENIGN_ATOM) {

			if (dbapi->TBOIsOn()) {
				Backout();
				throw Exception(TXNERR_AUTO_BACKOUT, std::string
					("Automatic backout invoked: ").append(err));
			}
			//No TBO but the file is not physically broken
			else {
				AbruptEnd();
				throw Exception(TXNERR_LOGICALLY_BROKEN, std::string(err));
			}
		}
	}

	//All exception situations that haven't explicitly been coded as "benign" are assumed
	//to have happened at a time when the file has had some kind of partial update made
	//on it. 
	f->MarkPhysicallyBroken(dbapi, true);

	//All files in the transaction will be logically inconsistent
	AbruptEnd();

	throw Exception(TXNERR_PHYSICALLY_BROKEN, std::string(err)
		.append(", file is now physically inconsistent"));
}



} //close namespace


