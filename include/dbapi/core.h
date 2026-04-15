//****************************************************************************************
// The lowest tier of host functionality.  The 3 levels are:
// - Core    : Parameters, statistics, messaging, the audit trail, user initiation.
// - Database: All database work, structured locks and waits.  MVS style sequential IO.
// - Session : User IO, UL compiler/debugger, commands, APSY, web server.
//   (The session level does not exist in the API library - user code replaces this).
//****************************************************************************************

#if !defined(BB_API_CORE)
#define BB_API_CORE

#include <string>
#include <vector>

#include "msgroute.h"
#include "parmvr.h"
#include "statview.h"
#include "access.h"
#include "const_file.h"

namespace dpt {

class CoreServices;
namespace util {
	class LineOutput;
}

class APICoreServices {
public:
	CoreServices* target;
	APICoreServices(CoreServices* t) : target(t) {}
	APICoreServices(const APICoreServices& t) : target(t.target) {}
	//-----------------------------------------------------------------------------------

	//Service objects
	APIMsgRouter GetRouter();
	APIViewerResetter GetViewerResetter();
	APIStatViewer GetStatViewer();
	APIAccessController GetAccessController();

	void AuditLine(const std::string& text, const char* linetype);
	void ExtractAuditLines(std::vector<std::string>* result, const std::string& from, 
			const std::string& to, const std::string& patt, bool pattcase = false, int maxlines = 0);

	//System/user startup/closedown
	static void Quiesce(const APICoreServices& caller);
	static void Unquiesce(const APICoreServices& caller);
	static bool IsQuiesceing();
	static void ScheduleForBump(int usernum, bool user_0_too = false);
	bool IsScheduledForBump();

	//V2.25.  Results valid at time of call (i.e. files might get freed)
	std::vector<std::string> GetAllocatedFileNames(FileType = FILETYPE_ALL);

	//Functions relating to all users
	static std::vector<int> GetUsernos(const std::string& = "ALL");
	static int GetUsernoOfThread(const unsigned int threadid);

	//Functions relating to a specific user
	const std::string& GetUserID();
	util::LineOutput* Output();
	int GetUserNo();
	unsigned int GetThreadID();
	int SetWT(int);
	int GetWT();

	void Tick(const char* activity); //checks for bump

	//Callbacks
	bool InteractiveYesNo(const std::string& prompt, bool default_response);
	void RegisterInteractiveYesNoFunc(bool (*custom_func) (const std::string&, bool, void*));
	void RegisterInteractiveYesNoObj(void* custom_object);
};

} //close namespace

#endif
