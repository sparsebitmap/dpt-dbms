
#include "stdafx.h"

#include "dbapi\seqserv.h"
#include "dbapi\seqfile.h"
#include "seqserv.h"

namespace dpt {

//*************************************
void APISequentialFileServices::Allocate
(const std::string& dd, const std::string& dsn, FileDisp disp, 
 int lrecl, char pad, unsigned int max, const std::string& alias, bool nocrlf)
{
	target->Allocate(dd, dsn, disp, lrecl, pad, max, alias, nocrlf);
}

//*************************************
void APISequentialFileServices::Free(const std::string& dd)
{
	target->Free(dd);
}

//*************************************
APISequentialFileView APISequentialFileServices::OpenView(const std::string& dd, bool lk)
{
	return APISequentialFileView(target->OpenSeqFile(dd, lk));
}

//*************************************
void APISequentialFileServices::CloseView(const APISequentialFileView& v)
{
	target->CloseSeqFile(v.target);
}

} //close namespace


