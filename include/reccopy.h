//****************************************************************************************
//This class is used to construct records prior to using StoreRecord().
//It can also be used to take a snapshot of a record if you are going to be doing a lot
//of read-only access to the record without any intervening changes.
//****************************************************************************************

#if !defined BB_RECCOPY
#define BB_RECCOPY

#include <string>
#include <map>
#include <vector>
#include "fieldval.h"
#include "record.h"

namespace dpt {

typedef short FieldID;
struct PhysicalFieldInfo;

class RecordCopy : public ReadableRecord {
	friend class Record;
	friend class RecordDataAccessor;
	friend class SortRecord;

	void ScanFields(const std::string&, int*, int*, FieldValue*) const;

	friend class DatabaseFile;
	void SetRecNum(int r) {primary_extent_recnum = r;}

	//Mar 07.  For use by API wrapper.
	friend class APIRecordCopy;
	int refcount;

	//V2.18. Code calling Store() must now use the derived class below
protected:
	std::vector<std::string> fnames;
	std::vector<FieldValue> fvals;
	void Append(const std::string&, const FieldValue&);


public:
	RecordCopy(int r = -1) : ReadableRecord(r), refcount(0) {}

	//A RecordCopy is a simple stand-alone structure, so not linked to context
	SingleDatabaseFileContext* HomeFileContext() const {return NULL;}

	void Clear() {fnames.clear(); fvals.clear();}

	//Functions as per read-only base class
	bool GetFieldValue(const std::string& fname, FieldValue&, int occ = 1) const;
	int CountOccurrences(const std::string&) const;
	void CopyAllInformation(RecordCopy&) const;
	bool GetNextFVPair(std::string&, FieldValue&, int& fvpix) const;

	//Extra func usable with copies
	int NumFVPairs() const {return fnames.size();}
};

//***************************************************************************
//V2.18 May 09.  This is now a class in its own right.
//typedef RecordCopy StoreRecordTemplate;

class StoreRecordTemplate : public RecordCopy {

	friend class DatabaseFile;
	std::vector<PhysicalFieldInfo*> fids;
	void Validate() const;
	bool got_fids;
	void AppendFid(PhysicalFieldInfo*);

	void ClearFids() {fids.clear(); got_fids = false;}


public:
	StoreRecordTemplate() : RecordCopy() {ClearFids();}

	void Append(const std::string&, const FieldValue&);

	//These save time both in calling code and in the DBMS where it can cache FIDs.
	//Various ways to do it - just make sure to load up the same number of names and values.
	//With multiply-occurring fields, you can just clear the MO section if it's all at the end.
	void ClearFieldNames(int leave_number = 0) {ClearFids(); fnames.resize(leave_number);}
	void AppendFieldName(const std::string& s) {ClearFids(); fnames.push_back(s);} 
	void SetFieldName(unsigned int, const std::string&);

	void ClearFieldValues(int leave_number = 0) {fvals.resize(leave_number);}
	void AppendFieldValue(const FieldValue& v) {fvals.push_back(v);}
	void SetFieldValue(unsigned int, const FieldValue&);

	void Clear() {RecordCopy::Clear(); ClearFids();} //V2.25
};


} //close namespace

#endif

