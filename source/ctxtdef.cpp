
#include "stdafx.h"

#include "ctxtdef.h"

//Constants
#include "const_group.h"
//API tiers
#include "grpserv.h"
#include "grpfind.h"
#include "group.h"
#include "ctxtspec.h"
//Diagnostics
#include "except.h"
#include "msg_file.h"

namespace dpt {

//*****************************************************************************************
//Factory constructor for defined context objects
//*****************************************************************************************
DefinedContext* DefinedContext::Create
(GroupServices* groups, const ContextSpecification& spec)
{
	//Firstly are we going to have a go at group context?
	GroupType gt = NO_GROUP;
	if (spec.level == ContextSpecification::cspec_temp_group) 
		gt = TEMP_GROUP;
	else if (spec.level == ContextSpecification::cspec_perm_group) 
		gt = PERM_GROUP;
	else if (spec.level == ContextSpecification::cspec_dynamic_group) 
		gt = ANY_GROUP; 
	else if (spec.level == ContextSpecification::cspec_dynamic_any) 
		gt = ANY_GROUP; 

	if (gt != NO_GROUP) {
		//GroupServices will handle groups not existing.  However, if we tried for a group
		//with dynamic_any (the most common case in fact), we have to trap that here.
		try {
			GroupFindInfo group_info = groups->Find(spec.short_name, gt);

			if (group_info.type == PERM_GROUP) 
				return new DefinedPermGroupContext(spec.short_name, group_info.group);
			else 
				return new DefinedTempGroupContext(spec.short_name, group_info.group);
		}

		//I don't really like this as it means throwing an exception as a matter of course
		//in the most common case.  However, opening a context is not a very frequent thing
		//in a session, and is not performance-sensitive, so I'm living with it.  Possibly
		//recode at some point.  The group find function is used in 2 or 3 places - all
		//would need attention.
		catch (Exception& e) {
			if (e.Code() != GROUP_NONEXISTENT) 
				throw;
			if (spec.level != ContextSpecification::cspec_dynamic_any && 
					spec.level != ContextSpecification::cspec_single_file) 
				throw;
		}
	}

	//Ok, so they said FILE, or they said nothing and there was no group above.  Here we
	//assume it's a file, and let the caller (proc or db services) decide if it exists
	//based on their own criteria (i.e. different for DB files and proc directories).
	return new DefinedSingleFileContext(spec.short_name);
}

//*****************************************************************************************
DefinedGroupContext::DefinedGroupContext(const std::string& s, Group* g) 
: DefinedContext(s), grp(g) 
{
	//V2 - 25/9/06 (see group.h)
	if (g->CastToPerm())
		g->CastToPerm()->Get(BOOL_SHR);
	else
		g->CastToTemp()->GetOpenLock();

}

DefinedGroupContext::~DefinedGroupContext() 
{
	//V2 - 25/9/06 (see group.h)
	if (grp->CastToPerm())
		grp->CastToPerm()->Release();
	else
		grp->CastToTemp()->ReleaseOpenLock();
}


} //close namespace


