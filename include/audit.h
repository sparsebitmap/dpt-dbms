//****************************************************************************************
//The system audit trail.
//There should be a single one of these objects used by the whole system.  Much access is
//via Router::Issue(msgnum), but code in higher-level modules writes lines directly to the 
//file during all sorts of processing, for example during the AUDIT UL statement, during 
//terminal IO as controlled by LAUDIT, CAUDIT etc., when writing statistics lines...
//****************************************************************************************

#if !defined(BB_AUDIT)
#define BB_AUDIT

#include <vector>

#include "lockable.h"
#include "filehandle.h"

namespace dpt {

//****************************************************************************************
class CoreServices;
namespace util {
	class LineOutput;
	class StdIOLineOutput;
}
	
//****************************************************************************************
class Audit {
	FileHandle alloc_handle;

	std::string filename;
	util::StdIOLineOutput* thefile;
	Lockable write_lock;

//	time_t audit_previous_second;
//	int audit_second_count;

	int audctl;
	int audkeep;
	enum {
		audctl_show_date=1,
		audctl_show_time=2,
		audctl_show_millisecs=4,
		audctl_show_usernum=8,
		audctl_show_linetype=16
	};

	friend class CoreServices;
	void WriteOutputLine(const std::string& linedata, const int usernum, const char* linetype);
	Audit(const std::string& filename, util::LineOutput* secondary_audit, 
		const std::string&, int = audctl_show_date, int = 0);
	~Audit();

	std::string MakeScanDateString(const std::string&);
	void ExtractLines(std::vector<std::string>*, const std::string&, const std::string&, 
						const std::string&, bool, int);
};

}	//close namespace

#endif
