/*******************************************************************************************
These classes encapsulate the functionality of single file and group contexts, and are 
elaborated on by versions in procedure services and database services, for those two 
types of contexts.
In other words when you open a group it also opens the member files as usable contexts in 
their own right, etc.
*******************************************************************************************/

#if !defined(BB_CTXTOPEN)
#define BB_CTXTOPEN

#include <set>
#include <string>
#include <vector>

namespace dpt {

class DefinedContext;
class SingleFileOpenableContext;

//*****************************************************************************************
//Base class
//*****************************************************************************************
class OpenableContext {
protected:
	DefinedContext* defined_context;
	OpenableContext(DefinedContext* dc) : defined_context(dc) {}
	virtual ~OpenableContext();
};

//*****************************************
//Groups
//*****************************************
class GroupOpenableContext : public OpenableContext {

	//These are used to create/delete member file contexts, and are overridden by the
	//proc and DB services versions.
	virtual SingleFileOpenableContext* OpenSingleFileSecondary(const std::string&) const = 0;
	virtual bool CloseSingleFileSecondary(SingleFileOpenableContext*, bool = false) const = 0;

protected:
	//This assists with many group algorithms like Close() and Find()
	std::vector<SingleFileOpenableContext*> member_contexts;
	SingleFileOpenableContext* updtfile_context;

protected:
	GroupOpenableContext(DefinedContext* dc) : OpenableContext(dc), updtfile_context(NULL) {}

	//*IMPORTANT* these are not virtual.  Open() for a group context implies looping and
	//opening all the individual files as "secondaries".
	bool Open();
	bool Close(bool = false);
};

//*****************************************
//Single files
//*****************************************
class SingleFileOpenableContext : public OpenableContext {
	bool open_as_file;
	std::set<const GroupOpenableContext*> parent_groups;

protected:
	SingleFileOpenableContext(DefinedContext* dc) : OpenableContext(dc), open_as_file(false) {}

	//*IMPORTANT* these are not virtual.  Open() for a single file means little more than 
	//setting some flags.
	friend class GroupOpenableContext;
	bool Open(const GroupOpenableContext*);
	bool PreClose(const GroupOpenableContext*);
	void Close(const GroupOpenableContext*);

public:
	int GroupOpenCount() const {return parent_groups.size();}
	bool IsOpenAsFile() const {return open_as_file;}
};

} //close namespace

#endif
