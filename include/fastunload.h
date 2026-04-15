
#if !defined(BB_FASTUNLOAD)
#define BB_FASTUNLOAD

#include "apiconst.h"
#include "iowrappers.h"
#include <vector>
#include <string>

namespace dpt {

class SingleDatabaseFileContext;
class BitMappedRecordSet;
class BitMappedFileRecordSet;
class FoundSet;
class LoadDiagnostics;
struct PhysicalFieldInfo;
class RoundedDouble;
class FieldValue;

#define BUFFMAX 65500

//********************************************************************************
class FastUnloadOutputFile {
	util::BBStdioFile file;

	FastUnloadOptions opts;

	char* buff;
	int buffcurr;

	void FlushBuffer();
	char* GetBuffer(int);
	void DeleteBuffer() {if (buff) delete[] buff; buff = NULL;}

public:
	FastUnloadOutputFile(const std::string& n, const FastUnloadOptions& o) 
		: file(n.c_str(), (o & FUNLOAD_REPLACE) ? util::STDIO_CCLR : util::STDIO_NEW),
          opts(o), buff(NULL) {}
	~FastUnloadOutputFile() {DeleteBuffer();}

	util::BBStdioFile* BaseIO() {return &file;}

	void Initialize(SingleDatabaseFileContext*, const std::string&);
	void Finalize() {FlushBuffer(); DeleteBuffer();}

	void RewindBuffer(int);

	void AppendString(const char*, int len, bool b);
	void AppendString(const std::string& s, bool b) {AppendString(s.c_str(), s.length(), b);}
	void AppendTextLine(const char* s, unsigned _int8 l);
	void AppendTextLine(const std::string& s) {AppendTextLine(s.c_str(), s.length());}
	void AppendCRLF() {AppendTextLine(NULL, 0);}

	void AppendBinaryInt16(short);
	void AppendBinaryUint16(unsigned short s) {AppendBinaryInt16((short)s);}
	void AppendBinaryUint16Array(unsigned short*, unsigned short);
	void AppendBinaryDouble(const double&);
	void AppendBinaryInt32(int);

	void AppendRawData(const char*, int);
};

//********************************************************************************
class FastUnloadRequest {

	SingleDatabaseFileContext* context;
	const FastUnloadOptions opts;
	const BitMappedRecordSet* baseset;
	std::vector<std::string>* fnames;
	std::string unloaddir;
	bool reorging;

	std::vector<PhysicalFieldInfo*> fieldinfos;

	LoadDiagnostics* diags;

	FastUnloadOutputFile* tapef;
	FastUnloadOutputFile* taped;
	std::vector<FastUnloadOutputFile*> tapei_array;

	//******************************
	void CloseTapeFiles();

	bool baseset_prepped;
	const BitMappedFileRecordSet* file_baseset;
	bool empty_file_baseset;
	FoundSet* ebp;
	void PrepIndexBaseset();

public:
	FastUnloadRequest(SingleDatabaseFileContext*, const FastUnloadOptions&, 
		const BitMappedRecordSet*, const std::vector<std::string>*, const std::string&, bool);
	~FastUnloadRequest();

	SingleDatabaseFileContext* Context() {return context;}
	const BitMappedRecordSet* BaseSet() {return baseset;}
	const BitMappedFileRecordSet* FileBaseSet() {return file_baseset;}
	bool EmptyFileBaseSet() {return empty_file_baseset;}

	bool AllFields() {return (fnames == NULL);}
	bool FidRequiredInTapeF() {return ( (opts & FUNLOAD_FNAMES) == 0);}
	bool AnyDataReformatOptions() {return AnyDataReformatOptions(opts);}
	bool NofloatOption() {return ( (opts & FUNLOAD_NOFLOAT) != 0);}
	bool FnamesOption() {return ( (opts & FUNLOAD_FNAMES) != 0);}
	bool CrlfOption() {return ( (opts & FUNLOAD_CRLF) != 0);}
	bool PaiMode() {return ( (opts & FUNLOAD_PAI) != 0);}
	bool AnyBLOBs();

	static bool AnyDataReformatOptions(const FastUnloadOptions& o) {
		return ( (o & FUNLOAD_DATA_REFORMAT) != 0);}
	static std::string MakeOptsDiagSummary(const FastUnloadOptions&, int);

	const std::vector<PhysicalFieldInfo*>& FieldInfos() {return fieldinfos;}

	LoadDiagnostics* Diags() {return diags;}
	static std::string Sep(char c = '-') {return std::string(72, c);}
	static void ThrowBad(const std::string&);

	FastUnloadOutputFile* TapeF() {return tapef;}
	FastUnloadOutputFile* TapeD() {return taped;}
	const std::vector<FastUnloadOutputFile*>& TapeIArray() {return tapei_array;}

	void Perform();
};

} //close namespace

#endif
