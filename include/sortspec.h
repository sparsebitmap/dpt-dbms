
#if !defined BB_SORTSPEC
#define BB_SORTSPEC

#include <string>
#include <vector>

#include "apiconst.h"

namespace dpt {

typedef short FieldID;

//*************************************************************************************
struct SortRecordsFieldSpec {
	std::string name;
	SortType key_type;
	SortDirection key_dir;
	bool sort_by_each;
	bool collect_all_occs;

	//Work fields used at sort time
	FieldID fid;
	int kix;
	int ekix;

	SortRecordsFieldSpec(const std::string& n, 
		const SortType t,
		const SortDirection d, 
		const bool se,
		const bool ca) 
		: name(n), key_type(t), key_dir(d), sort_by_each(se), 
			collect_all_occs(ca), kix(-1), ekix(-1) {}

	SortRecordsFieldSpec(const SortRecordsFieldSpec& from)
		: name(from.name), key_type(from.key_type), 
			key_dir(from.key_dir), sort_by_each(from.sort_by_each), 
			collect_all_occs(from.collect_all_occs), kix(from.kix), ekix(from.ekix) {}

};

//*************************************************************************************
class SortRecordsSpecification {
	friend class BitMappedRecordSet;
	friend class SortRecordSet;
	int record_count_limit;
	std::vector<SortRecordsFieldSpec> fields;
	bool collect_all_fields;
	bool sort_keys_only;

	void ClearNonKeys();

	//Mar 07 - save some copying around if API progs pass by value.
	friend class APISortRecordsSpecification;
	int refcount;

public:
	//Leave rl=-1 for "all".  Overspecifying it will waste memory and could degrade the sort.
	SortRecordsSpecification(int rl = -1, bool ko = false) 
		: record_count_limit(rl), collect_all_fields(false), sort_keys_only(ko), refcount(0) {}
	SortRecordsSpecification(const SortRecordsSpecification& from)
		: record_count_limit(from.record_count_limit), fields(from.fields),
			collect_all_fields(from.collect_all_fields), sort_keys_only(from.sort_keys_only),
			refcount(0) {}

	//Specify the order the base set should be sorted into.  Equal recs stay in orig order.
	//Calling a field a key here also adds it as a data item.  Dupe keys are disallowed.
	//The order that keys are added defines their relative priority (first = highest).
	void AddKey(const std::string& fieldname, 
				const SortDirection = SORT_ASCENDING, 
				const SortType = SORT_DEFAULT_TYPE,
				const bool sort_by_each = false);

	//Here you say what fields to collect off the base set records before sorting.
	//Dupe entries are OK with this function - the alll occs flag gets set but never reset.
	void AddData(const std::string& fieldname,
				const bool collect_all_occs = false);

	//Use this for a PAI
	void SetOptionCollectAllFields();
	//Make things work like User Language SORT RECORD KEYS
	void SetOptionSortKeysOnly();
};

}	//close namespace

#endif

