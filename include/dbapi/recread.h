//***************************************************************************************
//Interface shared by the read-only RecordCopy and SortRecord classes, as well as
//if you open an actual database record in read-only mode.
//***************************************************************************************

#if !defined(BB_API_RECREAD)
#define BB_API_RECREAD

#include <string>

#include "fieldval.h"

namespace dpt {

class APIRecordCopy;
class ReadableRecord;
class APIDatabaseFileContext;

class APIReadableRecord {
public:
	ReadableRecord* target;
	APIReadableRecord(ReadableRecord* t) : target(t) {}
	APIReadableRecord(const APIReadableRecord& t) : target(t.target) {}
	virtual ~APIReadableRecord() {}
	//-----------------------------------------------------------------------------------

	int RecNum() const;
	APIDatabaseFileContext GetHomeFileContext() const;

	//Return value confirms presence.  If missing, value is set to zero/null as per ftype.
	bool GetFieldValue(const std::string& fname, APIFieldValue&, int occ = 1) const;
	int CountOccurrences(const std::string& fname) const;

	//Can be more convenient if you are happy for "not present" to look like zero/null
	APIFieldValue GetFieldValue(const std::string& fname, int occ = 1) const;

	//Use for "PAI" or if you'll be doing a lot of read access
	void CopyAllInformation(APIRecordCopy&) const;

	//Or this to read off f=v pairs one by one - see API doc for other considerations
	bool GetNextFVPair(std::string&, APIFieldValue&, int& fvpix) const;

	//Alternate syntax for e.g. Python (again see doc)
	bool AdvanceToNextFVPair();
	const std::string& LastAdvancedFieldName();
	APIFieldValue LastAdvancedFieldValue();
	void RestartFVPairLoop();
};


} //close namespace

#endif
