/*******************************************************************************************
This class contains all the stuff that you can do on database files.  It is not accessible
directly to a user, but only via DatabaseServices (for allocate and a couple of others) 
and database file contexts (group or otherwise) for most operations.
*******************************************************************************************/
#if !defined(BB_DBFILE)
#define BB_DBFILE

#include <string>
#include <vector>
#include <map>
#include "const_file.h"
#include "infostructs.h"
#include "file.h"
#include "apiconst.h"
#include "statized.h"

namespace dpt {

class DatabaseServices;
class DatabaseFileContext;
class SingleDatabaseFileContext;
class BufferedFileInterface;
class BufferPage;
class CriticalFileResource;
class FileOpenLockSentry;
class FileStatusInfo;
class SequentialFile;
class SequentialFileView;
class DeferredUpdate1StepInfo;

class ValueSet;
class FoundSet;
class BitMappedRecordSet;
class BitMappedFileRecordSet;
class FindSpecification;
class Exception;
class Record;
class StoreRecordTemplate;
class FieldValue;
class RecordSetCursor;
struct PhysicalFieldInfo;

class DatabaseFileDataManager;
class DatabaseFileIndexManager;
class DatabaseFileFieldManager;
class DatabaseFileEBMManager;
class DatabaseFileRLTManager;  //V2.24 Friend Injection is non-standard - Roger M.
class DatabaseFileTableBManager;
class DatabaseFileTableDManager;

//*****************************************************************************************
class DatabaseFile : public AllocatedFile {

	friend class FileOpenLockSentry;

	//CFRs and other locks
	CriticalFileResource* cfr_direct;
	CriticalFileResource* cfr_index;
	CriticalFileResource* cfr_updating;
	CriticalFileResource* cfr_exists;
	CriticalFileResource* cfr_recenq;
	friend class FastUnloadRequest;

	BufferPage* fct_buff_page;
	Lockable fistat_and_misc_lock;
	Lockable broadcast_message_lock;

	//V2.10 - Dec 2007.  FISTAT/FIFLAG/FILEORG now in apiconst.h.
	//<snip>

	//Buffer processing and related stuff
	BufferedFileInterface* buffapi;
	bool SetFistat(DatabaseServices*, unsigned char, bool);
	bool SetFiflags(DatabaseServices*, unsigned char, bool);
	void ReleaseFCT(DatabaseServices*);
	void AcquireFCT(DatabaseServices*);

	//Transaction control
	ThreadSafeFlag updated_ever; //UPDATING would prob be enough, but use TSF to be sure
	ThreadSafeFlag updated_since_checkpoint; //ditto - see doc tech notes for more
	static Sharable sys_update_start_inhibitor;
	static Lockable chkmsg_info_lock;
	std::string chkmsg_info;

	//Sub-objects for code convenience
	friend class DatabaseFileDataManager;
	friend class DatabaseFileIndexManager;
	friend class DatabaseFileFieldManager;
	friend class DatabaseFileRLTManager;
	friend class DatabaseFileEBMManager;
	friend class DatabaseFileTableBManager;
	friend class DatabaseFileTableDManager;
	friend class FindOperation;
	friend class DirectValueCursor;
	friend class FastLoadRequest;
	DatabaseFileDataManager* datamgr;
	DatabaseFileIndexManager* indexmgr;
	DatabaseFileFieldManager* fieldmgr;
	DatabaseFileRLTManager* rltmgr;
	DatabaseFileEBMManager* ebmmgr;
	DatabaseFileTableBManager* tablebmgr;
	DatabaseFileTableDManager* tabledmgr;

	//Frequently-accessed but rarely-changed structural parms are cached for efficiency
	int cached_bsize;
	int cached_dsize;
	void CacheParms();

	//Stats
	//V2.07 - Sep 07.  All now use threadsafe objects to guarantee accuracy.
	FileStat stat_backouts;
	FileStat stat_commits;
	FileStat stat_updttime;

	FileStat stat_badd;
	FileStat stat_bchg;
	FileStat stat_bdel;
	FileStat stat_bxdel;
	FileStat stat_bxfind;
	FileStat stat_bxfree;
	FileStat stat_bxinse;
	FileStat stat_bxnext;
	FileStat stat_bxrfnd;
	FileStat stat_bxspli;
	FileStat stat_dirrcd;
	FileStat stat_finds;
	FileStat stat_recadd;
	FileStat stat_recdel;
	FileStat stat_recds;
	FileStat stat_strecds;

	FileStat stat_dkpr;
	FileStat stat_dkrd;
	FileStat stat_dkwr;
	FileStat stat_fbwt;
	FileStat stat_dksfbs;
	FileStat stat_dksfnu;
	FileStat stat_dkskip;
	FileStat stat_dkskipt;
	FileStat stat_dkswait;
	FileStat stat_dkuptime;

