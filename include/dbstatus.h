/*******************************************************************************************
Information primarily for the STATUS command
*******************************************************************************************/
#if !defined(BB_DBSTATUS)
#define BB_DBSTATUS

#include <string>
#include <vector>
#include <map>

#include "lockable.h"

namespace dpt {

class DatabaseFile;

class FileStatusInfo {
	static std::map<std::string, FileStatusInfo*> systable;
	static Lockable syslock;

	DatabaseFile* file;
	time_t alloc_time;
	bool recovery_required;
	std::string recovery_failure_reason;

	void SetAllocTime();
	void SetFile(DatabaseFile* f) {file = f; SetAllocTime();}

	FileStatusInfo() : file(NULL), recovery_required(false) {SetAllocTime();}

public:
	static void Attach(const std::string&, DatabaseFile*);
	static void Detach(const std::string&);

	static void DestroyStatusInfo();
	static void SetRecoveryFailedReason(const std::string& fn, const std::string& r) {
		systable[fn]->recovery_failure_reason = r;}

	//----------------------
	//For the STATUS command
	struct DumpItem {
		std::string filename;
		time_t alloc_time;
		bool open;
		bool updated_ever;
		bool updated_since_checkpoint;
		bool recovery_required;
		std::string recovery_failure_reason;

		DumpItem() : alloc_time(0), recovery_required(false) {}
	};
	static void Dump(std::vector<DumpItem>&);
};

} //close namespace

#endif
