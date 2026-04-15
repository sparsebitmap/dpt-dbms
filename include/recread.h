
#if !defined(BB_RECREAD)
#define BB_RECREAD

#include <string>
#include "fieldval.h"

namespace dpt {

class SingleDatabaseFileContext;
class RecordCopy;

//***************************************************************************************
//Interface shared by the read-only RecordCopy and SortRecord classes
//***************************************************************************************
class ReadableRecord {

	//V2.06 - see below
	std::string ladv_fname;
	FieldValue ladv_fval;
	int ladv_fvpix;

protected:
	int primary_extent_recnum;

	ReadableRecord(int r) : ladv_fvpix(0), primary_extent_recnum(r)  {}

public:
	virtual ~ReadableRecord() {}

	int RecNum() const {return primary_extent_recnum;}
	virtual SingleDatabaseFileContext* HomeFileContext() const = 0;

	//Return value confirms presence.  If missing, value is set to zero/null as per ftype.
	virtual bool GetFieldValue(const std::string& fname, FieldValue&, int occ = 1) const = 0;
	virtual int CountOccurrences(const std::string&) const = 0;

	//Can be more convenient if you are happy for "not present" to look like zero/null
	FieldValue GetFieldValue(const std::string&, int) const;

	//Use for "PAI" if the record will fit in memory and you'll be accessing it a lot
	virtual void CopyAllInformation(RecordCopy&) const = 0;

	//Or this to read off f=v pairs one by one - see API doc for other considerations
	virtual bool GetNextFVPair(std::string&, FieldValue&, int& fvpix) const = 0;

	//Alternate syntax for e.g. Python (again see doc)
	bool AdvanceToNextFVPair() {return GetNextFVPair(ladv_fname, ladv_fval, ladv_fvpix);}
	const std::string& LastAdvancedFieldName() {return ladv_fname;}
	const FieldValue& LastAdvancedFieldValue() {return ladv_fval;}
	void RestartFVPairLoop() {ladv_fvpix = 0;} //V3.0
};


} //close namespace

#endif
