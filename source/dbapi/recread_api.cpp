
#include "stdafx.h"

#include "dbapi\recread.h"
#include "dbapi\dbctxt.h"
#include "dbapi\reccopy.h"
#include "recread.h"
#include "dbctxt.h"

namespace dpt {

int APIReadableRecord::RecNum() const
{
	return target->RecNum();
}

APIDatabaseFileContext APIReadableRecord::GetHomeFileContext() const
{
	return APIDatabaseFileContext(target->HomeFileContext());
}


//********************************************
bool APIReadableRecord::GetFieldValue
(const std::string& fname, APIFieldValue& val, int occ) const
{
	return target->GetFieldValue(fname, *(val.target), occ);
}

int APIReadableRecord::CountOccurrences(const std::string& fname) const
{
	return target->CountOccurrences(fname);
}


//********************************************
APIFieldValue APIReadableRecord::GetFieldValue(const std::string& fname, int occ) const
{
	APIFieldValue result;
	target->GetFieldValue(fname, *(result.target), occ);
	return result;
}


//********************************************
void APIReadableRecord::CopyAllInformation(APIRecordCopy& dest) const
{
	target->CopyAllInformation(*(dest.target));
}


//********************************************
bool APIReadableRecord::GetNextFVPair
(std::string& fname, APIFieldValue& val, int& fvpix) const
{
	return target->GetNextFVPair(fname, *(val.target), fvpix);
}

//********************************************
//V2.06 - Jun 07.
bool APIReadableRecord::AdvanceToNextFVPair() 
{
	return target->AdvanceToNextFVPair();
}
const std::string& APIReadableRecord::LastAdvancedFieldName() 
{
	return target->LastAdvancedFieldName();
}
APIFieldValue APIReadableRecord::LastAdvancedFieldValue() 
{
	return APIFieldValue(target->LastAdvancedFieldValue());
}
void APIReadableRecord::RestartFVPairLoop() 
{
	target->RestartFVPairLoop();
}

} //close namespace


