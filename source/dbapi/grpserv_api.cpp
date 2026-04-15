
#include "stdafx.h"

#include "dbapi\grpserv.h"
#include "grpserv.h"

namespace dpt {

//**************************************
void APIGroupServices::Create
(const std::string& n, const std::vector<std::string>& m, GroupType t, const std::string& u)
{
	target->Create(n, m, t, u);
}

//**************************************
GroupType APIGroupServices::Delete(const std::string& n, GroupType t)
{
	return target->Delete(n, t);
}

//**************************************
std::vector<std::string> APIGroupServices::List(GroupType t) const
{
	return target->List(t);
}

GroupType APIGroupServices::DisplayType
(const std::string& gname, GroupType type) const
{
	return target->DisplayType(gname, type);
}

std::string APIGroupServices::DisplayUpdtfile
(const std::string& gname, GroupType type) const
{
	return target->DisplayUpdtfile(gname, type);
}

std::vector<std::string> APIGroupServices::DisplayMembers
(const std::string& gname, GroupType type) const
{
	return target->DisplayMembers(gname, type);
}


} //close namespace


