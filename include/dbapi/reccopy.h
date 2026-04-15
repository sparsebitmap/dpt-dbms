//****************************************************************************************
//Use this to take a snapshot of a record if you are going to be doing a lot
//of read-only access to the record without any intervening changes.
//Following on from that it is also used in sorted record set processing, where the data
//that's actually sorted is a vector of pointers to these objects, and by default the
//AccessCurrentRecord() function returns such a pointer instead of a "real" Record*
//as it does with normal unsorted set processing
//****************************************************************************************

#if !defined BB_API_RECCOPY
#define BB_API_RECCOPY

#include <string>
#include "fieldval.h"
#include "recread.h"

namespace dpt {

class RecordCopy;

class APIRecordCopy : public APIReadableRecord {
public:
	RecordCopy* target;
	APIRecordCopy();
	APIRecordCopy(RecordCopy*);
	APIRecordCopy(const APIRecordCopy&);
	~APIRecordCopy();
	//-----------------------------------------------------------------------------------

	void Clear();

	int NumFVPairs() const;
};

//***************************************************************************************
//V2.18 May 09.  This is now a class in its own right.
//typedef APIRecordCopy APIStoreRecordTemplate;

class APIStoreRecordTemplate : public APIRecordCopy {
public:
	APIStoreRecordTemplate();

	void Append(const std::string&, const APIFieldValue&);

	//More efficient to reuse the same object when e.g. field names are always the same 
	//for each call - just set new values for the existing field names.
	//Various ways to do it - just make sure to load up the same number of names and values.
	//With multiply-occurring fields, you can just clear the MO section if it's all at the end.
	void ClearFieldNames(int leave_number = 0);
	void AppendFieldName(const std::string&); 
	void SetFieldName(int, const std::string&);

	void ClearFieldValues(int leave_number = 0);
	void AppendFieldValue(const APIFieldValue&);
	void SetFieldValue(int, const APIFieldValue&);

	void Clear(); //V2.25
};

} //close namespace

#endif

