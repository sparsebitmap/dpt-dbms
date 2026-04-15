/*****************************************************************************************
Interface to group definitions.  The same set of groups is used by both procedure file
commands (INCLUDE, DISPLAY PROC etc.), and data file commands (DISPLAY FILE etc.).
*****************************************************************************************/

#if !defined(BB_GRPSERV)
#define BB_GRPSERV

#include <string>
#include <vector>
#include <map>
#include "lockable.h"
#include "const_group.h"

namespace dpt {

class DatabaseServices;
class DefinedContext;
class Group;
class GroupFindInfo;
class CoreServices;

//*****************************************************************************************
class GroupServices
{
	CoreServices* core;
	static ThreadSafeLong instances;

	std::map<std::string, Group*> temp_group_definitions;
	static std::map<std::string, Group*> perm_group_definitions;
	static Sharable groupdef_lock;

	friend class GroupFindInfo;
	static std::map<std::string, Group*>* PermContainer() {return &perm_group_definitions;}

	friend class DatabaseServices;
	GroupServices(CoreServices* c) : core(c) {instances.Inc();}
	~GroupServices();

	friend class DefinedContext;
	GroupFindInfo Find(const std::string&, GroupType) const;

	friend class AdHocGroupContextableStatement;
	void CreateAdHoc(const std::string&, const std::vector<std::string>&);
	void Create_S(const std::string&, const std::vector<std::string>&, GroupType, 
									const std::string&, bool);

public:
	CoreServices* Core() const {return core;}

	//Creation and deletion
	void Create(const std::string& n, const std::vector<std::string>& m, GroupType t, 
		const std::string& u = std::string()) {Create_S(n, m, t, u, false);}
	GroupType Delete(const std::string&, GroupType);  //returns actual type deleted

	//TEMP_GROUP is assumed unless PERM_GROUP specified
	std::vector<std::string> List(GroupType) const;
	//A TEMP group takes preference here if ANY_GROUP is specified, although the result
	//will not make clear which type it is.
	std::vector<std::string> DisplayMembers(const std::string&, GroupType) const;
	//See comment above
	GroupType DisplayType(const std::string&, GroupType) const;
	std::string DisplayUpdtfile(const std::string&, GroupType) const;
};

} //close namespace

#endif
