//****************************************************************************************
//Like a foundset but buildable by user code
//****************************************************************************************

#if !defined(BB_API_RECLIST)
#define BB_API_RECLIST

#include "bmset.h"

namespace dpt {

class APIReadableRecord;
class RecordList;

class APIRecordList : public APIBitMappedRecordSet {
public:
	RecordList* target;
	APIRecordList(RecordList*);
	APIRecordList(const APIRecordList&);
	//-----------------------------------------------------------------------------------
	
	void Remove(const APIBitMappedRecordSet&);
	void Place(const APIBitMappedRecordSet&);

	void Remove(const APIReadableRecord&);
	void Place(const APIReadableRecord&);
};

} //close namespace

#endif
