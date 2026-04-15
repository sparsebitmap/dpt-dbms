//*****************************************************************************************
//The system uses these internally when it has already created an object, such as the
//audit trail, just to register the OS file as off limits for other kinds of processing
//such as USE.
//*****************************************************************************************

#if !defined(BB_SYSFILE)
#define BB_SYSFILE

#include "file.h"
#include "filehandle.h"

namespace dpt {

class SystemFile : public AllocatedFile {

	bool isopen;

	SystemFile(const std::string&, std::string&, bool);
	~SystemFile();

public:
	static FileHandle Construct(const std::string&, std::string&, bool);
	static void Destroy(FileHandle&);

	void Open(bool);
	void Close();
};

} //close namespace

#endif
