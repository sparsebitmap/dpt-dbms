/*****************************************************************************************
Database file contexts elaborate on the basic functionality of file and group contexts
(in ctxtopen.h) with all the database stuff like record sets, value cursors etc.
*****************************************************************************************/

#if !defined(BB_DBCTXT)
#define BB_DBCTXT

#include <string>
#include <map>
#include <set>
#include "ctxtopen.h"
#include "apiconst.h"
#include "dbcursor.h"
#include "filehandle.h"
#include "infostructs.h"
#include "fieldval.h"
#include "fieldatts.h"
#include "lockable.h"

namespace dpt {

class Group;
class DefinedContext;
class DatabaseServices;
class GroupDatabaseFileContext;
class SingleDatabaseFileContext;
class DatabaseFile;
class StoreRecordTemplate;
class RecordSet;
class BitMappedRecordSet;
class FoundSet;
class RecordList;
class SortRecordSet;
class SortRecordLayout;
class ValueSet;
class DirectValueCursor;
class FieldAttributeCursor;
class FieldAttributeCursor_Group;
class FieldValue;
class FindSpecification;
class FindValuesSpecification;
struct PhysicalFieldInfo;

typedef short FieldID;

#ifdef _BBHOST
class QProgram;
#endif

//*****************************************************************************************
class DatabaseFileContext : public DBCursorTarget {
	friend class DatabaseServices; //interface class

	std::set<RecordSet*> record_sets;
	Lockable record_sets_lock;
	void RegisterRecordSet(RecordSet*);

	std::set<ValueSet*> value_sets;

	friend class BitMappedRecordSet;
	SortRecordSet* CreateSortRecordSet(bool);

protected:
	virtual bool Open(const GroupDatabaseFileContext*, 
						const std::string&, const std::string&, int, int) = 0;
	virtual bool Close(const GroupDatabaseFileContext* = NULL, bool = false) = 0;
	void Close_S(bool, bool = false);

	DatabaseServices* dbapi;
	bool HasChildren() const;
	virtual DefinedContext* DC() const = 0;

	void NotifyRecordSetsOfDirtyDelete(BitMappedRecordSet*);

#ifdef _BBHOST
	//See comments in QProgram
	friend class QProgram;
	bool locked_by_request;
	void ApplyRequestLock() {locked_by_request = true;}
	void RemoveRequestLock() {locked_by_request = false;}
#endif

	//MRO management
	FoundSet* CreateFoundSet();
	friend class ValueSet;
	ValueSet* CreateValueSet();

	DatabaseFileContext(DatabaseServices*);
	virtual ~DatabaseFileContext() {}

public:
	std::string GetCURFILE() const;
	virtual std::string GetShortName() const;
	std::string GetFullName() const;
	virtual std::string GetFullFilePath() const = 0;

	DatabaseServices* DBAPI() {return dbapi;}

	//Group-related
	virtual SingleDatabaseFileContext* UpdtfileContext() = 0;
	virtual SingleDatabaseFileContext* CastToSingle() {return NULL;}
	virtual GroupDatabaseFileContext* CastToGroup() {return NULL;}

	//These are virtual to cater for some of the functions where generic code is used
	//handling group and single file contexts.  Importantly M204 commands.
	virtual int GroupOpenCount() const {return 0;}
	virtual bool IsOpenAsFile() const {return false;}

	//DBA stuff
	virtual void Initialize(bool leave_fields = false) = 0;
	virtual void Increase(int, bool tabled) = 0;
	virtual void ShowTableExtents(std::vector<int>*) = 0;
	virtual void Unload(const FastUnloadOptions& = FUNLOAD_DEFAULT, const BitMappedRecordSet* = NULL, 
				std::vector<std::string>* fnames = NULL, const std::string& dir = FUNLOAD_DIR) = 0;
	virtual void Load(const FastLoadOptions& = FLOAD_DEFAULT, int eyeball = 0, 
				BB_OPDEVICE* eyeball_altdest = NULL, const std::string& dir = FLOAD_DIR) = 0;
	virtual void Defrag() = 0;
	virtual void Reorganize(bool oi = false, const std::string& oi_fieldname = "") = 0;
	virtual _int64 ApplyDeferredUpdates(int) = 0;

	//Field definitions
	virtual void DefineField(const std::string&, bool flt = false, bool inv = false, 
			bool uae = false, bool ord = false, bool ordnum = false, unsigned char = 50,
			bool nomerge = false, bool blob = false) = 0; //V2.14 Jan 09. V3.0 Nov 10.
	virtual void RedefineField(const std::string&, const FieldAttributes&) = 0;
	virtual void DeleteField(const std::string&) = 0;
	virtual void RenameField(const std::string&, const std::string&) = 0;

