//*****************************************************************************************
//Interface to M204-style group definitions
//*****************************************************************************************

#if !defined(BB_API_GRPSERV)
#define BB_API_GRPSERV

#include <string>
#include <vector>

#include "const_group.h"

namespace dpt {

class GroupServices;

class APIGroupServices {
public:
	GroupServices* target;
	APIGroupServices(GroupServices* t) : target(t) {}
	APIGroupServices(const APIGroupServices& t) : target(t.target) {}
	//-----------------------------------------------------------------------------------

	//After creation, use OpenContext etc. to use the group
	void Create(const std::string& n, const std::vector<std::string>& m, GroupType t, 
		const std::string& u = std::string());
	GroupType Delete(const std::string&, GroupType);  

	//Since groups are shared structures you are only allowed indirect query options:
	std::vector<std::string> List(GroupType) const;

	GroupType DisplayType(const std::string&, GroupType) const;
	std::string DisplayUpdtfile(const std::string&, GroupType) const;
	std::vector<std::string> DisplayMembers(const std::string&, GroupType) const;
};

} //close namespace

#endif
