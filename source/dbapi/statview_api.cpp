
#include "stdafx.h"

#include "dbapi\statview.h"
#include "statview.h"

namespace dpt {

//**************************************
unsigned int APIStatViewer::View
(const std::string& s, const StatLevel l, const APIDatabaseFileContext& file, int* hiword)
{
	_int64 val = target->View(s, l, file.target);

	unsigned int result = val;
	
	if (hiword) {
		val >>= 32;
		*hiword = val;
	}
			
	return result;
}

std::string APIStatViewer::UnformattedLine
(const StatLevel l, const APIDatabaseFileContext& file)
{
	return target->UnformattedLine(l, file.target);
}

const std::string& APIStatViewer::TRequestCache()
{
	return target->TRequestCache();
}

//**************************************
void APIStatViewer::StartActivity(const std::string& name)
{
	target->StartActivity(name);
}

void APIStatViewer::EndActivity()
{
	target->EndActivity();
}

const std::string& APIStatViewer::CurrentActivity()
{
	return target->CurrentActivity();
}

} //close namespace


