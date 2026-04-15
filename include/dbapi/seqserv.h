//****************************************************************************************
// Interface to mainframe/M204 style sequential file IO.
//****************************************************************************************

#if !defined(BB_API_SEQSERV)
#define BB_API_SEQSERV

#include <string>

#include "apiconst.h"

namespace dpt {

class SequentialFileServices;
class APISequentialFileView;

class APISequentialFileServices {
public:
	SequentialFileServices* target;
	APISequentialFileServices(SequentialFileServices* t) : target(t) {}
	APISequentialFileServices(const APISequentialFileServices& t) : target(t.target) {}
	//-----------------------------------------------------------------------------------	

	void Allocate(const std::string& dd, const std::string& dsn, FileDisp = FILEDISP_OLD, 
		int lrecl = -1, char pad = ' ', unsigned int maxfilesize = 0, 
		const std::string& alias = std::string(), bool = false);
	void Free(const std::string& dd);

	//Mixed read and write on the same view is disallowed - hence parm 2.
	APISequentialFileView OpenView(const std::string& dd, bool for_write);
	void CloseView(const APISequentialFileView&);
};

} //close namespace

#endif
