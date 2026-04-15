
#include "stdafx.h"

#include "grpserv.h"

//Utils
#include "dataconv.h"
//API tiers
#include "core.h"
#include "msgroute.h"
#include "group.h"
#include "grpfind.h"
//Diagnostics
#include "except.h"
#include "msg_file.h"

namespace dpt {

//Define static objects
std::map<std::string, Group*> GroupServices::perm_group_definitions = std::map<std::string, Group*>();
ThreadSafeLong GroupServices::instances = ThreadSafeLong();

#ifdef _DEBUG_LOCKS
Sharable GroupServices::groupdef_lock = Sharable("Group defs");
#else
Sharable GroupServices::groupdef_lock = Sharable();
#endif

//*****************************************************************************************
GroupServices::~GroupServices()
{
	//Delete any remaining temp group definitions the user has
	std::map<std::string, Group*>::iterator gi;
	for (gi = temp_group_definitions.begin(); gi != temp_group_definitions.end(); gi++) {
		core->GetRouter()->Issue(GROUP_DELETED_FINAL, 
			std::string("Temp group deleted at logoff: ").append(gi->second->ShortName()));
		delete gi->second;
	}

	//Delete system-wide (perm) groups if this is the last user
	instances.Dec();
	if (instances.Value() == 0) {
		for (gi = perm_group_definitions.begin(); gi != perm_group_definitions.end(); gi++) {
			core->GetRouter()->Issue(GROUP_DELETED_FINAL, 
				std::string("Perm group deleted at system closedown: ")
					.append(gi->second->ShortName()));
			delete gi->second;
		}
	}
}

//*****************************************************************************************
void GroupServices::Create_S
(const std::string& name, const std::vector<std::string>& members, 
 GroupType type, const std::string& updtfile, bool sysgen)
{
	const char* lit = (type == PERM_GROUP) ? "Perm " : "Temp ";

	LockingSentry s(&groupdef_lock); //slight overkill if temp group

	//We can use Find() here to check for prior existence, because the CreateGroupCommand
	//will already have inserted TEMP as a concrete type if the user didn't give one.  i.e.
	//By the time we get here there is never an unspecified group type.
	try {
		Find(name, type);
		throw Exception(GROUP_ALREADY_EXISTS, std::string(lit).
			append("group ").append(name).append(" already exists"));
	}
	catch (Exception& e) {
		if (e.Code() != GROUP_NONEXISTENT) throw;
	}

	//Doesn't exist
	//V2. Create different class of group for temps and perms (see group.h).
	if (type == PERM_GROUP) 
		perm_group_definitions[name] = new SharedGroup(name, members, updtfile);
	else 
		temp_group_definitions[name] = new PrivateGroup(name, members, updtfile, sysgen);

	core->GetRouter()->Issue(GROUP_CREATED, std::string
		(lit).append("group ").append(name).append(" created"));
}

//*****************************************************************************************
void GroupServices::CreateAdHoc(const std::string& n, const std::vector<std::string>& m)
{
	Create_S(n, m, TEMP_GROUP, std::string(), true);
}

//*****************************************************************************************
//This is used by DELETE GROUP, DISPLAY GROUP, and whenever opening a context.  It locates 
//either a specified temp or perm group, or gets the temp by preference if "ANY" is given.
//*****************************************************************************************
GroupFindInfo GroupServices::Find(const std::string& name, GroupType type) const
{
	//Rare case of casting off const here, to allow the non-const findinfo to be created, 
	//which may then be used to delete or view entries in other functions.
	std::map<std::string, Group*>* non_const_defs;
	std::map<std::string, Group*>::iterator gi;

	//First have a go at temp groups
	if (type != PERM_GROUP) {
		non_const_defs = const_cast<std::map<std::string, Group*>* >(&temp_group_definitions);
		gi = non_const_defs->find(name);

		if (gi != non_const_defs->end()) 
			return GroupFindInfo(non_const_defs, gi);

		//If they explicitly said TEMP_GROUP, give up
		if (type == TEMP_GROUP) 
			throw Exception(GROUP_NONEXISTENT, std::string
				("Temp group ").append(name).append(" does not exist"));
	}

	//Otherwise have a go at a perm group
	non_const_defs = const_cast<std::map<std::string, Group*>* >(&perm_group_definitions);
	gi = non_const_defs->find(name);

	if (gi != non_const_defs->end()) 
		return GroupFindInfo(non_const_defs, gi);

	//finally give up
	if (type == PERM_GROUP) 
		throw Exception(GROUP_NONEXISTENT, std::string
			("Perm group ").append(name).append(" does not exist"));
	else
		throw Exception(GROUP_NONEXISTENT, std::string
			("Neither a temp nor a perm group exists with name ").append(name));
}

//*****************************************************************************************
GroupType GroupServices::Delete(const std::string& name, GroupType type)
{
	//Ensure nobody else deletes/creates groups while do this (overkill for temp groups)
	LockingSentry s(&groupdef_lock);

	//Locate the desired group (this function will throw if appropriate)
	GroupFindInfo info = Find(name, type);

	//First delete the group itself if it's not in use (e.g. open)
	//V2 - only perm groups now are resources (see group.h).
	bool locked = false;
	if (info.type == PERM_GROUP)
		locked = info.group->CastToPerm()->Try(BOOL_EXCL); 
	else
		locked = info.group->CastToTemp()->GetDeleteLock(); 

	if (!locked) {
		throw Exception(GROUP_IN_USE, std::string
			("Group ").append(name).append(" is in use - not deleted"));
	}

	delete info.group;

	//Then remove all trace of it from the group directory  
	info.def_container->erase(info.def_container_iterator);

	const char* lit = (info.type == PERM_GROUP) ? "Perm " : "Temp ";
	core->GetRouter()->Issue(GROUP_DELETED, std::string
		(lit).append("group ").append(name).append(" deleted"));

	return info.type;
}

//*****************************************************************************************
std::vector<std::string> GroupServices::List(GroupType type) const
{
	std::vector<std::string> result;

	//Decide whether to look at the perm group map or the temp group map
	const std::map<std::string, Group*>* defs;
	std::map<std::string, Group*>::const_iterator gi;

	SharingSentry s(&groupdef_lock);  //slight overkill if temp group

	if (type != PERM_GROUP)
		defs = &temp_group_definitions;
	else
		defs = &perm_group_definitions;

	for (gi = defs->begin(); gi != defs->end(); gi++) {
		result.push_back(gi->first);
	}

	return result;
}

//*****************************************************************************************
std::vector<std::string> GroupServices::DisplayMembers
(const std::string& name, GroupType type) const
{
	return Find(name, type).group->GetMemberList();
}

//*****************************************************************************************
GroupType GroupServices::DisplayType(const std::string& name, GroupType type) const
{
	return Find(name, type).type;
}

//*****************************************************************************************
std::string GroupServices::DisplayUpdtfile(const std::string& name, GroupType type) const
{
	return Find(name, type).group->GetUpdtfile();
}

//*****************************************************************************************
GroupFindInfo::GroupFindInfo
(std::map<std::string, Group*>* c, 
 std::map<std::string, Group*>::iterator i)
: group(i->second), def_container(c), def_container_iterator(i)
{
	if (def_container == GroupServices::PermContainer()) 
		type = PERM_GROUP;
	else 
		type = TEMP_GROUP;
}

} //close namespace


