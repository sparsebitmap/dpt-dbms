
#include "stdafx.h"

#include "access.h"

//Utils
#include "pattern.h"
#include "dataconv.h"
#include "iowrappers.h"
#include "hash.h"
#include "winutil.h"
//API tiers
#include "sysfile.h"
#include "parmref.h"
#include "parmvr.h"
#include "core.h"
#ifdef _BBDBAPI
#include "dbctxt.h"
#endif
//Diagnostics
#include "except.h"
#include "msg_core.h"
#include "msg_util.h"

namespace dpt {

bool AccessController::created = false;

//User zero and various others initialize their account to this to start with.
//This is an illegal userid to be defined in LOGCTL so you can't spoof it.
std::string AccessController::fullprivs_account = "+SYSTEM+";
//This is a bit more klugey. Used to give daemons their caller's account.
std::string AccessController::system_account_prefix_string = "++SYSTEMACCOUNTKLUGE++";

static Hash_SHA1 hasher;
static int digest_length = Hash_SHA1::DIGEST_LENGTH;

static _int64 NO_SPECIFIC_FILE_PRIVS = -1i64;

//*****************************************************************************************
AccessController::AccessController(CoreServices* caller)
{
	if (created) 
		throw Exception(API_SINGLETON, "There can be only one access control file object");

	created = true;
	last_write_time_t = 1;
	cached_sys_dd_array_size = 0;

	filename = "dptstat.dat";
	alloc_handle = SystemFile::Construct("+DPTSTAT", filename, BOOL_EXCL);

	char* buff = NULL;
	try {

		//Create an empty file if it's not there
		try {
			file = new util::BBStdioFile(filename.c_str(), util::STDIO_OLD);
		}
		catch (...) {
			file = new util::BBStdioFile(filename.c_str(), util::STDIO_NEW);
			RewriteFile(caller);
			return;
		}

		//Otherwise read the existing file data into our local cache
		int flength = file->FLength();

		//Overall length sanity check (header line is datetime + user name + CRLF)
		if (flength < digest_length + 30) 
			ThrowCorruptData();

		//Read it all in
		buff = new char[flength];
		file->Read(buff, flength);

		ptr = buff;
		end = buff + (flength - digest_length); //checksum position

		//Get header line information: last update time
		last_write_time = std::string(ptr, 24);
		AdvanceFilePointer(24);

		//...last update user
		int namelen = *(reinterpret_cast<int*>(ptr));
		AdvanceFilePointer(4);
		last_write_user = std::string(ptr, namelen);
		AdvanceFilePointer(namelen);
		AdvanceFilePointer(2);

		//Then read each user entry line
		while (ptr < end) {

			//User name
			int namelen = *(reinterpret_cast<int*>(ptr));
			AdvanceFilePointer(4);
			std::string name(ptr, namelen);
			AdvanceFilePointer(namelen);

			CachedAccountInfo info;

			//Password digest
			info.pwhash = std::string(ptr, digest_length);
			AdvanceFilePointer(digest_length);

			//Privileges
			info.privs = *(reinterpret_cast<unsigned int*>(ptr));
			AdvanceFilePointer(4);

			//Arbitrary number of file-specific [name+privs], each prefixed by "F"
			while (*ptr == 'F') {
				AdvanceFilePointer(1);

				int fnamelen = *(reinterpret_cast<int*>(ptr));
				AdvanceFilePointer(4);
				std::string fname(ptr, fnamelen);
				AdvanceFilePointer(fnamelen);

				unsigned int fprivs = *(reinterpret_cast<unsigned int*>(ptr));
				AdvanceFilePointer(4);

				info.fileprivs_by_filename[fname] = fprivs;
			}

			//Cache the info locally for fast lookup
			cached_account_table[name] = info;

			//Go past crlf
			AdvanceFilePointer(2);
		}

		if (ptr != end)
			ThrowCorruptData();

		//Finally validate the checksum
		std::string gotsum = std::string(ptr, digest_length);
		std::string calcsum = CalcCheckSum(std::string(buff, flength - digest_length));
		if (gotsum != calcsum)
			ThrowCorruptData();

		delete[] buff;
	}
	catch (...) {
		if (buff)
			delete[] buff;
		delete file;
		SystemFile::Destroy(alloc_handle);
		throw;
	}
}

//****************************************
void AccessController::ThrowCorruptData()
{
	throw Exception(ACCESS_CONTROL_BADDATA, "Access control file (dptstat) is corrupt");
}

//****************************************
AccessController::~AccessController()
{
	delete file;
	SystemFile::Destroy(alloc_handle);

	created = false;
}

//****************************************
std::string AccessController::CalcCheckSum(const std::string& s)
{
	//Make it a little bit more obscure than just a straight hash on the data
	std::string dummy = std::string("DUMMY").append(s).append("DUMMY");

	return hasher.Perform(dummy.c_str(), dummy.length());
}

//****************************************
void AccessController::RewriteFile(CoreServices* caller)
{
	time(&last_write_time_t);
	last_write_time = win::GetCTime(last_write_time_t);
	last_write_user = caller->GetUserID();

	//Start with a header line
	std::string data = last_write_time;
	int namelen = last_write_user.length();
	data.append(std::string(reinterpret_cast<char*>(&namelen), 4));
	data.append(last_write_user);
	data.append("\r\n");

	//Then all the entry data
	std::map<std::string, CachedAccountInfo>::iterator i;
	for (i = cached_account_table.begin(); i != cached_account_table.end(); i++) {

		//Account name
		int namelen = i->first.length();
		data.append(std::string(reinterpret_cast<char*>(&namelen), 4));
		data.append(i->first);

		CachedAccountInfo& info = i->second;

		//PW hash
		data.append(info.pwhash);

		//Privs
		data.append(std::string(reinterpret_cast<char*>(&info.privs), 4));

		//File-specific privs
		std::map<std::string, unsigned int>::iterator pi;
		for (pi = info.fileprivs_by_filename.begin(); pi != info.fileprivs_by_filename.end(); pi++) {
			data.append("F");
			
			int fnamelen = pi->first.length();
			data.append(std::string(reinterpret_cast<char*>(&fnamelen), 4));
			data.append(pi->first);

			data.append(std::string(reinterpret_cast<char*>(&pi->second), 4));
		}

		data.append("\r\n");
	}

	//And a final check sum
	data.append(CalcCheckSum(data));

	//Overwrite the file contents
	file->Chsize(data.length());
	file->Seek(0);
	file->Write(data.c_str(), data.length());
}

//****************************************
AccessController::CachedAccountInfo* AccessController::LocateCachedInfo
(const std::string& name, bool throwit)
{
	std::map<std::string, CachedAccountInfo>::iterator i = cached_account_table.find(name);
	if (i != cached_account_table.end())
		return &(i->second);

	if (throwit)
		throw Exception(ACCOUNT_DOES_NOT_EXIST, std::string("Account is not defined: ")
				.append(name).append(" (check case?)"));
	else
		return NULL;
}

//****************************************
unsigned int AccessController::GetAccountPrivs(const std::string& name) 
{
	LockingSentry ls(&lock);

	return LocateCachedInfo(name)->privs;
}

//****************************************
bool AccessController::CheckAccountPassword
(const std::string& name, const std::string& checkee)
{
	LockingSentry ls(&lock);

	std::string checkee_hash = hasher.Perform(checkee.c_str(), checkee.length());
	return (LocateCachedInfo(name)->pwhash == checkee_hash);
}

//****************************************
void AccessController::CreateAccount
(CoreServices* caller, const std::string& name)
{
	LockingSentry ls(&lock);

	if (LocateCachedInfo(name, false))
		throw Exception(ACCOUNT_ALREADY_EXISTS, std::string("Account already exists: ").append(name));

	//Validate
	const ParmRefInfo& ref = caller->GetViewerResetter()->GetRefTable()->GetRefInfo("USERID");
	util::Pattern patt(ref.validation_pattern);
	if (!patt.IsLike(name))
		throw Exception(ACCESS_ACCOUNT_BADFORMAT, "Invalid format for account name");

	CachedAccountInfo info;

	//Set a default password here, although you would expect a better one to be
	//set almost immediately.  For example the LOGCTL command does that.
	static const std::string default_pw = "password";
	info.pwhash = hasher.Perform(default_pw.c_str(), default_pw.length());

	//Default privs is none
	info.privs = 0;

	cached_account_table[name] = info;

	RewriteFile(caller);
}

//****************************************
void AccessController::DeleteAccount
(CoreServices* caller, const std::string& name)
{
	LockingSentry ls(&lock);

	LocateCachedInfo(name);
	cached_account_table.erase(name);

	RewriteFile(caller);
}

//****************************************
void AccessController::ChangeAccountPassword
(CoreServices* caller, const std::string& name, const std::string& pw)
{
	LockingSentry ls(&lock);

	EnsurePasswordIsValidFormat(pw);

	LocateCachedInfo(name)->pwhash = hasher.Perform(pw.c_str(), pw.length());

	RewriteFile(caller);
}

//****************************************
void AccessController::ChangeAccountPrivs
(CoreServices* caller, const std::string& name, unsigned int p)
{
	LockingSentry ls(&lock);

	LocateCachedInfo(name)->privs = p;

	RewriteFile(caller);
}

//****************************************
std::vector<std::string> AccessController::GetAllAccountNames()
{
	LockingSentry ls(&lock);
	std::vector<std::string> result;

	std::map<std::string, CachedAccountInfo>::iterator i;
	for (i = cached_account_table.begin(); i != cached_account_table.end(); i++)
		result.push_back(i->first);

	return result;
}

//****************************************
std::vector<std::string> AccessController::GetAllAccountHashes()
{
	LockingSentry ls(&lock);
	std::vector<std::string> result;

	std::map<std::string, CachedAccountInfo>::iterator i;
	for (i = cached_account_table.begin(); i != cached_account_table.end(); i++)
		result.push_back(i->second.pwhash);

	return result;
}

//****************************************
std::vector<unsigned int> AccessController::GetAllAccountPrivs()
{
	LockingSentry ls(&lock);
	std::vector<unsigned int> result;

	std::map<std::string, CachedAccountInfo>::iterator i;
	for (i = cached_account_table.begin(); i != cached_account_table.end(); i++)
		result.push_back(i->second.privs);

	return result;
}

//****************************************
std::string AccessController::ToStringAbbrev(AccessLevel privs)
{
	if (privs == PRIV_SYSMGR) return "SYSMGR";
	if (privs == PRIV_SYSADMIN) return "SYSADMIN";
	if (privs == PRIV_SUPER) return "SUPER";
	if (privs == PRIV_FILEMGR) return "FILEMGR";
	return "NONE";
}

//****************************************
bool AccessController::EnsureSystemManager
(unsigned int privs, const char* usage, bool throwit)
{
	if ( (privs & PRIV_SYSMGR) != 0)
		return true;

	if (throwit)
		throw Exception(ACCESS_INSUFFICIENT_PRIVS, std::string(usage)
			.append(" requires system manager privileges"));
	else
		return false;
}

//****************************************
bool AccessController::EnsureSystemAdministrator
(unsigned int privs, const char* usage, bool throwit)
{
	if ( (privs & PRIV_SYSADMIN) != 0)
		return true;

	if (throwit)
		throw Exception(ACCESS_INSUFFICIENT_PRIVS, std::string(usage)
			.append(" requires system administrator privileges"));
	else
		return false;
}

//****************************************
bool AccessController::EnsureCanChangeFileParms
(unsigned int privs, bool throwit)
{
	if ( (privs & PRIV_RESET_FPARMS) != 0)
		return true;

	if (throwit)
		throw Exception(ACCESS_INSUFFICIENT_PRIVS, 
			"Resetting file parameters is a privileged operation");
	else
		return false;
}

//****************************************
bool AccessController::EnsureCanChangeOwnPassword
(unsigned int privs, const char* usage, bool throwit)
{
	if ( (privs & PRIV_CHANGE_PASSWORD) != 0)
		return true;

	if (throwit)
		throw Exception(ACCESS_INSUFFICIENT_PRIVS, std::string(usage)
			.append(" requires password-change privileges"));
	else
		return false;
}

//****************************************
bool AccessController::EnsurePasswordIsValidFormat
(const std::string& pw, const char* msg, bool throwit)
{
	if (IsAValidPassword(pw))
		return true;

	if (throwit)
		throw Exception(ACCESS_PASSWORD_BADFORMAT, msg);
	else
		return false;
}

//--------------------------------------------------------------------------------------
//File privs functionality
//--------------------------------------------------------------------------------------
#ifdef _BBDBAPI

//****************************************
void AccessController::ChangeAccountFilePrivs
(CoreServices* caller, const std::string& name,
		const std::vector<std::string>& filenames, 
		const std::vector<unsigned int>& fileprivs)
{
	LockingSentry ls(&lock);

	if (filenames.size() != fileprivs.size())
		throw Exception(ACCESS_CONTROL_BADDATA, "Bug: file privs size mismatch");

	CachedAccountInfo* info = LocateCachedInfo(name);
	
	//Clear their existing file privileges
	info->fileprivs_by_filename.clear();

	//Just add back specific ones as supplied now
	for (size_t x = 0; x < filenames.size(); x++)
	{
		const std::string& filename = filenames[x];
		unsigned int privs = fileprivs[x];

		//Note that we always ensure that our memory data structure for each user
		//has a slot for every file allocated to the system. Hence just set it here.
		info->fileprivs_by_filename[filename] = privs;
	}

	//Keep the lookup by FID in sync
	info->RefreshFidTable(cached_sys_dd_directory, cached_sys_dd_array_size);

	RewriteFile(caller);
}

//****************************************
std::map<std::string, unsigned int> AccessController::GetAccountFilePrivs(const std::string& name) 
{
	LockingSentry ls(&lock);

	return LocateCachedInfo(name)->fileprivs_by_filename;
}

//****************************************
unsigned int AccessController::GetAccountFilePrivs(const std::string& name, int fid) 
{
	LockingSentry ls(&lock);

	return LocateCachedInfo(name)->FilePrivs(fid);
}

//****************************************
unsigned int AccessController::CachedAccountInfo::FilePrivs(int fid) 
{
	//Privs for the specified file, if any defined
	if (fid != -1) {
		_int64 fileprivs = fileprivs_by_fid[fid];
		if (fileprivs != NO_SPECIFIC_FILE_PRIVS)
			return fileprivs;
	}

	//Basic privileges are returned as a default
	return privs;
}

//****************************************
std::vector<std::map<std::string, unsigned int> > AccessController::GetAllAccountFilePrivs()
{
	LockingSentry ls(&lock);
	std::vector<std::map<std::string, unsigned int> > result;

	std::map<std::string, CachedAccountInfo>::iterator i;
	for (i = cached_account_table.begin(); i != cached_account_table.end(); i++)
		result.push_back(i->second.fileprivs_by_filename);

	return result;
}

//****************************************
//This function does as its lengthy name implies. It represents a trade-off
//between having super-fast access control checks during regular use of the
//system, and having system start-up procssing go reasonably fast on systems
//where many files get allocated at start-up. As usual we take the view that
//longer start-up processing is worth fast general usage. In any case it's
//not a massive amount of work.
//****************************************
void AccessController::RefreshAllAccountsCachedFileIDsOnFileAllocate
(const std::map<std::string, int>& sys_dd_directory, size_t sys_dd_array_size)
{
	LockingSentry ls(&lock);

	//Refresh our cached version of the system's filaname/FID lookup table
	cached_sys_dd_directory = sys_dd_directory;

	//Note that system FIDs are allocated into a table which is often larger than the 
	//actual number of files allocated. FREE commands do not compact it - they just
	//leave a slot available for the next ALLOCATE. We need the full size so we can
	//work with arrays that size.
	cached_sys_dd_array_size = sys_dd_array_size;

	//Go through all cached privs and resize the fid (fast-access)
	//version of their file-specific privs.
	std::map<std::string, CachedAccountInfo>::iterator i;
	for (i = cached_account_table.begin(); i != cached_account_table.end(); i++)
	{
		i->second.RefreshFidTable(sys_dd_directory, sys_dd_array_size);
	}
}

//****************************************
void AccessController::CachedAccountInfo::RefreshFidTable
	(const std::map<std::string, int>& sys_dd_directory, size_t sys_dd_array_size)
{
	using AccessController;

	//Initialize the account's fid/privs array to system fid table size.
	fileprivs_by_fid.clear();
	fileprivs_by_fid.resize(sys_dd_array_size, NO_SPECIFIC_FILE_PRIVS);

	//Add back any specific file privs the account has defined. We are doing a kind
	//of join here, and it will generally be the case that the account recs have
	//just a few files defined compared to the total number of allocated DDs. Hence
	//this orientation for the loop is best.
	std::map<std::string, unsigned int>::iterator pi;
	for (pi = fileprivs_by_filename.begin(); pi != fileprivs_by_filename.end(); pi++)
	{
		const std::string& filename = pi->first;
		unsigned int fileprivs = pi->second;

		//Is this file allocated to the system? If not then none of the access-control 
		//processing will query for its privileges. So we can leave the default flag 
		//there. Also there's no need to refresh this table during the FREE command.
		std::map<std::string, int>::const_iterator fi;
		fi = sys_dd_directory.find(filename);

		if (fi != sys_dd_directory.end())
		{
			//Yes the file is allocated - add the corresponding privs to our array. 
			short fid = fi->second;
			fileprivs_by_fid[fid] = fileprivs;
		}
	}
}

//****************************************
bool AccessController::EnsureFileManager
(unsigned int privs, SingleDatabaseFileContext* context,
 const char* usage, bool throwit)
{
	if ( (privs & PRIV_FILEMGR) != 0)
		return true;

	if (throwit)
		throw Exception(ACCESS_INSUFFICIENT_PRIVS, std::string(usage)
			.append(" requires file manager privileges in the target file (")
			.append(context->GetShortName()).append(")"));
	else
		return false;
}

//****************************************
bool AccessController::EnsureCanReadData
(unsigned int privs, SingleDatabaseFileContext* context,
 const char* usage, bool throwit)
{
	if ( (privs & PRIV_DATA_READING) != 0)
		return true;

	if (throwit)
		throw Exception(ACCESS_INSUFFICIENT_PRIVS, std::string(usage)
			.append(" requires data-read privileges in the target file (")
			.append(context->GetShortName()).append(")"));
	else
		return false;
}

//****************************************
bool AccessController::EnsureCanUpdateData
(unsigned int privs, SingleDatabaseFileContext* context,
 const char* usage, bool throwit)
{
	if ( (privs & PRIV_DATA_UPDATES) != 0)
		return true;

	if (throwit)
		throw Exception(ACCESS_INSUFFICIENT_PRIVS, std::string(usage)
			.append(" requires data-update privileges in the target file (")
			.append(context->GetShortName()).append(")"));
	else
		return false;
}


#endif

} //close namespace


