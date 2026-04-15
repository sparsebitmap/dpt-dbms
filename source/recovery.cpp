
#include "stdafx.h"

#include "recovery.h"

#include <direct.h>

//Utils
#include "bbstdio.h"
#include "paged_io.h"
//API tiers
#include "checkpt.h"
#include "dbstatus.h"
#include "dbfile.h"
#include "buffmgmt.h"
#include "dbserv.h"
#include "core.h"
#include "msgroute.h"
//Diagnostics
#include "except.h"
#include "msg_db.h"

namespace dpt {

//Define static objects
DatabaseServices* Recovery::rb_dbapi = NULL;
int Recovery::rb_phase = 0;
int Recovery::rb_num_preimages = 0;
time_t Recovery::rb_cptimestamp = 0;
std::string Recovery::rb_backupdir = "#DUMPS";
bool Recovery::rb_backups_taken = false;
bool Recovery::bypassed = true;

std::string Recovery::failed_reason;
int Recovery::failed_code = 0;
int Recovery::control_code = 0;

std::map<std::string, std::string> Recovery::rb_allocation_info;
std::set<std::string> Recovery::rb_required_files;
std::vector<std::string> Recovery::rb_backup_filenames;
std::map<std::string, FileHandle> Recovery::rb_opened_files;
std::map<std::string, std::string> Recovery::rb_failed_files;

//Install default callbacks
void (*Recovery::callback_rb_prescan)() = Recovery::DefaultCallbackRBPrescan;
void (*Recovery::callback_rb_openfiles)() = Recovery::DefaultCallbackRBOpenFiles;
void (*Recovery::callback_rb_backup)() = Recovery::DefaultCallbackRBBackup;
void (*Recovery::callback_rb_rollback)() = Recovery::DefaultCallbackRBRollback;
void (*Recovery::callback_rb_restore)() = Recovery::DefaultCallbackRBRestore;

bool (*Recovery::callback_rb_setprogress)(int, _int64*) 
						= Recovery::DefaultCallbackRBSetProgress;
bool (*Recovery::callback_rb_advanceprogress)(int) 
						= Recovery::DefaultCallbackRBAdvanceProgress;
void (*Recovery::callback_rb_configureprogress)(const char*, _int64*) 
						= Recovery::DefaultCallbackRBConfigureProgress;

//*************************************************************************************
void Recovery::StartRollback(DatabaseServices* d)
{
	if (rb_dbapi)
		throw Exception(MISC_SINGLETON, 
			"Recovery has already been invoked once in this run");

	rb_dbapi = d;
}

//**************************************************************************************
void Recovery::RollbackDeleteBackups()
{
	for (unsigned int x = 0; x < rb_backup_filenames.size(); x++)
		util::StdioDeleteFile(rb_backup_filenames[x].c_str());

	rb_backup_filenames.clear();
}

//**************************************************************************************
void Recovery::RollbackCloseDatabaseFiles()
{
	std::map<std::string, FileHandle>::iterator oi;
	for (oi = rb_opened_files.begin(); oi != rb_opened_files.end(); oi++)
		DatabaseFile::Destroy(rb_dbapi, oi->second);
	rb_opened_files.clear();
}

//**************************************************************************************
void Recovery::RollbackSetFilesStatusBypassed()
{
	std::set<std::string>::iterator i;
	for (i = rb_required_files.begin(); i != rb_required_files.end(); i++)
		FileStatusInfo::SetRecoveryFailedReason(*i, "manually bypassed by user");
}



//*************************************************************************************
//*************************************************************************************
//The worker functions
//*************************************************************************************
//*************************************************************************************
void Recovery::WorkerFunctionRBPrescan()
{
	bypassed = false;

	for (;;) {

		//Use the address of this for progress reporting as it's a 2 word quantity
		_int64 bytes_so_far = CheckpointFile::Object()->Tell();

		if (SetProgress(1, &bytes_so_far))
			throw Exception(ROLLBACK_CANCELLED, "Cancelled during phase 1");

		CheckpointFileRecord* rec = CheckpointFile::Object()->ReadIn(NULL);
		if (rec == NULL)
			return;

		CheckpointHeaderRecord* hdr = rec->CastToHeader();
		if (hdr) {
			rb_cptimestamp = hdr->GetTimeStamp();
			delete hdr;
			continue;
		}

		if (rb_cptimestamp == 0)
			throw Exception(ROLLBACK_ABORTED, "Bug - No CP header record");

		CheckpointAllocatedFileRecord* alloc = rec->CastToAllocatedFile();
		if (alloc) {
			rb_allocation_info[alloc->GetDD()] = alloc->GetDSN();
			delete alloc;
			continue;
		}

		CheckpointPreImageRecord* preimage = rec->CastToPreImage();
		std::string fn = preimage->GetFileName();
		delete preimage;

		if (rb_allocation_info[fn].length() == 0)
			throw Exception(ROLLBACK_ABORTED, 
				"Bug - CP preimage for non-allocated file!");

		rb_num_preimages++;

		//All files that have pre-images will feature in the STATUS command this run,
		//regardless of whether we can recover, open, or even allocate them.
		if (rb_required_files.insert(fn).second)
			FileStatusInfo::Attach(fn, NULL);
	}
}

//*************************************************************************************
void Recovery::WorkerFunctionRBOpenFiles()
{
	bypassed = false;

	std::set<std::string>::const_iterator i;
	for (i = rb_required_files.begin(); i != rb_required_files.end(); i++) {

		if (AdvanceProgress(1))
			throw Exception(ROLLBACK_CANCELLED, "Cancelled during phase 2");

		const std::string& dd = *i;
		std::string dsn = rb_allocation_info[*i];
		std::string fail_reason;
		FileHandle fh;

		try {
			fh = DatabaseFile::Construct(dd, dsn, FILEDISP_OLD, std::string());
		}
		catch (Exception& e) {
			fail_reason = e.What();
		}
		catch (...) {
			fail_reason = "unknown";
		}

		if (fail_reason.length() == 0) {

			try {
				//I've decided not to use the normal Open() code as that messes with
				//the FCT too much, and since we're going to blat the FCT anyway there
				//would be no point either.  Also none of the rollback code is going
				//via the buffer pool - it's all direct page IO.
				DatabaseFile* f = static_cast<DatabaseFile*>(fh.GetFile());
				f->BuffAPI()->ValidateFileSize();

				//This timestamp in the FCT must exactly equal that of the CP header rec
				time_t lastcp = f->RollbackGetLastCPTime(rb_dbapi);

				if (lastcp > rb_cptimestamp)
					throw Exception(0, "Invalid rollback information - obsolete");
				else if (lastcp < rb_cptimestamp)
					throw Exception(0, "Invalid rollback information - old copy of file");

				//Put the handle in a map for "closing" later on
				rb_opened_files[dd] = fh;
				rb_dbapi->Core()->GetRouter()->Issue(ROLLBACK_FILEINFO,
					std::string("File opened for rollback: ").append(dd));

				FileStatusInfo::SetRecoveryFailedReason(dd, "");
			}
			catch (Exception& e) {
				DatabaseFile::Destroy(rb_dbapi, fh);
				fail_reason = e.What();
			}
			catch (...) {
				DatabaseFile::Destroy(rb_dbapi, fh);
				fail_reason = "unknown";
			}
		}

		if (fail_reason.length() > 0) {
			rb_dbapi->Core()->GetRouter()->Issue(ROLLBACK_FILEINFO,
				std::string("Open failed for file ").append(dd)
				.append(" (Reason: ").append(fail_reason).append(1, ')'));
	
			rb_failed_files[dd] = fail_reason;
			FileStatusInfo::SetRecoveryFailedReason(dd, fail_reason);
		}
	}
}

//**********************************************************************************
int Recovery::ChkpBackupProgressReporter
(const int, const ProgressReportableActivity* act, void* v)
{
	_int64 sc = act->StepsComplete();
	if (SetProgress(2, &sc))
		return PROGRESS_CANCEL_ACTIVITY;

//	TRACE("Dumped pages %d \n", act->StepsComplete());
	return PROGRESS_PROCEED;
}

//*************************************************************************************
void Recovery::WorkerFunctionRBBackup()
{
	bypassed = false;

	//Ensure the dump directory exists
	_mkdir(rb_backupdir.c_str());

	std::map<std::string, FileHandle>::iterator oi;
	for (oi = rb_opened_files.begin(); oi != rb_opened_files.end(); oi++) {
		FileHandle& fh = oi->second;
		DatabaseFile* f = static_cast<DatabaseFile*>(fh.GetFile());
		PagedFile* pf = f->BuffAPI()->GetPagedFile();

		if (AdvanceProgress(1))
			throw Exception(ROLLBACK_CANCELLED, "Cancelled during phase 3");

		const char* label = "Page";
		_int64 numpages = pf->GetSize();
		ConfigureProgress(label, &numpages);

		std::string bkname = std::string(rb_backupdir).append(1, '\\')
			.append(fh.GetDD()).append(".bak");
		PagedFile bkp(bkname);

		PagedFileCopyOperation copy(pf, &bkp, ChkpBackupProgressReporter, NULL);
		try {
			copy.Perform();
			rb_backup_filenames.push_back(bkname); //used to delete later
		}
		catch (Exception& e) {
			throw Exception(ROLLBACK_CANCELLED, 
				std::string("Error during backup (").append(e.What()).append(1, ')'));;
		}

//		rb_dbapi->Core()->GetRouter()->Issue(ROLLBACK_FILEINFO,
//			std::string("Backup complete for file ").append(fh.GetDD()));
	}

	rb_backups_taken = true;
}

//*************************************************************************************
void Recovery::WorkerFunctionRBRollback()
{
	bypassed = false;

	//We're doing a second pass of the checkpoint file now
	CheckpointFile::Object()->Rewind();

	AlignedPage preimage_buffer;

	std::set<unsigned _int64> donepages; //see below

	//Any failure in this bit is going to disallow the system from starting, so 
	//we have no need to catch exceptions and set failure reasons etc.
	for (;;) {
		CheckpointFileRecord* rec = CheckpointFile::Object()->ReadIn(preimage_buffer.data);
		if (rec == NULL)
			break;

		//Skip the header info in this pass
		CheckpointPreImageRecord* preimage = rec->CastToPreImage();
		if (!preimage) {
			delete rec;
			continue;
		}

		std::string prefile = preimage->GetFileName();
		int prepage = preimage->GetPageNum();
		delete preimage;

		if (AdvanceProgress(1))
			throw Exception(ROLLBACK_CANCELLED, "Cancelled during phase 4");

		//This table lookup isn't ideal, but there are issues with using file IDs, (see
		//tech docs) and generally there won't be many files anyway.  Maybe review later.
		std::map<std::string, FileHandle>::iterator pi = rb_opened_files.find(prefile);

		//Preimages for files that were eliminated from the process are skipped now
		if (pi == rb_opened_files.end())
			continue;

		//We also skip subsequent preimages for pages which feature in the file more than
		//once!!  This is kind of a kluge to allow for a couple of other neat things.
		//One is to allow INITIALIZE to operate without requiring a checkpoint and without 
		//having to zeroize the whole file (see dbfile.cpp).
		//Another is to allow for the very cool buffer tidy-up code, which also does
		//not require a checkpoint, with the same result as above.
		//Note that an alternative here would be to process the checkpoint file in reverse
		//but that would slightly complicate the processing which caters for a partial
		//final record (admittedly obscure).  No big deal either way really.
		//File ID lookup is OK here as we're not worried about the ID in the original run.
		unsigned _int64 key;
		ULARGE_INTEGER& castkey = *( (ULARGE_INTEGER*) &key);
		castkey.HighPart = pi->second.FileID();
		castkey.LowPart = prepage;
		if (donepages.find(key) == donepages.end())
			donepages.insert(key);
		else
			continue;

		FileHandle& fh = pi->second;
		DatabaseFile* f = static_cast<DatabaseFile*>(fh.GetFile());
		PagedFile* pf = f->BuffAPI()->GetPagedFile();

		//Write the page straight through to the file
		pf->WritePage(prepage, preimage_buffer.data);
	}

	//Finally finish off by setting timestamps into each file
	std::map<std::string, FileHandle>::iterator oi;
	for (oi = rb_opened_files.begin(); oi != rb_opened_files.end(); oi++) {
		FileHandle& fh = oi->second;
		DatabaseFile* f = static_cast<DatabaseFile*>(fh.GetFile());
		f->RollbackSetPostInfo(rb_dbapi, rb_cptimestamp);
	}
}

//*************************************************************************************
void Recovery::WorkerFunctionRBRestore()
{
	bypassed = false;

	std::map<std::string, FileHandle>::iterator oi;
	for (oi = rb_opened_files.begin(); oi != rb_opened_files.end(); oi++) {
		FileHandle& fh = oi->second;
		DatabaseFile* f = static_cast<DatabaseFile*>(fh.GetFile());
		PagedFile* pf = f->BuffAPI()->GetPagedFile();

		if (AdvanceProgress(1))
			throw Exception(ROLLBACK_CANCELLED, "Cancelled during error handling");

		const char* label = "Page";
		_int64 numpages = pf->GetSize();
		ConfigureProgress(label, &numpages);

		std::string bkname = std::string(rb_backupdir)
			.append(1, '\\').append(fh.GetDD()).append(".bak");
		PagedFile bkp(bkname);

		PagedFileCopyOperation copy(&bkp, pf, ChkpBackupProgressReporter, NULL);
		try {
			copy.Perform();
		}
		catch (Exception& e) {
			throw Exception(ROLLBACK_CANCELLED, 
				std::string("Error during restore (").append(e.What()).append(1, ')'));;
		}

//		rb_dbapi->Core()->GetRouter()->Issue(ROLLBACK_FILEINFO,
//			std::string("Restore complete for file ").append(fh.GetDD()));
	}
}

} //close namespace
