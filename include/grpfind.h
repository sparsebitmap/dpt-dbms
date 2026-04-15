//*****************************************************************************************
//This class is returned by Find() which is used by a few of the functions to locate groups
//based on name.  It caches the group pointer, container ID and iterator to cater for all
//possible uses.  However it's not publically usable as only internally can we ensure correct 
//locking whilst it exists.
//*****************************************************************************************

#if !defined(BB_GRPFIND)
#define BB_GRPFIND

#include <map>
#include <string>
#include "const_group.h"

namespace dpt {

class Group;

class GroupFindInfo {
public:
	GroupType type;
	Group* group;
	std::map<std::string, Group*>* def_container;
	std::map<std::string, Group*>::iterator def_container_iterator;

	GroupFindInfo(
		std::map<std::string, Group*>* c, 
		std::map<std::string, Group*>::iterator i);
};
	
} //close namespace

#endif
