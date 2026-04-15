
#include "stdafx.h"

#include "dbapi\parmvr.h"
#include "parmvr.h"
#include "parmref.h"

namespace dpt {

std::string APIViewerResetter::Reset(const std::string& parmname, const std::string& newvalue, 
	const APIDatabaseFileContext& file)
{
	return target->Reset(parmname, newvalue, file.target);
}

std::string APIViewerResetter::View(const std::string& parmname, 
	const APIDatabaseFileContext& file, bool fancyformat) const
{
	return target->View(parmname, fancyformat, file.target);
}

int APIViewerResetter::ViewAsInt(const std::string& parmname, 
	const APIDatabaseFileContext& file) const
{
	return target->ViewAsInt(parmname, file.target);
}

void APIViewerResetter::GetParmsInCategory
(std::vector<std::string>& vs, const std::string& cat) const
{
	target->GetRefTable()->GetParmsInCategory(vs, cat);
}

} //close namespace


