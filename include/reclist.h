
/****************************************************************************************
Like foundsets but buildable by the user from found sets, other lists, and single records
****************************************************************************************/

#if !defined(BB_RECLIST)
#define BB_RECLIST

#include "bmset.h"

namespace dpt {

class ReadableRecord;

//***************************************************************************************
class RecordList : public BitMappedRecordSet {

	friend class DatabaseFileContext;
	RecordList(DatabaseFileContext* c) : BitMappedRecordSet(c) {}
	~RecordList() {} //ensure private - use context destroy function

public:
	void Remove(const BitMappedRecordSet* s) {BitAndNot(s);} //Remove self calls Clear()
	void Place(const BitMappedRecordSet* s) {BitOr(s);} //Place self has no effect

	void Remove(const ReadableRecord* r) {BitAndNot(r);}
	void Place(const ReadableRecord* r) {BitOr(r);}

	//In terms of the distinction between Clear (invalidates further access on same
	//cursor) and Remove (doesn't), this works like the latter unless the whole
	//group set becomes empty, in which case the former.
	void ClearMember(SingleDatabaseFileContext* sfc) {ClearMember_B(sfc);}
};

} //close namespace

#endif
