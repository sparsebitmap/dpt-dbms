
#if !defined(BB_SEQFILE)
#define BB_SEQFILE

#include <string>

#include "apiconst.h"
#include "lockable.h"
#include "file.h"

#ifdef _BBHOST
#include "imagable.h"
#else
#include "lineio.h"
#endif

namespace dpt {

class SequentialFileServices; //V2.24 Friend Injection is non-standard - Roger M.
namespace util {
	class SharableLineIO;
}

//****************************************************************************************
class SequentialFile : public AllocatedFile {
	util::SharableLineIO* thefile;
	unsigned _int64 maxsize; //0 for no checks
	char pad;
	bool open_append;
	short lrecl;
	bool tempdsn;
	bool nocrlf;

	friend class SequentialFileServices;
	SequentialFile(const std::string&, std::string&, short, char, 
		unsigned int, FileDisp, bool&, const std::string&, bool, bool);
	~SequentialFile();

public:
	static FileHandle Construct(const std::string&, std::string&, short, char, 
		unsigned int, FileDisp, bool&, const std::string&, bool, bool);
	static bool Destroy(FileHandle&);

	util::SharableLineIO* BaseIO() {return thefile;}
	short Lrecl() {return lrecl;}
	char Pad() {return pad;}
	bool NoCRLF() {return nocrlf;}
	bool OpenAppend() {return open_append;}
	void MaxSizeCheck();

	_int64 Open(bool, bool = false);
	void Close() {ReleaseOpenLock();}
};

//****************************************************************************************
class SequentialFileView 
#ifdef _BBHOST
: public Imagable {
#else
: public util::LineInput, public util::LineOutput {
	bool reading;
	std::string write_buffer;
#endif

	SequentialFile* seqfile;
	SequentialFileServices* seqserv;
	_int64 pos;
	FileHandle hfile;

	friend class SequentialFileServices;
	SequentialFileView(SequentialFileServices*, SequentialFile*, bool); 
	~SequentialFileView() {seqfile->Close();}

	int ReadLine_D(char* c); 
	void NewLine_D(std::string&);

#ifndef _BBHOST
	friend class APISequentialFileView;
	int LineInputPhysicalReadLine(char* c); 
	void LineOutputPhysicalWrite(const char*, int len);
	void LineOutputPhysicalNewLine();
#endif

	friend class CopyDatasetCommand; //gets correct enqueues
	void CopyFrom(const SequentialFileView*);

public:
	SequentialFileServices* Seqserv() {return seqserv;}
	void CloseAndDestroy();

	bool ReadNoCRLF(char* buffer, unsigned int len);
	unsigned int ReadNoCRLFAnyLength(char* buffer, unsigned int len);
	void WriteNoCRLF(const char* buffer, unsigned int len);
	void Rewind() {pos = 0;}
	
	short MRL() {short r = seqfile->Lrecl(); if (r == -1) return 32767; return r;}
	const std::string& GetDDName() {return hfile.GetDD();}

#ifdef _BBHOST
	void ThrowIfEOF();
#endif
};

} //close namespace

#endif
