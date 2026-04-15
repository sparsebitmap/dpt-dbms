
#if !defined(BB_API_RECORD)
#define BB_API_RECORD

#include "recread.h"

namespace dpt {

class Record;

//***************************************************************************************
class APIRecord : public APIReadableRecord {
public:
	Record* target;
	APIRecord(Record*);
	APIRecord(const APIRecord&);
	//-----------------------------------------------------------------------------------

	//Field functions return information:  In all cases the return value indicates an
	//occurrence number, with specific meaning differing as follows.  In some cases you
	//can also get back the pre-change value.
	
	//Add, insert: retval means the occurrence added/inserted.
	int AddField(const std::string& fname, const APIFieldValue& add_value);
	int InsertField(const std::string& fname, const APIFieldValue& newval, const int newocc = 1);

	//Change and delete by occurrence: retval is the old occurrence that was affected.  So
	//if occ existed, same as input, otherwise -1.  -1 means either delete did nothing, change 
	//worked like add, or field is invisible (index work may or may not have been done).
	//Note that with UAE fields, the replaced occ may not be the same as that changed.
	int ChangeField(const std::string& fname, const APIFieldValue& newval, 
									const int occ = 1, APIFieldValue* outoldval = NULL);
	int DeleteField(const std::string& fname, const int occ = 1, APIFieldValue* ov = NULL);

	//Change and delete by value: retval is the old occurrence that got affected, as above.
	int ChangeFieldByValue(const std::string& fname, const APIFieldValue& newval, 
									const APIFieldValue& oldval);
	int DeleteFieldByValue(const std::string& fname, const APIFieldValue& oldval);

	//Retval is the number of occs deleted
	int DeleteEachOccurrence(const std::string& fname);

	//Delete the record
	void Delete();

	//Potentially useful diagnostics - accessible in UL via the *RECINFO print term
	std::string ShowPhysicalInformation();

	//V3.0. BLOBs.  None of the above have any special BLOB processing (treated just like STRING).
	//These versions allow retrieval of the value and/or the descriptor or neither.
	//If just the descriptor is requested, *pdesc gets the value if it's a non-BLOB field.
	bool GetNextFVPairAndOrBLOBDescriptor(std::string&, APIFieldValue* pval, APIFieldValue* pdesc, int& fvpix) const;
	//This one returns the normal value if it's a non-BLOB field.
	bool GetBLOBDescriptor(const std::string& fname, APIFieldValue&, int occ = 1) const;
};

} //close namespace

#endif
