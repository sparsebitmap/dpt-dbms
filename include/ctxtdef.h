/********************************************************************************************
These classes represent a context that is defined to the system, but may not be open.  They
are independent of the differences between procedure directory contexts and database
file contexts.
An "open" context (see ctxtopen.h) contains one of these.
********************************************************************************************/

#if !defined(BB_CTXTDEF)
#define BB_CTXTDEF

#include <string>

namespace dpt {

class Group;
class GroupServices;
class ContextSpecification;

//******************************************************************************************
//Base class
//******************************************************************************************
class DefinedContext {
protected:
	std::string short_name;
	DefinedContext(const std::string& s) : short_name(s) {}
	
public:
	//Factory function creates the derived versions
	static DefinedContext* Create(GroupServices*, const ContextSpecification&);

	virtual ~DefinedContext() {};
	virtual Group* GetGroup() const {return NULL;}

	const std::string& GetShortName() const {return short_name;}
	virtual std::string GetFullName() const = 0;
	virtual std::string GetCURFILE() const {return short_name;}
};

//*****************************************
//Groups
//*****************************************
class DefinedGroupContext : public DefinedContext {
	Group* grp;

protected:
	DefinedGroupContext(const std::string& s, Group* g);

public:
	virtual ~DefinedGroupContext();
	Group* GetGroup() const {return grp;}

	virtual std::string GetFullName() const = 0;
	//curfile shows up as null for groups on M204
	std::string GetCURFILE() const {return std::string();}
};

//*****************************************
//Temp groups
//*****************************************
class DefinedTempGroupContext : public DefinedGroupContext {
	friend class DefinedContext;
	DefinedTempGroupContext(const std::string& s, Group* g) : DefinedGroupContext(s, g) {}
public:
	std::string GetFullName() const {return std::string("TEMP GROUP ").append(short_name);}
};

//*****************************************
//Perm groups
//*****************************************
class DefinedPermGroupContext : public DefinedGroupContext {
	friend class DefinedContext;
	DefinedPermGroupContext(const std::string& s, Group* g) : DefinedGroupContext(s, g) {}
public:
	std::string GetFullName() const {return std::string("PERM GROUP ").append(short_name);}
};

//*****************************************
//Single files
//*****************************************
class DefinedSingleFileContext : public DefinedContext {
	friend class DefinedContext;
public:
	DefinedSingleFileContext(const std::string& s) : DefinedContext(s) {}
	std::string GetFullName() const {return std::string("FILE ").append(short_name);}
};

} //close namespace

#endif
