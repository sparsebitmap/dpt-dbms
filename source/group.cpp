
#include "stdafx.h"

#include "group.h"

//Constants
#include "const_group.h"
//Utils
#include "pattern.h"
//Diagnostics
#include "except.h"
#include "msg_file.h"

namespace dpt {

//*****************************************************************************************
//This constructor validates group details.
//Here we check that none of the members is there twice, but apart from that we
//leave it up to the GroupServices object to ensure that 
//duplicate temp/perm etc. groups aren't created twice.
//*****************************************************************************************
Group::Group(const std::string& n, 
			 const std::vector<std::string>& m, const std::string& u, bool sysgen) 
: short_name(n), members(m), updtfile(u)
{
	//Screen the group name for for reserved combinations.  Use pattern matching for the 
	//forbidden prefixes, and STL find for the leading wildcard ones (purely a perf decision:
	//pattern matching would do it, but I can't force myself to use leading wildcards!)
	util::Pattern forbid("FILE,GROUP,ALL,!%*,!£*,!$*,CCA*,SYS*,OUT*,TAPE*");
	if (forbid.IsLike(short_name))
		throw Exception(GROUP_INVALID_NAME, "Invalid group name");

	//Allow asterisk for adhoc groups.  I chose this as the only forbidden character
	//in both group and file names, and helps with the test that ensures the user does
	//not use an adhoc group by name once it has been defined.
	const char* badchars = (sysgen) ? "=(-+);'/ " : "*=(-+);'/ ";

	if (short_name.find_first_of(badchars) != std::string::npos) 
		throw Exception(GROUP_INVALID_NAME, "Invalid group name");

	bool updtfile_ok = (updtfile.length() == 0);
	
	for (size_t x = 0; x < members.size(); x++) {

		//Check for dupe members
		for (size_t y = x+1; y < members.size(); y++) {
			if (members[x] == members[y]) 
				throw Exception(GROUP_DUPE_MEMBER,
					"Duplicate member in group definition");
		}

		//Ensure the update file is part of the group
		if (!updtfile_ok) {
			if (members[x] == updtfile)
				updtfile_ok = true;
		}
	}

	if (!updtfile_ok)
		throw Exception(GROUP_INVALID_UPDTFILE,
			"The specified UPDTFILE is not a member of the group");
}

//*****************************************************************************************
int Group::IsMember(const std::string& candidate) const
{
	for (size_t x = 0; x < members.size(); x++) {
		if (members[x] == candidate) return x;
	}

	return -1;
}

} //close namespace


