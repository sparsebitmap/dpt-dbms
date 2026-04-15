
#include "stdafx.h"

#include "file.h"

//Utils
#include "resource.h"
//API tiers
#include "core.h"
#include "access.h"
#ifdef _BBHOST
#include "procserv.h"
#endif
//Diagnostics
#include "except.h"
#include "msg_file.h"
#include "msg_util.h"

#ifdef DEBUG_V225_OKTECH
#include "iowrappers.h"
#include "dataconv.h"
#include "winutil.h"
#include "procctxt.h"
#endif

namespace dpt {

//Define static objects
std::vector<AllocatedFile*> AllocatedFile::all_objects;
std::map<std::string, int> AllocatedFile::dd_directory;
std::map<std::string, int> AllocatedFile::dsn_directory;
std::map<std::string, AllocatedFile*> AllocatedFile::aliases;

#ifdef _DEBUG_LOCKS
Sharable AllocatedFile::allocation_lock = Sharable("AllocatedFile syslock");
#else
Sharable AllocatedFile::allocation_lock;
#endif

//******************************************************************************************
AllocatedFile::AllocatedFile
(const std::string& d, std::string& ds, FileType ft, const std::string& a)
: dd(d), type(ft), alias(a), open_lock(NULL), pending(true)
#ifdef _DEBUG_LOCKS
  , alloc_lock(std::string("AllocatedFile: ").append(d))
#endif
{
	//Since the same OS file name can be expressed in several ways, here we expand to
	//the absolute path name.
	char fpbuff[_MAX_PATH];
	if (!_fullpath(fpbuff, ds.c_str(), _MAX_PATH))
		throw Exception(SYSFILE_BAD_DSN, 
			"Invalid path specification for OS file or directory");

#ifdef DEBUG_V225_OKTECH
	std::string dsparm = ds;
#endif

	//Record the full DSN used for later messaging etc.
	dsn = fpbuff;
	ds = dsn;

	//Check for acceptable file names (nicer than getting a low level exception thrown up).
	//NB. Backslash is also prohibited, but only because it means a directory delimiter, and
	//we allow users to specify those.  Same comment for the colon (drive specification).  If
	//the user uses a colon elsewhere in the file name, they'll get a low level message.
	//static char* OS_prohibited_chars = "/*?\"<>|";  //V2.24. non-const deprecated in gcc now.
	static const char* OS_prohibited_chars = "/*?\"<>|";
	if (dsn.find_first_of(OS_prohibited_chars) != std::string::npos)
		throw Exception(SYSFILE_BAD_DSN, std::string
		("The OS disallows these characters in file names: ").append(OS_prohibited_chars));

	//This is something I've put in - M204 might allow it
	if (dd == "$UPDATE" || dd == "$CURFILE")
		throw Exception(SYSFILE_BAD_DD, 
			"It is not recommended that you use this file name - it has a special meaning");

	//This is something I've put in - M204 might allow it
	if (dd.length() > 8)
		throw Exception(SYSFILE_BAD_DD, "File DD length must not exceed 8 characters long");

	//Is the dd or dsn already in use?
	std::map<std::string, int>::const_iterator i = dd_directory.find(d);
	if (i != dd_directory.end()) {

#ifdef _DEBUG
		std::string tempdd = all_objects[i->second]->dd;
		std::string tempdsn = all_objects[i->second]->dsn;
#endif
		if (d == all_objects[i->second]->dd && dsn == all_objects[i->second]->dsn) {
			std::string msg("File or directory ");
			msg.append(all_objects[i->second]->dd);
			msg.append(" is already allocated.");
			throw Exception(SYSFILE_ALLOC_ALREADY, msg);
		}
		else {

			//V3.01.  No reason not to add some more info.
			//std::string msg("DD name is in use");
			std::string msg("DD name is in use (allocating ");
			msg.append(dd);
			msg.append(" to ");
			msg.append(dsn);
			msg.append(")");

//			MessageBox(NULL, msg.c_str(), "Ta-dah!", 0);

#ifdef DEBUG_V225_OKTECH
			/*
			It appears here that dd_directory is out of step with the pseudo_dd_array, since
			if we come in here with e.g. "+P000005" it means that PDDA said 5 was available,
			hence +P000005 had presumably been destroyed (proc closed by last user), and 
			therefore DDD should have had entry the entry removed.  But it's there.
			*/

			std::string dumpfname = std::string(util::StartupWorkingDirectory()).append("\\")
				.append("#DUMPS\\OKTECH_DD_IN_USE.TXT");

			try {

				//Raise this thread's priority, since we may be accessing other threads' files.
				SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);

				util::StdioEnsureDirectoryExists(dumpfname.c_str());
				dumpfname = win::GetUnusedFileName(dumpfname);

				msg.append(".  **Debug** see dump in ").append(dumpfname);

				util::BBStdioFile f(dumpfname.c_str(), util::STDIO_NEW);

				f.WriteLine("Debug: 'DD name is in use...'  extra diagnostics"); 
				f.WriteLine(std::string("Time: ").append(win::GetCTime()));
				f.WriteLine("------------------------------------------------");

				f.WriteLine("");
				f.WriteLine("Stack frame in AllocatedFile(...) constructor");
				f.WriteLine("---------------------------------------------");
				f.WriteLine("Call parms:");
				f.WriteLine(std::string("  dd=").append(d));
				f.WriteLine(std::string("  dsn=").append(dsparm));
				f.WriteLine(std::string("  ftype=").append(util::IntToString(ft)));
				f.WriteLine(std::string("  alias=").append(a));
				f.WriteLine("Inferred info:");
				f.WriteLine(std::string("  path=").append(dsn));

				ProcDirectoryContext::DumpStaticData(&f);
				DumpStaticData(&f);

				msg.append(" (dump completed successfully)");
			}
			catch (Exception& e) {
				msg.append(" (dump threw a DPT exception: ");
				msg.append(e.What()).append(")");
			}
			catch (...) {
				msg.append(" (dump threw an unknown exception)");
			}

			SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);
#endif
			throw Exception(SYSFILE_ALLOC_DD_IN_USE, msg);
		}
	}
	
	i = dsn_directory.find(dsn);
	if (i != dsn_directory.end()) {
		std::string msg("OS file name is in use, on DD ");
		throw Exception(SYSFILE_ALLOC_DSN_IN_USE, msg.append(all_objects[i->second]->dd));
	}

	//A late addition to allow users to have a proc directory with the "same" name as
	//a data file.
	if (alias.length() != 0) {
		std::map<std::string, AllocatedFile*>::const_iterator ia = aliases.find(alias);
		if (ia != aliases.end()) {
			std::string msg("Alias has already been used, on DD ");
			throw Exception(SYSFILE_BAD_ALIAS, msg.append(ia->second->dd));
		}
	}

	//Try to prevent user mischief
