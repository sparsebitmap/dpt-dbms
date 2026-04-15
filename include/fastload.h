
#if !defined(BB_FASTLOAD)
#define BB_FASTLOAD

#include "apiconst.h"
#include "fieldatts.h"
#include "fieldinfo.h"
#include "iowrappers.h"
#include "infostructs.h"
#include "buffer.h"
#include <vector>
#include <string>
#include "msg_db.h"

#define RECNUM_XREF_BUFFSIZE 1000

namespace dpt {

class SingleDatabaseFileContext;
class BitMappedFileRecordSet;
class LoadDiagnostics;
class FastLoadRequest;

typedef short FieldID;
class FieldValue;

//************************************************************************************
class FastLoadInputFile {
	util::LocalBufferedSequentialInputFile file;

	std::string tapei_field;

	FastLoadRequest* request;
	FastUnloadOptions opts; //(the options found in the metadata header)
	std::string saved_comments;
	int data_start;

	bool delete_file;

	void UnexpectedEOF(int);
	void HitEOF(bool* eof_flag, int n) {if (eof_flag) *eof_flag = true; else UnexpectedEOF(n);}
	void ReadBytes(void* b, int n, bool* e) {if (file.Read(b, n) < n) HitEOF(e, n);}

	~FastLoadInputFile();

public:
	FastLoadInputFile(FastLoadRequest*, const std::string&, const std::string& = std::string());
	static void Destroy(FastLoadInputFile* t, bool d) {t->delete_file = d; delete t;}

	bool Initialize(bool save_comments = false);
	const std::string& TapeIFieldName() {return tapei_field;}
	FastLoadRequest* Request() {return request;}

	double FilePosPercent() {double l,t; l = file.FLengthI64(); t = file.TellI64(); return (t/l)*100;}
	std::string FilePosDetailed();

	bool CrlfOption() {return ( (opts & FUNLOAD_CRLF) != 0);}
	bool EbcdicOption() {return ( (opts & FUNLOAD_EBCDIC) != 0);}
//	bool IEndianOption() {return ( (opts & FUNLOAD_IENDIAN) != 0);}
	bool PaiOption() {return ( (opts & FUNLOAD_PAI) != 0);}
	bool FnamesOption() {return ( (opts & FUNLOAD_FNAMES) != 0);}
	bool NofloatOption() {return ( (opts & FUNLOAD_NOFLOAT) != 0);}
	std::string MakeOptsDiagSummary(int);

	int ReadBinaryInt32(bool* eof = NULL);
	short ReadBinaryInt16(bool* eof = NULL);
	unsigned short ReadBinaryUint16(bool* eof = NULL);
	void ReadBinaryUint16Array(unsigned short*, int, bool* eof = NULL);
	unsigned _int8 ReadBinaryUint8(bool* eof = NULL);
	double ReadBinaryDouble(bool* eof = NULL);
	std::string ReadTextLine(bool* eof = NULL, char pretermchar = 0);
	void ReadChars(void*, int, bool* eof = NULL);
	void ReadRawData(void*, int, bool* eof = NULL);
	void ReadCRLF();

	void OpenFile() {file.Open(NULL, util::STDIO_RDONLY); file.SeekI64(data_start);}
	void CloseFile() {file.Close();}
	bool FileIsOpen() {return file.IsOpen();}

	void ReadCommandTextLine(std::string&, bool* eof);
	void SplitCombinedTapeI();
};

//************************************************************************************
class FastLoadFieldInfo {

	//This information comes from a combination of two possible sources, namely the
	//TAPEF entries and the existing table A before the load, as follows:

	//Uniquely identifies one of these objects.  Union of those two sets.
	std::string name;

	//A copy of the table A info for the field after the TAPEF phase is finished, 
	//or a local fresh dummy copy if just eyeball mode and the file is empty.  This
	//item is only held to cater for that second case.
	PhysicalFieldInfo local_pfi;

	//The actual object in the field manager, if present
	const PhysicalFieldInfo* actual_pfi;

	//Whether it was new or existed before the load.
	bool defined_this_load;

	//The info from TAPEF if supplied.  More specifically what will be required in TAPED/I.
	bool was_in_tapef;
	FieldID tapef_id;
	FieldAttributes tapedi_atts;

	//When supplied
	FastLoadInputFile* tapei;

	const PhysicalFieldInfo* PFI() const {return (actual_pfi) ? (actual_pfi) : &local_pfi;}

public:
	//----------------
	FastLoadFieldInfo(const std::string& n, const PhysicalFieldInfo* p, bool d,
		bool tf, FieldID tfi, const FieldAttributes& a); 

	const std::string& Name() const {return name;}
	const PhysicalFieldInfo* GetActualPFI() const {return actual_pfi;}

	const FieldID TableA_ID() const {return PFI()->id;}
	const FieldAttributes& TableA_Atts() const {return PFI()->atts;}