	FileStat stat_ilmradd;
	FileStat stat_ilmrdel;
	FileStat stat_ilmrmove;
	FileStat stat_ilradd;
	FileStat stat_ilrdel;
	FileStat stat_ilsadd;
	FileStat stat_ilsdel;
	FileStat stat_ilsmove;

	FileStat stat_mrgvals;

	//Deferred updates (can't use SF views as they are user-owned)
	bool du_flag;
	static ThreadSafeLong num_du_files;
	FileHandle du_file_num;
	_int64 dupos_num;
	short du_numlen;
	short du_num_currmaxlen;
	FileHandle du_file_alpha;
	_int64 dupos_alpha;
	void CloseDUFiles(DatabaseServices*);
	SequentialFile* DUSFNum();
	SequentialFile* DUSFAlpha();
	_int64 ApplyDUFile(SingleDatabaseFileContext*, SequentialFileView*, bool, int, int&);
	int ApplyOneStepDUInfo(SingleDatabaseFileContext*, bool);
	void ApplyFinalOneStepDUInfo(SingleDatabaseFileContext*c) {ApplyOneStepDUInfo(c, true);}
	enum Du1Reason {DU1_NORMAL, DU1_REDEFINE, DU1_TAPED, DU1_TAPEI};
	void EnterOneStepDUMode(DatabaseServices*, int, Du1Reason = DU1_NORMAL);
	void ExitOneStepDUMode(DatabaseServices*);
	void DeleteOneStepDUInfo();

	//V2.10
	DUFormat du_format;
	int WriteVariableLengthDURecord(bool, int, short, _int64, const std::string&);
	//V2.14 Jan 09.
	DeferredUpdate1StepInfo* du_1step_info; 

	DatabaseFile(const std::string&, std::string&, FileDisp, const std::string&);
	~DatabaseFile();
	int ValidateCreateParmValue(DatabaseServices*, const std::string&, int);

public:
	static FileHandle Construct(const std::string&, std::string&, FileDisp, const std::string&);
	static void Destroy(DatabaseServices*, FileHandle&);
	const std::string& FileName(DatabaseFileContext*);

	void CheckFileStatus(bool full, bool notinit, bool broken, bool deferred);

	DatabaseFileDataManager* GetDataMgr() {return datamgr;}
	DatabaseFileIndexManager* GetIndexMgr() {return indexmgr;}
	DatabaseFileFieldManager* GetFieldMgr() {return fieldmgr;}
	DatabaseFileRLTManager* GetRLTMgr() {return rltmgr;}
	DatabaseFileEBMManager* GetEBMMgr() {return ebmmgr;}
	DatabaseFileTableBManager* GetTableBMgr() {return tablebmgr;}
	DatabaseFileTableDManager* GetTableDMgr() {return tabledmgr;}
	BufferedFileInterface* BuffAPI() {return buffapi;}

	//The defaults mean take values from parm defaults or parms.ini.  
	void Create(DatabaseServices*,		
		int	bsize		= -1, 
		int	brecppg		= -1, 
		int	breserve	= -1, 
		int	breuse		= -1, 
		int	dsize		= -1, 
		int	dreserve	= -1,
		int	dpgsres		= -1,
		int	fileorg		= -1);

	static void ValidateFileName(const std::string&);

	//Buffer management and related stuff
	static void InitializeBuffers(int, bool);
	static void ClosedownBuffers(DatabaseServices*);

	//Interface functions into the FCT.
	bool MarkPhysicallyBroken(DatabaseServices*, bool);
	bool MarkLogicallyBroken(DatabaseServices*, bool);
	bool MarkPageBroken(DatabaseServices*, bool);
	void MarkTableBFull(DatabaseServices*);
	void MarkTableDFull(DatabaseServices*);
	void SetLastTransactionTime(DatabaseServices*);
	void RollbackSetPostInfo(DatabaseServices*, time_t);
	time_t RollbackGetLastCPTime(DatabaseServices*);
	bool IsRRN();

	//Transaction management
	void BeginUpdate(DatabaseServices*);
	void StartNonBackoutableUpdate(SingleDatabaseFileContext*, bool = true);
	void CompleteUpdateAndFlush(DatabaseServices*, bool, _int64);
	bool IsPhysicallyUpdatedEver() {return updated_ever.Value();}
	void MarkPhysicallyUpdatedEver() {updated_ever.Set();}
	bool IsPhysicallyUpdatedSinceCheckpoint()  {return updated_since_checkpoint.Value();}
	void MarkPhysicallyUpdatedSinceCheckpoint(DatabaseServices*, bool);
	static bool CheckPointIsHappening();
	static void GetAllChkMsgInfo(std::vector<std::string>&);
	static void CheckpointProcessing(DatabaseServices*, int, FileHandle*);
	static int BufferTidyProcessing(int);
	void OperationDelimitingCommit(DatabaseServices*, 
		bool if_backoutable = true, bool if_nonbackoutable = true);

	//Most functions below are passed through from from DatabaseFileContext
	void Open(SingleDatabaseFileContext*, const std::string&, const std::string&, int, int);
	void Close(SingleDatabaseFileContext*);
	bool AnybodyHasOpen(); //for STATUS