	//Field info
	virtual FieldAttributes GetFieldAtts(const std::string&) = 0;
	virtual FieldID GetFieldID(const std::string&) = 0;
	virtual FieldAttributeCursor* OpenFieldAttCursor(bool gotofirst = true) = 0;
	void CloseFieldAttCursor(FieldAttributeCursor* c) {DeleteCursor((DBCursor*)c);}

	//Data manipulation:

	//Records and record sets
	virtual int StoreRecord(StoreRecordTemplate&) = 0;

	FoundSet* CreateEmptyFoundSet() {return CreateFoundSet();}
	SortRecordSet* CreateEmptySortRecordSet() {return CreateSortRecordSet(false);}

	//V2.13 Dec 08. Reformulate these without default parameters
	virtual FoundSet* FindRecords(const FindSpecification*, const FindEnqueueType&, const BitMappedRecordSet*) = 0;
	FoundSet* FindRecords() {		
									return FindRecords(NULL, FD_LOCK_SHR, NULL);}
	FoundSet* FindRecords(const FindSpecification& s) {
									return FindRecords(&s, FD_LOCK_SHR, NULL);}
	FoundSet* FindRecords(const FindSpecification& s, const FindEnqueueType& lk) {
									return FindRecords(&s, lk, NULL);}
	FoundSet* FindRecords(const FindSpecification& s, const BitMappedRecordSet* referback) {
									return FindRecords(&s, FD_LOCK_SHR, referback);}
	FoundSet* FindRecords(const FindSpecification& s, const FindEnqueueType& lk, const BitMappedRecordSet* referback) {
									return FindRecords(&s, lk, referback);}

	//These apply to lists and sorted sets too.  (NB. 'release' is included)
	void DestroyRecordSet(RecordSet*);
	void DestroyAllRecordSets();

	//Value processing
	virtual ValueSet* FindValues(const FindValuesSpecification&) = 0;
	virtual unsigned int CountValues(const FindValuesSpecification&) = 0;
	virtual void FileRecordsUnder(BitMappedRecordSet*, const std::string&, const FieldValue&) = 0;
	ValueSet* CreateEmptyValueSet() {return CreateValueSet();}
	void DestroyValueSet(ValueSet*);
	void DestroyAllValueSets();

	//Value loops directly on the b-tree (Single file only - use value sets on groups)
	virtual DirectValueCursor* OpenDirectValueCursor(const std::string&) = 0;
	virtual DirectValueCursor* OpenDirectValueCursor(const FindValuesSpecification&) = 0;
	void CloseDirectValueCursor(DirectValueCursor* c) {DeleteCursor((DBCursor*)c);}

	//Misc
	RecordList* CreateRecordList();
	virtual void DirtyDeleteRecords(BitMappedRecordSet*) = 0;

	//V3.03 Access control
	virtual void ValidateReadDataPrivs(const char*, BitMappedRecordSet* = NULL) = 0;
	virtual void ValidateWriteDataPrivs(const char*, BitMappedRecordSet* = NULL) = 0;

	//-----------------------------
	//Diagnostics.
	//Analyze type 1 returns the info for the ANALYZE command
	virtual void Analyze1(BTreeAnalyzeInfo*, 
		InvertedListAnalyze1Info*, const std::string&, bool reverse_scan = false) = 0;
	//Analyze type 2 is a complete table B scan returning inverted list page usage info
	virtual void Analyze2(InvertedListAnalyze2Info*) = 0;

	//These can be lengthy so they spool directly to output device (see #define at top)
	virtual void TableB(BB_OPDEVICE*, 
		bool list = false, bool reclen = false, int pagefrom = -1, int pageto = -1) = 0;
	//Analyze type 3 is a page-by-page analysis of a field's entire b-tree
	virtual void Analyze3(BB_OPDEVICE*, 
		const std::string& fname, bool leaves_only = false, bool reverse_scan = false) = 0;
};

//*****************************************************************************************
//Single file version
//*****************************************************************************************
class SingleDatabaseFileContext : public DatabaseFileContext, public SingleFileOpenableContext {

	FileHandle af_handle;
	DatabaseFile* dbfile;

	void EnsureNoParentGroups() const;
	void EnsureNoChildren() const;

	friend class DatabaseServices; //interface class
	SingleDatabaseFileContext(DefinedContext* dc, DatabaseServices* ds) 
		: DatabaseFileContext(ds), SingleFileOpenableContext(dc), dbfile(NULL) {}

	DefinedContext* DC() const {return defined_context;}

	bool Open(const GroupDatabaseFileContext*, 
				const std::string&, const std::string&, int, int);
	bool Close(const GroupDatabaseFileContext* parent = NULL, bool force = false);