#ifdef _BBHOST
	if (ft != FILETYPE_PROC) {
		if (dsn.find(std::string("\\").append(ProcedureServices::UserTemp())) 
			!= std::string::npos)
		{
			throw Exception(SYSFILE_ALLOC_DSN_IN_USE, 
				"You can not allocate files or directories in the #USERTMP directory");
		}
	}
#endif

	//Find the first free file id slot
	int fid = -1;
	for (size_t x = 0; x < all_objects.size(); x++) {
		if (all_objects[x] == NULL) {
			fid = x;
			all_objects[x] = (AllocatedFile*)1;
			break;
		}
	}
	if (fid == -1) {
		//create a new slot
		fid = all_objects.size();
		all_objects.push_back((AllocatedFile*)1);
	}

	file_id = fid;
	dd_directory[d] = fid;
	dsn_directory[dsn] = fid;
	all_objects[fid] = this;

	if (alias.length() != 0)
		aliases[alias] = this;

	open_lock = new Resource(std::string("OSFILE:").append(d)); 

	//V3.03. Lets us do much more efficient access control checks for file privs
#ifdef _BBDBAPI
	CoreServices* user0core = CoreServices::User0Core();
	if (user0core)
		user0core->GetAccessController()->
			RefreshAllAccountsCachedFileIDsOnFileAllocate(dd_directory, all_objects.size());
#endif
}

//******************************************************************************************
AllocatedFile::~AllocatedFile()
{
	if (!pending)
		throw Exception(BUG_MISC, "Bug: Allocated file not pre-staged during free");

	//Remove from the dd and dsn directories.  If the caller tries to delete the same object  
	//twice we'll get an access violation before now, so there's no point checking for 
	//existence in the dd directory.  Normally this function is only called in controlled
	//circumstances such as DatabaseServices::Free, which has code to ensure no repeat 
	//deletions occur.
	std::map<std::string, int>::iterator i = dd_directory.find(dd);
	int k = (i->second);

	std::map<std::string, int>::iterator j = dsn_directory.find(all_objects[k]->dsn);
	if (j == dsn_directory.end()) 
		throw Exception(BUG_MISC, "Bug: File directory is corrupt");

	dd_directory.erase(i);
	dsn_directory.erase(j);
	all_objects[k] = NULL;

	if (alias.length() != 0)
		aliases.erase(alias);

	delete open_lock;
}

//******************************************************************************************
void AllocatedFile::CommitAllocation()
{
	SharingSentry ls(&allocation_lock);
	pending = false;
}

