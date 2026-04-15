
#ifndef BB_CHECKPT
#define BB_CHECKPT

#include "lockable.h"
#include "rawpage.h"
#include "filehandle.h"

namespace dpt {

//**********************************************************************************
class RawPageData;
class DatabaseServices;
class DatabaseFile;

class CheckpointFileRecord;
class CheckpointHeaderRecord;
class CheckpointAllocatedFileRecord;
class CheckpointPreImageRecord;

class ProgressReportableActivity;

class IODev;
namespace util { class LineOutput; }

//**********************************************************************************
class CheckpointFile {

	static CheckpointFile* the_one;

	FileHandle alloc_handle;

	Lockable cplock;
	std::string fname;
	int stdio_handle;

	int num_pre_images;

	int WriteRecord_S(DatabaseServices*, CheckpointFileRecord*);

public:
	CheckpointFile();
	~CheckpointFile();
	static CheckpointFile* Object() {return the_one;}

	_int64 FLength();
	_int64 Tell();
	void Rewind();
	int NumPreImages() {return num_pre_images;}
	CheckpointFileRecord* ReadIn(void*);
	void Reinitialize(DatabaseServices*, time_t);

	int WriteAllocatedFileInfo(DatabaseServices*, const char*, const char*);
	void EndOfAllocatedFileInfo();
	int WritePreImage(DatabaseServices*, RawPageData*, const char*, int);
	int WriteEOT(DatabaseServices*, time_t);

#ifdef _BBHOST
	void Dump(IODev*);
#else
	void Dump(util::LineOutput*);
#endif

	static unsigned int __stdcall WorkerThread(void*);
	void WorkerThreadPreScan();
	void WorkerThreadOpenFiles();
	void WorkerThreadBackup();
	void WorkerThreadRollback();
	void WorkerThreadRestore();
};









//**********************************************************************************
//Contents of the above file - several record types
//**********************************************************************************
class CheckpointFileRecord {
protected:
	static const int RECTYPE_LEN;
	static char buff[DBPAGE_SIZE + 100];

	char* Initialize(int l, char r);

public:
	virtual CheckpointHeaderRecord* CastToHeader() {return NULL;}
	virtual CheckpointAllocatedFileRecord* CastToAllocatedFile() {return NULL;}
	virtual CheckpointPreImageRecord* CastToPreImage() {return NULL;}

	virtual int FormatBuffer() = 0;
	const char* Buffer() {return &buff[0];}

	static CheckpointFileRecord* ReadIn(int, const std::string&, void*);
	virtual std::string Info() = 0;
};

//**********************************************************************************
class CheckpointHeaderRecord : public CheckpointFileRecord {
	static const int TIMESTAMP_LEN;
	static const int TOTAL_LEN;
	static const char RECTYPE;

	time_t timestamp;

	int FormatBuffer();

public:
	CheckpointHeaderRecord* CastToHeader() {return this;}

	CheckpointHeaderRecord(time_t t) : timestamp(t) {}
	static CheckpointHeaderRecord* Construct(int, const std::string&);

	time_t GetTimeStamp() {return timestamp;}
	std::string Info();
};

//**********************************************************************************
class CheckpointAllocatedFileRecord : public CheckpointFileRecord {
	static const int DDNAME_LEN;
//	static const int TOTAL_LEN;
	static const char RECTYPE;

	const char* ddname;
	const char* dsname;

	int FormatBuffer();

public:
	CheckpointAllocatedFileRecord* CastToAllocatedFile() {return this;}

	CheckpointAllocatedFileRecord(const char* dd, const char* dsn) 
		: ddname(dd), dsname(dsn) {}
	static CheckpointAllocatedFileRecord* Construct(int, const std::string&);

	const char* GetDD() {return ddname;}
	const char* GetDSN() {return dsname;}
	std::string Info();
};

//**********************************************************************************
class CheckpointPreImageRecord : public CheckpointFileRecord {
	static const int FILENAME_LEN;
	static const int PAGENUM_LEN;
	static const int TOTAL_LEN;
	static const char RECTYPE;

	const char* filename;
	int pagenum;
	RawPageData* data;

	int FormatBuffer();

public:
	CheckpointPreImageRecord* CastToPreImage() {return this;}

	CheckpointPreImageRecord(RawPageData* d, const char* f, int p)
		: filename(f), pagenum(p), data(d) {}
	static CheckpointPreImageRecord*  Construct(int, const std::string&, void*);

	const char* GetFileName() {return filename;}
	int GetPageNum() {return pagenum;}
	RawPageData* GetData() {return data;}
	std::string Info();
};

//**********************************************************************************
int ChkpBackupProgressReporter(const int, const ProgressReportableActivity*, void*);

} //close namespace

#endif
