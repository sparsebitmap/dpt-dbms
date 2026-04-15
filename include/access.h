//****************************************************************************************
// Management of the password/privileges file. Note that runtime access-control checks
// go via CoreServices::GetEffectiveUserPrivs();
//****************************************************************************************

#if !defined(BB_ACCESS)
#define BB_ACCESS

#include <string> 
#include <vector> 
#include <map>

#include "filehandle.h"
#include "lockable.h"

namespace dpt {

class CoreServices;
namespace util {class BBStdioFile;}

typedef unsigned int AccessLevel;
const AccessLevel PRIV_NONE             = 0x00000000;
const AccessLevel PRIV_SYSMGR           = 0x00000008;
const AccessLevel PRIV_CHANGE_PASSWORD  = 0x00000010;
const AccessLevel PRIV_SYSADMIN         = 0x00000040;
const AccessLevel PRIV_SUPER            = 0x00000080;
const AccessLevel PRIV_ALL              = 0xffffffff;

#ifdef _BBDBAPI
const AccessLevel PRIV_RESET_FPARMS     = 0x00000100;
const AccessLevel PRIV_DATA_READING     = 0x00000400;
const AccessLevel PRIV_DATA_UPDATES     = 0x00002000;
const AccessLevel PRIV_FILEMGR          = 0x00008000;

class SingleDatabaseFileContext;
#endif

//------------------------------------------------------------------------------------
class AccessController {
	static bool created;
	static std::string fullprivs_account;
	static std::string system_account_prefix_string;

	util::BBStdioFile* file;
	std::string filename;
	FileHandle alloc_handle;
	Lockable lock;

	time_t last_write_time_t;
	std::string last_write_time;
	std::string last_write_user;

	//Should make an intermediate class really.
	friend class CoreServices;
	friend class Session;
	struct CachedAccountInfo {
		std::string pwhash;
		unsigned int privs;
			
		#ifdef _BBDBAPI
		//V3.03.
		std::map<std::string, unsigned int> fileprivs_by_filename;
		std::vector<_int64> fileprivs_by_fid;
		void RefreshFidTable(const std::map<std::string, int>&, size_t);
		unsigned int FilePrivs(int fid);
		#endif
	};
	std::map<std::string, CachedAccountInfo> cached_account_table;

	CachedAccountInfo* LocateCachedInfo(const std::string&, bool = true);

	char* ptr;
	char* end;
	void AdvanceFilePointer(int delta) {ptr += delta; if (ptr > end) ThrowCorruptData();}

	void ThrowCorruptData();
	void RewriteFile(CoreServices*);
	std::string CalcCheckSum(const std::string&);

public:
	AccessController(CoreServices*);
	~AccessController();

	void CreateAccount(CoreServices*, const std::string&);
	void DeleteAccount(CoreServices*, const std::string&);

	void ChangeAccountPassword(CoreServices*, const std::string&, const std::string&);
	void ChangeAccountPrivs(CoreServices*, const std::string&, unsigned int);

	bool CheckAccountPassword(const std::string&, const std::string&);
	unsigned int GetAccountPrivs(const std::string&);

	//Change privs without logging on again. 
	//RC = >=0: previous privs. RC=-1: invalid PW, RC=-2: bad userid.
	_int64 ChangeUserAccount(CoreServices* caller, 
		const std::string& newaccount_userid, const std::string& newaccount_password);

	std::vector<std::string> GetAllAccountNames();
	std::vector<unsigned int> GetAllAccountPrivs();
	std::vector<std::string> GetAllAccountHashes();

	const std::string& LastUpdateUser() {LockingSentry ls(&lock); return last_write_user;}
	const std::string& LastUpdateTime() {LockingSentry ls(&lock); return last_write_time;}

	static bool EnsureSystemManager(unsigned int privs, 
									const char* usage, bool throwit = true);
	static bool EnsureSystemAdministrator(unsigned int privs, 
									const char* usage, bool throwit = true);
	static bool EnsureCanChangeFileParms(unsigned int privs, bool throwit = true);
	static bool EnsureCanChangeOwnPassword(unsigned int privs, 
									const char* usage, bool throwit = true);
	static bool EnsurePasswordIsValidFormat(const std::string& pw, 
					const char* = "Password is not in a valid format", bool throwit = true);

	static std::string ToStringAbbrev(AccessLevel);
	static bool IsAValidPassword(const std::string& pw) {
		return pw.length() > 0 && pw.find(':') == std::string::npos;}

	//--------------------------------------------------------------------
	//V3.03 this stuff. We are in the core layer here, but need some extra
	//funcs for file privileges. Too lazy to extend the class up properly!
	//--------------------------------------------------------------------
#ifdef _BBDBAPI

	//Cache system file IDs. This lets us do access control checks in 
	//virtually no time even for file operations, which WILL be frequent.
	std::map<std::string, int> cached_sys_dd_directory;
	size_t cached_sys_dd_array_size;
	void RefreshAllAccountsCachedFileIDsOnFileAllocate(const std::map<std::string, int>&, size_t);

	void ChangeAccountFilePrivs(CoreServices*, const std::string&, 
		const std::vector<std::string>&, const std::vector<unsigned int>&); 

	std::map<std::string, unsigned int> GetAccountFilePrivs(const std::string&);
	unsigned int GetAccountFilePrivs(const std::string&, int fid);

	std::vector<std::map<std::string, unsigned int> > GetAllAccountFilePrivs();

	static bool EnsureFileManager(unsigned int privs, SingleDatabaseFileContext*,
									const char* usage, bool throwit = true);
	static bool EnsureCanReadData(unsigned int privs, SingleDatabaseFileContext*,
									const char* usage, bool throwit = true);
	static bool EnsureCanUpdateData(unsigned int privs, SingleDatabaseFileContext*,
									const char* usage, bool throwit = true);

#endif
};


} //close namespace

#endif
