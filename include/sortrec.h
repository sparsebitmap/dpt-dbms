
//A lot like RecordCopy, but with more complicated build-time functions for use during
//the data-collection phase of the sort.

#if !defined BB_SORTREC
#define BB_SORTREC

#include <vector>
#include "fieldval.h"
#include "recread.h"
#include "apiconst.h"

namespace dpt {

class SingleDatabaseFileContext;
typedef short FieldID;
struct SortRecordsFieldSpec;

class SortRecord : public ReadableRecord {
	//Unlike class RecordCopy we can rely on this staying around
	SingleDatabaseFileContext* home_context;

	friend class SortRecordSet;
	std::vector<FieldID> fids;
	std::vector<FieldValue> fvals;
	std::vector<FieldValue> converted_key_values;

	struct KeyInfo {
		int fvpix;
		SortType type;
		SortDirection dir;
		KeyInfo() : fvpix(INT_MAX) {} //type and dir irrelevant if missing
		bool Missing() const {return fvpix == INT_MAX;}
	};
	std::vector<KeyInfo> key_infos;

	void ScanFields(const FieldID, int*, int*, FieldValue*) const;

	SortRecord(SingleDatabaseFileContext* h, int r) 
		: ReadableRecord(r), home_context(h) {}

	//Copy constructor used during EACH key rotation generation
	SortRecord(const SortRecord* from) 
		: ReadableRecord(from->primary_extent_recnum), home_context(from->home_context), 
			fids(from->fids), fvals(from->fvals),
			converted_key_values(from->converted_key_values),
			key_infos(from->key_infos) {}

	friend class BitMappedRecordSet;
	void InitializeKeys(int);
	int AppendNonKey(const FieldID&, const FieldValue&);
	void AppendKey(const SortRecordsFieldSpec&, const FieldID&, const FieldValue&);

	void Clear() {fids.clear(); fvals.clear();}

	int GetFVPix(const FieldID&, FieldValue&) const;

	const FieldValue& GetKeyVal(int fvpix) const {
		if (fvpix >= 0)
			return fvals[fvpix];
		else
			return converted_key_values[-fvpix-1];}

	friend struct SortRecordPtr;
	bool operator< (const SortRecord& rhs) const; 

public:
	SingleDatabaseFileContext* HomeFileContext() const {return home_context;}

	//Functions as per read-only base class
	bool GetFieldValue(const std::string& fname, FieldValue&, int occ = 1) const;
	int CountOccurrences(const std::string&) const;
	void CopyAllInformation(RecordCopy&) const;
	bool GetNextFVPair(std::string&, FieldValue&, int& fvpix) const;

	//Extra func usable with this class
	int NumFVPairs() const {return fvals.size();}
};

}	//close namespace

#endif

