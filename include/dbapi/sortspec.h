
#if !defined BB_API_SORTSPEC
#define BB_API_SORTSPEC

#include <string>
#include "apiconst.h"

namespace dpt {

class SortRecordsSpecification;

class APISortRecordsSpecification {
public:
	SortRecordsSpecification* target;
	APISortRecordsSpecification(const APISortRecordsSpecification&);
	~APISortRecordsSpecification();
	//-----------------------------------------------------------------------------------

	//-1 means all sort all records in the set.
	APISortRecordsSpecification(int num_records = -1, bool keys_only = false);

	//Specify the order the base set should be sorted into.  Equal recs stay in orig order.
	//Calling a field a key here also adds it as a data item.  Dupe keys are disallowed.
	//The order that keys are added defines their relative priority (first = highest).
	void AddKeyField(
		const std::string& fieldname, 
		const SortDirection = SORT_ASCENDING, 
		const SortType = SORT_DEFAULT_TYPE,
		const bool sort_by_each = false);

	//Here you say what fields to collect off the base set records before sorting.
	//Dupe entries are OK with this function.
	void AddDataField(
		const std::string& fieldname,
		const bool collect_all_occs = false);
	//Shorthand if you know you want all fields
	void SetOptionCollectAllFields();

	//Faster collection phase but means return to file when looping on the sorted set
	void SetOptionSortKeysOnly();
};

}	//close namespace

#endif

