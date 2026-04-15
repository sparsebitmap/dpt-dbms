/****************************************************************************************
Templates and default implementations for the various callback functions that are
used during recovery.
****************************************************************************************/

#ifndef BB_RECOVERY
#define BB_RECOVERY

#include <vector>
#include <map>
#include <set>
#include "filehandle.h"

namespace dpt {

class DatabaseServices;
class ProgressReportableActivity;

class Recovery {

	//Rollback information
	static DatabaseServices* rb_dbapi;
	static std::map<std::string, std::string> rb_allocation_info;
	static std::set<std::string> rb_required_files;
	static std::vector<std::string> rb_backup_filenames;
	static std::map<std::string, FileHandle> rb_opened_files;
	static std::map<std::string, std::string> rb_failed_files;

	static bool rb_backups_taken;
	static time_t rb_cptimestamp;
	static int rb_num_preimages;
	static int rb_phase;
	static std::string rb_backupdir;
	static bool bypassed;

	static std::string failed_reason;
	static int failed_code;
	static int control_code;

	//Pointers to callback functions
	static void (*callback_rb_prescan) ();
	static void (*callback_rb_openfiles) ();
	static void (*callback_rb_backup) ();
	static void (*callback_rb_rollback) ();
	static void (*callback_rb_restore) ();

	//Callback defaults
	static void DefaultCallbackRBPrescan() {WorkerFunctionRBPrescan();}
	static void DefaultCallbackRBOpenFiles() {WorkerFunctionRBOpenFiles();}
	static void DefaultCallbackRBBackup() {} //todo ?? Why is there no default callback for this????  Investigate.
	static void DefaultCallbackRBRollback() {WorkerFunctionRBRollback();}
	static void DefaultCallbackRBRestore() {WorkerFunctionRBRestore();}

	//The interface to dbapi
	friend class DatabaseServices;
	static void StartRollback(DatabaseServices*);
	static void RollbackDeleteBackups();
	static void RollbackSetFilesStatusBypassed();

	static void RollbackPreScan()	{rb_phase = 1; bypassed = true; callback_rb_prescan();}
	static void RollbackOpenFiles()	{rb_phase = 2; bypassed = true; callback_rb_openfiles();}
	static void RollbackBackup()	{rb_phase = 3; bypassed = true; callback_rb_backup();}
	static void Rollback()			{rb_phase = 4; bypassed = true; callback_rb_rollback();}
	static void RollbackRestore()	{rb_phase = 5; bypassed = true; callback_rb_restore();}

	//Progress reporting callbacks
	static bool (*callback_rb_setprogress) (int, _int64*);
	static bool (*callback_rb_advanceprogress) (int);
	static void (*callback_rb_configureprogress) (const char*, _int64*);

	//Defaults
	static bool DefaultCallbackRBSetProgress(int, _int64*) {return false;}
	static bool DefaultCallbackRBAdvanceProgress(int) {return false;}
	static void DefaultCallbackRBConfigureProgress(const char*, _int64*) {}

	//Called by the worker functions
	static bool SetProgress(int p, _int64* v) {return callback_rb_setprogress(p, v);}
	static bool AdvanceProgress(int p) {return callback_rb_advanceprogress(p);}
	static void ConfigureProgress(const char* t, _int64* m) {
										callback_rb_configureprogress(t, m);}
	static int ChkpBackupProgressReporter
					(const int, const ProgressReportableActivity* act, void* v);

public:
	static DatabaseServices* RBDBAPI() {return rb_dbapi;}
	static int RBPhase() {return rb_phase;}
	static int RBPreImages() {return rb_num_preimages;}
	static time_t RBTimeStamp() {return rb_cptimestamp;}
	static const std::string& GetBackupDir() {return rb_backupdir;}
	static bool RBBackupsTaken() {return rb_backups_taken;}
	static bool Bypassed() {return bypassed;}
	static void RollbackCloseDatabaseFiles();
	static int FailedCode() {return failed_code;}
	static const std::string& FailedReason() {return failed_reason;}
	static int GetControlCode() {return control_code;}

	static const std::set<std::string>* RequiredFiles() {return &rb_required_files;}
	static const std::map<std::string, std::string>* AllocInfo() {return &rb_allocation_info;}
	static const std::map<std::string, FileHandle>* OpenedFiles() {return &rb_opened_files;}
	static const std::map<std::string, std::string>* FailedFiles() {return &rb_failed_files;}

	static void InstallRBPrescanFunction(void (*pf)()) {callback_rb_prescan = pf;}
	static void InstallRBOpenFilesFunction(void (*pf)()) {callback_rb_openfiles = pf;}
	static void InstallRBBackupFunction(void (*pf)()) {callback_rb_backup = pf;}
	static void InstallRBRollbackFunction(void (*pf)()) {callback_rb_rollback = pf;}
	static void InstallRBRestoreFunction(void (*pf)()) {callback_rb_restore = pf;}

	static void InstallRBSetProgressFunction(bool (*pf)(int, _int64*)) 
												{callback_rb_setprogress = pf;}
	static void InstallRBAdvanceProgressFunction(bool (*pf)(int)) 
												{callback_rb_advanceprogress = pf;}
	static void InstallRBConfigureProgressFunction(void (*pf)(const char*, _int64*)) 
												{callback_rb_configureprogress = pf;}

	//The basic work that has to be done - callback overrides must call these
	static void WorkerFunctionRBPrescan();
	static void WorkerFunctionRBOpenFiles();
	static void WorkerFunctionRBBackup();
	static void WorkerFunctionRBRollback();
	static void WorkerFunctionRBRestore();

	static void SetBackupDir(const std::string& s) {rb_backupdir = s;}
};


} //close namespace

#endif