	const FieldID TAPEF_ID() const {return tapef_id;}
	const FieldAttributes& TAPEDI_Atts() const {return tapedi_atts;}

	void SetTapeI(FastLoadInputFile* t) {tapei = t;}
	const FastLoadInputFile* TapeI() const {return tapei;}

	bool DynamicIndexBuild() const {return (tapei == NULL && TableA_Atts().IsOrdered());}
	bool IsBLOB() const {return TableA_Atts().IsBLOB();}
	bool WasInTapeF() const {return was_in_tapef;}
};

//************************************************************************************
class FastLoadRecordBuffer : public util::RechunkingBuffer {
	char* tempbuff;
	
	char* putptr;
	bool throw_badnums;
	std::vector<int> fvp_boundaries;
	bool got_fvp_boundaries;

public:
	FastLoadRecordBuffer() : tempbuff(NULL), got_fvp_boundaries(false) {}
	~FastLoadRecordBuffer() {if (tempbuff) delete[] tempbuff;}

	void GetFVPair(FieldID&, FieldValue&);
	int FindHighestFVPBoundaryAtOrBelow(int);

	void InitReformattingBuffer(bool);
	void PutFVPair(const PhysicalFieldInfo*, FieldValue&);
	void PutReformattedFVPairFromChunk(const PhysicalFieldInfo*, FieldValue&);
	void CommitReformatting(); 
};

//************************************************************************************	
class FastLoadRequest {

	SingleDatabaseFileContext* context;
	int eyeball;
	FastLoadOptions opts;
	BB_OPDEVICE* eyedest;
	std::string loaddir;
	bool reorging;

	LoadDiagnostics* diags;
	int loadctl_flags;

	FastLoadInputFile* tapef;
	FastLoadInputFile* taped;
	std::map<std::string, FastLoadInputFile*> tapeis;
	FastLoadInputFile* combined_tapei;
	FastLoadInputFile* errtape;

	std::vector<FastLoadFieldInfo*> fieldinfos;
	std::vector<FastLoadFieldInfo*> fieldinfos_by_tapef_id;
	std::map<std::string, FastLoadFieldInfo*> fieldinfos_by_name;

	BitMappedFileRecordSet* loaded_recset;

	struct RecNumXrefOpEntry {
		int oldrn;
		int newrn;
		RecNumXrefOpEntry() : oldrn(-1), newrn(-1) {}
		RecNumXrefOpEntry(int o, int n) : oldrn(o), newrn(n) {}
	};
	RecNumXrefOpEntry recnum_xref_opbuff[RECNUM_XREF_BUFFSIZE];
	int recnum_xref_opbuff_current;
	int recnum_xref_oldrn_lowest;
	int recnum_xref_oldrn_highest;
	util::BBStdioFile* recnum_xref_file;
	int* recnum_xref_table;
	std::string recnum_xref_table_filename;
	void BuildRecNumXrefTable();
	bool any_orphan_ilrec;

	bool completed_ok;

	//******************************
	void Perform_Part1();
	void Perform_Part2();
	bool Perform_Part2a();
	void Perform_Part2b(double);

	void DisposeTapeFiles();

public:
	FastLoadRequest(SingleDatabaseFileContext*, const FastLoadOptions&, int,
						BB_OPDEVICE* eyeball_altdest, const std::string&, bool);
	~FastLoadRequest();

	SingleDatabaseFileContext* Context() {return context;}

	LoadDiagnostics* Diags() const {return diags;}
	static std::string Sep(char c = '-') {return std::string(72, c);}
	void ThrowBad(const std::string&, int = DBA_LOAD_ERROR) const;

	int EyeballRecs() const {return eyeball;}
	bool AnyDynamicIndexBuild() const;
	bool AnyBLOBs() const;
	bool AnyTapeI() const {return (tapeis.size() != 0);}
	bool GotAllTapeFIDs() const;
	bool AnyTapeFIDChanges() const;
	int NumDataFormatChanges() const;
	void ValidateFieldDefChanges(int) const;

	FastLoadInputFile* TapeF() const {return tapef;}
	FastLoadInputFile* TapeD() const {return taped;}
	std::map<std::string, FastLoadInputFile*>* TapeIs() {return &tapeis;}

	void Perform() {Perform_Part1(); Perform_Part2();}

	void AddFieldInfo(const std::string&, const PhysicalFieldInfo*, 
						bool, bool, FieldID = -1, const FieldAttributes& = FieldAttributes());
	FastLoadFieldInfo* GetLoadFieldInfoByTapeFID(int, bool = true) const;
	FastLoadFieldInfo* GetLoadFieldInfoByName(const std::string&, bool = true) const;

	//Record number info
	void NoteStoredRecNum(int recnum_in, int recnum_stored);
	int XrefRecNum(int oldrn);

	FastLoadInputFile* GetErrTape() {return errtape;}
	void SetErrTape(FastLoadInputFile* t) {errtape = t;}
};


} //close namespace

#endif
