/*****************************************************************************************
Groups, as defined by the CREATE GROUP command.
These groups are used by both data file and procedure directory group contexts.
*****************************************************************************************/

#if !defined(BB_GROUP)
#define BB_GROUP

#include <string>
#include <vector>
#include "resource.h"

namespace dpt {

class SharedGroup;
class PrivateGroup;

//**************************************
class Group {
	std::string short_name;
	std::vector<std::string> members;
	std::string updtfile;

protected:
	Group(const std::string&, const std::vector<std::string>&, const std::string&, bool);

public:
	virtual ~Group() {};

	const std::string& ShortName() {return short_name;}
	std::vector<std::string> GetMemberList() {return members;}
	const std::string& GetUpdtfile() {return updtfile;}

	int IsMember(const std::string&) const;
	int NumMembers() {return members.size();}

	virtual SharedGroup* CastToPerm() {return NULL;}
	virtual PrivateGroup* CastToTemp() {return NULL;}
};

//**************************************
//V2: 25/9/06 Temp groups not now resources otherwise it blocks perm with same name.
class SharedGroup : public Group, public Resource {
	friend class GroupServices;

	SharedGroup(const std::string& n, const std::vector<std::string>& m, const std::string& u)
		: Group(n, m, u, false), Resource(std::string("GROUP_").append(n)) {}

	SharedGroup* CastToPerm() {return this;}
};

//**************************************
class PrivateGroup : public Group {
	Sharable lock;

	friend class GroupServices;
	PrivateGroup(const std::string& n, const std::vector<std::string>& m, 
					const std::string& u, bool s)
		: Group(n, m, u, s) {}

	PrivateGroup* CastToTemp() {return this;}

public:
	//Replace functionality that shared groups inherit from Resource.
	void GetOpenLock() {lock.AcquireShared();}
	void ReleaseOpenLock() {lock.ReleaseShared();}
	bool GetDeleteLock() {return lock.AttemptExclusive();}
};

} //close namespace

#endif
