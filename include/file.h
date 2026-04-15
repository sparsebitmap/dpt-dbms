/***************************************************************************************
This is a virtual class from which a handful of other classes inherit.  The common 
characteristic is that all these classes are abstractions for an OS file entity. The
different ways Baby204 has of viewing files are:
- Database files
- Sequential files used for USE, WRITE and READ image
- Procedures
- All other files such as checkpoint, audit etc.
The functionality embodied in this base class helps ensure consistency of access to
files allocated to the host application, both in terms of different usages, and use
by different user threads.  For example we would not want to programs to read or write
to/from the checkpoint file.
***************************************************************************************/

#if !defined(BB_FILE)
#define BB_FILE

#include <string>
#include <vector>
#include <map>
#include "const_file.h"
#include "filehandle.h"
#include "lockable.h"

namespace dpt {

class Resource;

class AllocatedFile {

	std::string dd;		//mainframe terminology - internal "shorthand" name
	std::string dsn;	//likewise - OS file name
	FileType type;
	int file_id;		//used internally for one or two things
	
	std::string alias;
	
	Sharable alloc_lock;
	Resource* open_lock;

protected:
	bool pending;		//during free/allocate

	//The index into this vector denotes the file id of a particular file.
	static std::vector<AllocatedFile*> all_objects;
	static std::map<std::string, int> dd_directory;
	static std::map<std::string, int> dsn_directory;
	static Sharable allocation_lock;

	static std::map<std::string, AllocatedFile*> aliases;

	AllocatedFile(const std::string&, std::string&, FileType, const std::string& = std::string());
	virtual ~AllocatedFile();

	//Called by derived objects' Open() to enqueue the whole file.  
	void TryForOpenLock(bool, bool = false, bool = false);
	void ReleaseOpenLock(bool = false);

	//Return general information - only valid via handle - see below.
	friend class FileHandle;
	const std::string& GetDDName() const {return dd;}
	const std::string& GetDSN() const {return dsn;}
	FileType GetType() const {return type;}

	void CommitAllocation();
	void StageForFree();
public:
	static void ListAllocatedFiles(std::vector<FileHandle>&, bool, FileType);
	static FileHandle FindAllocatedFile(const std::string&, bool, FileType);

#ifdef DEBUG_V225_OKTECH
	void DumpInstanceData(void*);
	static void DumpStaticData(void*);
#endif
};





} //close namespace

#endif
