
#include "stdafx.h"

#include "parmvr.h"

#include "assert.h"

//Utils
#include "dataconv.h"
//API tiers
#ifdef _BBDBAPI
#include "dbfile.h"
#include "dbctxt.h"
#endif
#include "parmref.h"
#include "parmized.h"
//Diagnostics
#include "except.h"
#include "msg_core.h"
#include "msg_file.h"

namespace dpt {

//define static members
ParmRefTable*		ViewerResetter::reftable = NULL;

//****************************************************************************************
//When objects are created within the system they register themselves here so that
//VIEW and RESET can find the "host" object for each parameter.
//****************************************************************************************
void ViewerResetter::Register(const std::string& parmname, Parameterized* object)
{
	//Parameter should be valid...
	reftable->GetRefInfo(parmname);
	//...and not already registered
	assert(data.find(parmname) == data.end());

	data[parmname] = object;
}

//****************************************************************************************
//RESET
//****************************************************************************************
std::string ViewerResetter::Reset(const std::string& parmname, const std::string& newvalue
#ifdef _BBDBAPI
, DatabaseFileContext* context
#endif
)
{
	//Check that the parameter and new value are valid in the overall reference table,
	std::string val = reftable->ValidateResetDetails(parmname, newvalue);
	ParmRefInfo info = reftable->GetRefInfo(parmname);

	//Locate the actual parameterized object.  If it's a file parameter, a context must
	//be passed in.  This is preferred to passing in the DatabaseFile* itself
	//to save API users needing to know about that object.
	Parameterized* obj = NULL;
	std::map<std::string, Parameterized*>::iterator mi = data.find(parmname);
	if (mi != data.end())
		obj = mi->second;

	//We do this block if a registered parameterized object can not be found.
	//File parameters do not have to be registered, since the caller must tell 
	//us which file (of maybe many) they are interested in.
#ifdef _BBDBAPI
	if (!obj) {

		if (context) {
			if (context->CastToGroup())
				throw Exception(CONTEXT_SINGLE_FILE_ONLY, 
					"File parameters can not be reset in a group context");

			DatabaseFile* f = context->CastToSingle()->GetDBFile();
			return f->ResetParm(context->CastToSingle(), parmname, val);
		}
		else {
			if (info.category == fparms || info.category == tables)
				throw Exception(PARM_NEEDS_FILECONTEXT, std::string
					("Database file context is required to access parameter ")
					.append(parmname));
		}
	}
#endif

	//This should never happen, or at least only during development
	if (!obj)
		throw Exception(PARM_MISC, std::string
			("Bug! Parameter ")
			.append(parmname)
			.append(" is not registered for VIEW/RESET"));

	//The attempted new value may have been replaced with a max or min above, 
	//or the actual parameterized object may apply more subtle criteria too.
	return obj->ResetParm(parmname, val);
}

//****************************************************************************************
//VIEW
//****************************************************************************************
std::string ViewerResetter::View(const std::string& parmname, bool format
#ifdef _BBDBAPI
, DatabaseFileContext* context
#endif
) const
{
	//Check that the parameter is valid in the overall reference table
	ParmRefInfo info = reftable->GetRefInfo(parmname);

	//See comments in Reset() above.
	Parameterized* obj = NULL;
	std::map<std::string, Parameterized*>::const_iterator mi = data.find(parmname);
	if (mi != data.end())
		obj = mi->second;

	//-------------------------------------
	//File parameters
#ifdef _BBDBAPI
	if (!obj) {
		if (context) {
			//See comments in Reset() above
			if (context->CastToGroup())
				throw Exception(CONTEXT_SINGLE_FILE_ONLY, 
					"File parameters can not be viewed in a group context");

			SingleDatabaseFileContext* sfc = context->CastToSingle();
			DatabaseFile* f = sfc->GetDBFile();
			std::string result = f->ViewParm(sfc, parmname, format);
			if (format) if (result.length() == 0) result = "C\'\'";
			return result;
		}
		else {
			if (info.category == fparms || info.category == tables)
				throw Exception(PARM_NEEDS_FILECONTEXT, std::string
					("Database file context is required to access parameter ")
					.append(parmname));
		}
	}
#endif

	//This should never happen, or at least only during development
	if (!obj)
		throw Exception(PARM_MISC, std::string
			("Bug! Parameter ")
			.append(parmname)
			.append(" is not registered for VIEW/RESET"));
	
	//This will return a formatted result or not depending on the caller
	std::string result = obj->ViewParm(parmname, format);

	//Nicer format for null values (zero numbers always return "0" so not relevant here)
	if (format) if (result.length() == 0) result = "C\'\'";

	return result;
}

#ifdef _BBDBAPI
int ViewerResetter::ViewAsInt(const std::string& p, DatabaseFileContext* f) const {
		return util::StringToInt(View(p, false, f));}
#else
int ViewerResetter::ViewAsInt(const std::string& p) const {return util::StringToInt(View(p, false));}
#endif

} //close namespace