//******************************************************************************************
void AllocatedFile::StageForFree()
{
	SharingSentry ls(&allocation_lock);
	pending = true;
}

//******************************************************************************************
void AllocatedFile::ListAllocatedFiles
(std::vector<FileHandle>& result, bool locktype, FileType ft)
{
	SharingSentry ls(&allocation_lock);

	result.clear();
	for (size_t x = 0; x < all_objects.size(); x++) {
		AllocatedFile* f = all_objects[x];

		//Skip any free slots
		if (!f)
			continue;

		//Skip undesired types
		if (ft != FILETYPE_ALL && ft != f->type)
			continue;

		//During free and allocate the calling code has a EXCL handle already
		if (f->pending)
			continue;

		FileHandle h(f, locktype);
		result.push_back(h);
	}
}

//******************************************************************************************
FileHandle AllocatedFile::FindAllocatedFile
(const std::string& filename, bool locktype, FileType preferred_type)
{
	SharingSentry ls(&allocation_lock);

	AllocatedFile* fnd = NULL;

	size_t x;
	for (x = 0; x < all_objects.size(); x++) {
		AllocatedFile* f = all_objects[x];

		//Skip any free slots
		if (!f)
			continue;
			
		if (f->dd != filename)
			continue;

		fnd = f;
		break;
	}

	//Return now if we have one with the right type.  Otherwise hold fire for a moment
	//to see if we can do better with an aliased file
	if (fnd && fnd->type == preferred_type)
		return FileHandle(fnd, locktype);

	for (x = 0; x < all_objects.size(); x++) {
		AllocatedFile* f = all_objects[x];

		if (!f)
			continue;
			
		if (f->alias != filename || f->type != preferred_type)
			continue;

		fnd = f;
		break;
	}

	return FileHandle(fnd, locktype);
}

//******************************************************************************************
//This default implementation will be used by most types.
//******************************************************************************************
void AllocatedFile::TryForOpenLock(bool lock_type, bool currently_held, bool dummy)
{
	if (!open_lock->TryChangeLock(lock_type, currently_held, dummy)) {
		std::string msg("File is in use");
		if (dd.length() > 0 && dd[0] != '+')
			msg.append(": ").append(dd);
		throw Exception(SYSFILE_OPEN_FAILED, msg);
	}
}

//******************************************************************************************
void AllocatedFile::ReleaseOpenLock(bool anylocker)
{
	try {
		open_lock->Release(anylocker);
	}
	catch (Exception& e) {
		if (e.Code() == UTIL_RESOURCE_NOT_HELD) 
			throw Exception(SYSFILE_CLOSE_FAILED, 
				std::string("File is not open: ").append(dd));
		else 
			throw;
	}
}



//******************************************************************************************
#ifdef DEBUG_V225_OKTECH
void AllocatedFile::DumpInstanceData(void* vf)
{
	util::BBStdioFile* pf = (util::BBStdioFile*) vf;
	util::BBStdioFile& f = *pf;

	f.WriteLine(std::string("      dd      = ").append(dd));
	f.WriteLine(std::string("      dsn     = ").append(dsn));
	f.WriteLine(std::string("      file_id = ").append(util::IntToString(file_id)));
	f.WriteLine(std::string("      alias   = ").append(alias));
	f.WriteLine(std::string("      pending = ").append(util::IntToString(pending)));
}

//************************************************
void AllocatedFile::DumpStaticData(void* vf)
{
	util::BBStdioFile* pf = (util::BBStdioFile*) vf;
	util::BBStdioFile& f = *pf;

	f.WriteLine("");
	f.WriteLine("AllocatedFile static data");
	f.WriteLine("-------------------------");

	f.WriteLine("dd lookup:");
	std::map<std::string, int>::const_iterator i;
	for (i = dd_directory.begin(); i != dd_directory.end(); i++) {
		f.WriteLine(std::string("  ")
			.append(util::PadRight(util::IntToString(i->second), ' ', 5))
			.append(i->first));
	}
	f.WriteLine("dsn lookup:");
	for (i = dsn_directory.begin(); i != dsn_directory.end(); i++) {
		f.WriteLine(std::string("  ")
			.append(util::PadRight(util::IntToString(i->second), ' ', 5))
			.append(i->first));
	}
	f.WriteLine("object directory:");
	for (size_t x = 0; x < all_objects.size(); x++) {
		AllocatedFile* a = all_objects[x];

		f.WriteLine(std::string("  ")
			.append(util::PadRight(util::UlongToString(x), ' ', 5))
			.append("*").append(util::IntToString((int)a)));
		
		if ((int)a != 0 && (int)a != 1)
			a->DumpInstanceData(vf);
	}
}

#endif

} //close namespace