	//DBA stuff
	void Initialize(SingleDatabaseFileContext*, bool, bool);
	void Increase(SingleDatabaseFileContext*, int, bool);
	void ShowTableExtents(SingleDatabaseFileContext*, std::vector<int>*);
	void Unload(SingleDatabaseFileContext*, const FastUnloadOptions&, 
		const BitMappedRecordSet*, const std::vector<std::string>*, const std::string&, bool);
	void Load(SingleDatabaseFileContext*, const FastLoadOptions&, int, BB_OPDEVICE*, const std::string&, bool);
	void Defrag(SingleDatabaseFileContext*);
	void SetBroadcastMessage(SingleDatabaseFileContext*, const std::string&);
	bool IsInDeferredUpdateMode() {return du_flag;}
	void WriteDeferredUpdateRecord(DatabaseServices*, int, short, const FieldValue&);
	bool IsInOneStepDUMode() {return (du_1step_info) ? true : false;}
	void WriteOneStepDURecord(SingleDatabaseFileContext*, int, PhysicalFieldInfo*, const FieldValue&);
	_int64 ApplyDeferredUpdates(SingleDatabaseFileContext*, int);
	static int NumDUFiles() {return num_du_files.Value();}
	int RequestFlushOneStepDUInfo(SingleDatabaseFileContext*c) {return ApplyOneStepDUInfo(c, false);}

	//Record processing
	int StoreRecord(SingleDatabaseFileContext*, StoreRecordTemplate&);
	void FindRecords(int, SingleDatabaseFileContext*, FoundSet*, 
		const FindSpecification&, const FindEnqueueType&, const BitMappedFileRecordSet*);
	void DirtyDeleteRecords(SingleDatabaseFileContext*, BitMappedFileRecordSet*);
	void FileRecordsUnder(SingleDatabaseFileContext*, BitMappedFileRecordSet*, 
		const std::string&, const FieldValue&);

	//Parms and stats
	std::string ResetParm(SingleDatabaseFileContext*, const std::string&, const std::string&);
	std::string ViewParm(SingleDatabaseFileContext*, const std::string&, bool) const;

	_int64 ViewStat(const std::string&) const;
	void IncUpdateStat(bool c) {if (c) stat_commits.Inc(); else stat_backouts.Inc();}
	void IncStatDKPR(DatabaseServices* d);
	void IncStatDKRD(DatabaseServices* d);
	void IncStatDKWR(DatabaseServices* d);
	void IncStatFBWT(DatabaseServices* d);
	void IncStatDKSFBS(DatabaseServices* d);
	void IncStatDKSFNU(DatabaseServices* d);
	void AddToStatDKSKIPT(DatabaseServices* d, int);
	void IncStatDKSWAIT(DatabaseServices* d);

	void HWMStatDKSKIP(DatabaseServices*, int);
	void AddToStatDKUPTIME(DatabaseServices*, _int64);

	//File stats
	void IncStatRECADD(DatabaseServices* d);
	void IncStatRECDEL(DatabaseServices* d);
	void IncStatBADD(DatabaseServices* d);
	void IncStatBCHG(DatabaseServices* d);
	void IncStatBDEL(DatabaseServices* d);
	void IncStatBXDEL(DatabaseServices* d);
	void IncStatBXFIND(DatabaseServices* d);
	void IncStatBXFREE(DatabaseServices* d);
	void AddToStatBXFREE(DatabaseServices* d, int); //DELETE FIELD
	void IncStatBXINSE(DatabaseServices* d);
	void IncStatBXNEXT(DatabaseServices* d);
	void IncStatBXRFND(DatabaseServices* d);
	void IncStatBXSPLI(DatabaseServices* d);
	void AddToStatDIRRCD(DatabaseServices*, int);
	void IncStatFINDS();
	void IncStatRECDS(DatabaseServices* d);
	void IncStatSTRECDS(DatabaseServices* d);

	void IncStatILMRADD(DatabaseServices* d);
	void IncStatILMRDEL(DatabaseServices* d);
	void IncStatILMRMOVE(DatabaseServices* d);
	void IncStatILRADD(DatabaseServices* d);
	void IncStatILRDEL(DatabaseServices* d);
	void IncStatILSADD(DatabaseServices* d);
	void IncStatILSDEL(DatabaseServices* d);
	void IncStatILSMOVE(DatabaseServices* d);

	void AddToStatMRGVALS(DatabaseServices* d, _int64);
};

//****************************************************************************************
class FileOpenLockSentry {
	DatabaseFile* file;
	bool prev_shr;
public:
	FileOpenLockSentry(DatabaseFile* f, bool lk, bool ps) {Get(f, lk, ps);}

	//V3.0. To support conditional get.
	FileOpenLockSentry() : file(NULL) {}
	void Get(DatabaseFile*, bool, bool);

	~FileOpenLockSentry();
};

} //close namespace

#endif