	//V2.06. Jul 07.  For CreateClone()
	friend class DirectValueCursor;
	DirectValueCursor* OpenDirectValueCursor(PhysicalFieldInfo*);

public:
//	SingleDatabaseFileContext* CastToSingle() {return this;}
	DatabaseFile* GetDBFile() {return dbfile;}
	int GetFileID();
	std::string GetFullFilePath() const;
	SingleDatabaseFileContext* UpdtfileContext() {return this;}
	SingleDatabaseFileContext* CastToSingle() {return this;}

	int GroupOpenCount() const {return SingleFileOpenableContext::GroupOpenCount();}
	bool IsOpenAsFile() const {return SingleFileOpenableContext::IsOpenAsFile();}

	//DBA stuff
	void Initialize(bool leave_fields = false);
	void Increase(int, bool tableb);
	void ShowTableExtents(std::vector<int>*);
	void Defrag();
	_int64 ApplyDeferredUpdates(int forgiveness = -1);
	int RequestFlushSingleStepDeferredUpdates();
	void Unload(const FastUnloadOptions& = FUNLOAD_DEFAULT, const BitMappedRecordSet* = NULL, 
				std::vector<std::string>* fnames = NULL, const std::string& dir = FUNLOAD_DIR);
	void Load(const FastLoadOptions& = FLOAD_DEFAULT, int eyeball = 0, 
				BB_OPDEVICE* eyeball_altdest = NULL, const std::string& dir = FLOAD_DIR);
	void Reorganize(bool oi = false, const std::string& oi_fieldname = "");

	//Field definitions
	void DefineField(const std::string&, bool flt = false, bool inv = false, bool uae = false, 
		bool ord = false, bool ordnum = false, unsigned char = 50, bool nomerge = false, bool blob = false);
	void RedefineField(const std::string&, const FieldAttributes&);
	void DeleteField(const std::string&);
	void RenameField(const std::string&, const std::string&);
	FieldAttributes GetFieldAtts(const std::string&);
	FieldID GetFieldID(const std::string&);
	FieldAttributeCursor* OpenFieldAttCursor(bool gotofirst = true);

	//Data manipulation
	int StoreRecord(StoreRecordTemplate&);
	FoundSet* FindRecords(const FindSpecification*, const FindEnqueueType&, const BitMappedRecordSet*);
	ValueSet* FindValues(const FindValuesSpecification&);
	unsigned int CountValues(const FindValuesSpecification&);
	DirectValueCursor* OpenDirectValueCursor(const std::string&);
	DirectValueCursor* OpenDirectValueCursor(const FindValuesSpecification&);

	void FileRecordsUnder(BitMappedRecordSet*, const std::string&, const FieldValue&);
	void DirtyDeleteRecords(BitMappedRecordSet*);

	void ValidateReadDataPrivs(const char*, BitMappedRecordSet* = NULL);
	void ValidateWriteDataPrivs(const char*, BitMappedRecordSet* = NULL);

	//--------------------------------
	//Diagnostics - see comments on base declaration
	void Analyze1(BTreeAnalyzeInfo*, InvertedListAnalyze1Info*, const std::string&, bool = false);
	void Analyze2(InvertedListAnalyze2Info*);
	void TableB(BB_OPDEVICE*, bool = false, bool = false, int = -1, int = -1);
	void Analyze3(BB_OPDEVICE*, const std::string&, bool = false, bool = false);
};

//*****************************************************************************************
//Group version
//*****************************************************************************************
class GroupDatabaseFileContext : public DatabaseFileContext, public GroupOpenableContext {

	mutable SingleDatabaseFileContext* last_verified_member;
	mutable int last_verified_membid;

	//This amalgamated table is useful to the UL compiler
	friend class FieldAttributeCursor_Group;
	std::map<std::string, FieldAttributes> group_field_table;

	friend class DatabaseServices; //interface class
	GroupDatabaseFileContext(DefinedContext* dc, DatabaseServices* ds) 
		: DatabaseFileContext(ds), GroupOpenableContext(dc),
			last_verified_member((SingleDatabaseFileContext*)-1), last_verified_membid(-1) {}

	DefinedContext* DC() const {return defined_context;}
	
	bool Open(const GroupDatabaseFileContext*, 
				const std::string&, const std::string&, int, int);
	bool Close(const GroupDatabaseFileContext* parent = NULL, bool force = false);

	//Overrides from the general group class, called from GroupOpenableContext::xxx()
	SingleFileOpenableContext* OpenSingleFileSecondary(const std::string&) const;
	bool CloseSingleFileSecondary(SingleFileOpenableContext*, bool = false) const;

	//Shared processing for member and non-member functions as per below
	FoundSet* FindRecords_S(SingleDatabaseFileContext*, 
								const FindSpecification*, 
								const FindEnqueueType&, 
								const BitMappedRecordSet*);
	ValueSet* FindValues_S(SingleDatabaseFileContext*, 
								const FindValuesSpecification&);

