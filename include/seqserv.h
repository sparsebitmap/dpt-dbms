
#if !defined(BB_SEQSERV)
#define BB_SEQSERV

#include <string>
#include <set>

#include "lockable.h"
#include "filehandle.h"
#include "apiconst.h"

namespace dpt {

class DatabaseServices;
class SequentialFileView;

//****************************************************************************************
class SequentialFileServices {
	DatabaseServices* dbapi;
	static ThreadSafeLong instances;
	static std::string seqtemp;
	static std::string seqtemp_path;

	std::set<SequentialFileView*> open_directory;

	friend class DatabaseServices;
	SequentialFileServices(DatabaseServices*);
	~SequentialFileServices();

	FileHandle GetHandle(const std::string&, bool);

	static std::string MakeNewTempFileName(const std::string& = ".TXT");

public:
	DatabaseServices* DBAPI() {return dbapi;}

	void Allocate(const std::string&, const std::string&, FileDisp = FILEDISP_OLD, 
		int = -1, char = ' ', unsigned int = 0, const std::string& = std::string(), bool = false);
	void Free(const std::string& dd);

	SequentialFileView* OpenSeqFile(const std::string&, bool for_write);
	void CloseSeqFile(SequentialFileView*);

	//Handy place for general processing store temporary files and have them cleared down
	static std::string SeqTempDir() {return seqtemp_path;}

	//For miscellaneous quick use-and-discard situations - e.g. during $EMAIL
	static std::string CreateNewTempFile(const std::string& extn = ".TXT", int* leaveopen_handle = NULL);
	static int DeleteOldTempFiles(const std::string& extn = ".TXT", int agesecs = 0);
};

} //close namespace

#endif
