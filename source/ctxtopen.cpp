
#include "stdafx.h"

#include "ctxtopen.h"

//API tiers
#include "ctxtdef.h"
#include "group.h"
//Diagnostics
#include "except.h"
#include "msg_file.h"

namespace dpt {

OpenableContext::~OpenableContext()
{
	delete defined_context;
}

//*****************************************************************************************
//Group version functions
//These are far more intertesting than the single file versions, because they have to 
//create and destroy single file contexts too.  This is done by calling virtual functions 
//which are overridden on the database and procedure services sides in order to manipulate
//the appropriate type of single file context object.
//*****************************************************************************************
bool GroupOpenableContext::Open()
{
	Group* g = defined_context->GetGroup();

	try {
		std::vector<std::string> members_copy = g->GetMemberList();
		for (size_t x = 0; x < members_copy.size(); ++x) {
			SingleFileOpenableContext* sec = OpenSingleFileSecondary(members_copy[x]);
			member_contexts.push_back(sec);

			if (members_copy[x] == g->GetUpdtfile())
				updtfile_context = sec;
		}
	}
	//Re-close any opened members if any one fails
	catch (...) {
		for (size_t y = 0; y < member_contexts.size(); ++y)
			//This can't fail.  The open worked above so the single file context must be 
			//open against this group.  Child objects will not have had time to be created,
			//so we don't need to specify a force option here.
			CloseSingleFileSecondary(member_contexts[y]);

		updtfile_context = NULL;
		throw;
	}

	//No confusion about single file contexts here - see single file version below
	return true;
}

//*****************************************************************************************
//Here we attempt to close ALL the members of a group, regardless of whether they were
//opened when the group was opened, since even if they were open as a single file or in 
//another group before, they may not be now.
//This function never fails.  If any members fail to close they are converted into single
//file contexts in their own right and left open.
//*****************************************************************************************
bool GroupOpenableContext::Close(bool force)
{
	for (size_t x = 0; x < member_contexts.size(); ++x) {
		SingleFileOpenableContext* sec = member_contexts[x];

		try {
			//True or false is OK as far as we're concerned here.  In fact I can't remember
			//why I made it a bool function.
			CloseSingleFileSecondary(sec, force);
		}
		//Exception means the context was not closed because open child objects remain
		//against it.  This is a subtle point.  On M204 you get a "group closed" message,
		//but one or all of the individual files of the group remain open.  The way we
		//handle it here is to convert the context into one that's open in its own right
		//as a single file.  This should be close enough for most people.
		catch (...) {
			sec->Close(this);
			sec->Open(NULL);
		}
	}

	//Groups are always completely closed (cf single files below)
	member_contexts.clear();
	updtfile_context = NULL;
	return true;
}

//*****************************************************************************************
//Single file version functions.
//*****************************************************************************************
bool SingleFileOpenableContext::Open(const GroupOpenableContext* parent_group)
{
	//Called via GroupOpenableContext::Open().  
	if (parent_group) {

		//NB. Any given group can't be opened twice so this is OK
		parent_groups.insert(parent_group);
		return true;
	}

	//Called via ProcServices::OpenContext().
	else {

		//It may be useful for the caller to know if they must close it again
		if (open_as_file)
			return false;

		open_as_file = true;
		return true;
	}
}

//*****************************************************************************************
//This function predicts the outcome of the next one
//*****************************************************************************************
bool SingleFileOpenableContext::PreClose(const GroupOpenableContext* parent_group)
{
	if (parent_group)
		return (!open_as_file && parent_groups.size() <= 1);
	else
		return (parent_groups.size() == 0);

}

//*****************************************************************************************
void SingleFileOpenableContext::Close(const GroupOpenableContext* parent_group)
{
	//Called via GroupOpenableContext::Close()
	if (parent_group)
		parent_groups.erase(parent_group);

	//Called via ProcServices::CloseContext()
	else
		//NB. The user may try to close one of the file contexts only owned by a group, 
		//although it would take a mischievous sequence of calls to do so.  Anyway, we 
		//just do nothing
		open_as_file = false;
}

} //close namespace