	void SingleOnly() const;

public:
	SingleDatabaseFileContext* UpdtfileContext() {
		return static_cast<SingleDatabaseFileContext*>(updtfile_context);}
	Group* GetGroup() const;
	GroupDatabaseFileContext* CastToGroup() {return this;}

	//Stuff invalid for groups
	std::string GetFullFilePath() const {SingleOnly(); return std::string();}
	void Increase(int, bool) {SingleOnly();}
	void ShowTableExtents(std::vector<int>*) {SingleOnly();}
	void Defrag() {SingleOnly();}
	void Initialize(bool) {SingleOnly();}
	void DefineField(const std::string&, bool, bool, 
								bool, bool, bool, unsigned char, bool, bool) {SingleOnly();}
	void RedefineField(const std::string&, const FieldAttributes&) {SingleOnly();}
	void DeleteField(const std::string&) {SingleOnly();}
	void RenameField(const std::string&, const std::string&) {SingleOnly();}
	_int64 ApplyDeferredUpdates(int forgiveness = -1) {SingleOnly(); return 0;}
	void Unload(const FastUnloadOptions& = FUNLOAD_DEFAULT, const BitMappedRecordSet* = NULL, 
				std::vector<std::string>* fnames = NULL, const std::string& dir = FUNLOAD_DIR) {SingleOnly();}
	void Load(const FastLoadOptions& = FLOAD_DEFAULT, int eyeball = 0, 
				BB_OPDEVICE* eyeball_altdest = NULL, const std::string& dir = FLOAD_DIR) {SingleOnly();}
	void Reorganize(bool oi = false, const std::string& oi_fieldname = "") {SingleOnly();}

	//Access the GFT
	FieldAttributes GetFieldAtts(const std::string&);
	FieldID GetFieldID(const std::string&) {return -1;}
	FieldAttributeCursor* OpenFieldAttCursor(bool gotofirst = true);

	//Data manipulation
	int StoreRecord(StoreRecordTemplate&);
	FoundSet* FindRecords(const FindSpecification* s, const FindEnqueueType& e, const BitMappedRecordSet* b) {
								return FindRecords_S(NULL, s, e, b);}
	ValueSet* FindValues(const FindValuesSpecification& s) {
								return FindValues_S(NULL, s);}

	void FileRecordsUnder(BitMappedRecordSet*, const std::string&, const FieldValue&);
	void DirtyDeleteRecords(BitMappedRecordSet*);

	//We'd need to build a deduped set to count, so no benefit of a sep group count func 
	unsigned int CountValues(const FindValuesSpecification&) {SingleOnly(); return 0;}
	//This is only allowed if a member is given - see below.  If not, the values would not
	//be returned in order, and would also probably contain dupes, which would be confusing.
	DirectValueCursor* OpenDirectValueCursor(const std::string&) {SingleOnly(); return NULL;}
	DirectValueCursor* OpenDirectValueCursor(const FindValuesSpecification&) {SingleOnly(); return NULL;}

	void ValidateReadDataPrivs(const char*, BitMappedRecordSet* = NULL);
	void ValidateWriteDataPrivs(const char*, BitMappedRecordSet* = NULL);

	//--------------------------------
	//For 'IN GROUP MEMBER' style processing
	SingleDatabaseFileContext* GetMemberContextByName(const std::string&) const;
	SingleDatabaseFileContext* GetMemberContextByGroupOrder(int) const;

	FoundSet* FindRecords(SingleDatabaseFileContext* m, 
								const FindSpecification* s = NULL, 
								const FindEnqueueType& e = FD_LOCK_SHR, 
								const BitMappedRecordSet* b = NULL) {
									return FindRecords_S(m, s, e, b);}
	ValueSet* FindValues(SingleDatabaseFileContext* m, const FindValuesSpecification& s) {
									return FindValues_S(m, s);}

	DirectValueCursor* OpenDirectValueCursor(SingleDatabaseFileContext* m, const std::string&);
	DirectValueCursor* OpenDirectValueCursor(SingleDatabaseFileContext* m, const FindValuesSpecification&);

	int GetGroupIndex(SingleDatabaseFileContext*, const char* usage, bool throwit = true) const;

	//--------------------------------
	void Analyze1(BTreeAnalyzeInfo*, InvertedListAnalyze1Info*, 
		const std::string&, bool = false) {SingleOnly();}
	void Analyze2(InvertedListAnalyze2Info*) {SingleOnly();}
	void TableB(BB_OPDEVICE*, bool = false, bool = false, int = -1, int = -1) {SingleOnly();}
	void Analyze3(BB_OPDEVICE*, const std::string&, bool = false, bool = false) {SingleOnly();}
};


} //close namespace

#endif
