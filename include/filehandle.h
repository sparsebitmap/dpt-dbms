//***************************************************************************************
// These ensure file pointers remain valid (i.e. other users don't free files) 
// while they're used.  
// NB.  Not the same as 'opening' the file - functions like MONITOR often need to 
// process files that are 'open' in exclusive mode on other threads.
//***************************************************************************************

#if !defined(BB_FILEHANDLE)
#define BB_FILEHANDLE

#include <string>
#include "const_file.h"

namespace dpt {

class AllocatedFile;


//********************************
class FileHandle {
	AllocatedFile* afile;
	bool type;
	mutable bool enabled;

	void Release();
	void CopyFrom(const FileHandle&);

	friend class SingleDatabaseFileContext;
	friend class Recovery;
	int FileID();

public:
	FileHandle() : enabled(false) {}
	FileHandle(AllocatedFile*, bool);
	~FileHandle() {Release();}

	FileHandle(const FileHandle& h) : enabled(false) {CopyFrom(h);}
	FileHandle& operator=(const FileHandle& h) {CopyFrom(h); return *this;}

	const std::string& GetDD() const;
	const std::string& GetDSN() const;
	const std::string& GetAlias() const;
	FileType GetType() const;
	AllocatedFile* GetFile() const {return afile;}

	bool IsEnabled() {return enabled;}
	void Clear() {Release(); afile = NULL;}
	void CommitAllocation();
	void StageForFree();
};

} //close namespace

#endif
