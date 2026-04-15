//**************************************************************************************
//Reference information for parameters.
//NOTE 1.
//It could be argued that this information should be held locally to the controlled
//objects.  It is quite nice to have it all in one place though.  In the case of whether
//or not a parameter is actually resettable at all, this information *IS* held in the
//controlled objects (it used to be here).  I can anticipate a time when it might be
//necessary to move all the information one way or the other, for example if access 
//levels come in at some point.  However for now this will do.
//NOTE 2.
//The parameter initialization has been split into parts, just as is done with
//messages, in order to save loading up settings for parameters that aren't relevant in
//API mode.  This will save memory and improve start-up time.  See file parmref1/2.cpp.
//**************************************************************************************

#include "stdafx.h"

#include "parmref.h"

//Utils
#include "dataconv.h"
#include "pattern.h"
//Diagnostics
#include "except.h"
#include "msg_core.h"
#include "assert.h"

namespace dpt {

//define static members
bool ParmRefTable::created = false;

//**************************************************************************************
void ParmRefTable::StoreEntry
(const char* name, const char* def, _int32 max, _int32 min, 
 const char* desc, ParmCategory cat, ParmResettability res)
{
	assert(data.find(name) == data.end());
	data[name] = ParmRefInfo(def, max, min, desc, cat, res);
}

//***************************************
void ParmRefTable::StoreEntry
(const char* name, const char* def, const char* patt, 
 const char* desc, ParmCategory cat, ParmResettability res)
{
	assert(data.find(name) == data.end());
	data[name] = ParmRefInfo(def, patt, desc, cat, res);
}

//***************************************
void ParmRefTable::StoreEntry
(const char* name, const char* desc, ParmType type, ParmCategory cat)
{
	assert(data.find(name) == data.end());
	data[name] = ParmRefInfo(desc, type, cat);
}

//**************************************************************************************
ParmRefTable::ParmRefTable()
{

if (created) 
	throw Exception(MISC_SINGLETON, "There can be only one parm RefTable object");
created = true;

//Note that in all non-resettable parameters, the validation pattern is set to "*", so that
//attempts to reset them are always passed to the target object, rather than being rejected
//with a confusing preliminary validation error message.

//*******************
//User Parameters
//*******************

//M204 emulated
//=============
StoreEntry("ACCOUNT", "", "", "Access-control account", user, never_resettable); //V3.03
StoreEntry("ERRSAVE", "4", 100, 0, "# of saved error messages", user);
StoreEntry("MCNCT", "-1", INT_MAX, -1, "Max wall clock time per request", user);
StoreEntry("MCPU", "-1", INT_MAX, -1, "Max CPU time per request", user);
//V2.03 Changed max to a number with all bits on - router does bit validation now.
StoreEntry("MSGCTL", "0", 63, 0, "Message format/suppression/routing options", user);
StoreEntry("PRIORITY", "0", 0, 0 , "User thread priority", user, never_resettable);
StoreEntry("UPRIV", "", "", "Current access-control privileges", user, never_resettable); //V3.03
StoreEntry("USERID", "", "@/0-30(+)", "User ID/name", user); //(V2.16 see logctl command)
StoreEntry("USERNO", "User number", num, user);


//*******************
//System parameters
//*******************

//M204 emulated
//=============
StoreEntry("NUSERS", "99", 99, 1, "Max # of concurrent users", system, inifile_only);
StoreEntry("OPSYS", "", "*", "Operating system", system, never_resettable);

//Baby204 specific
//================
StoreEntry("AUDCTL", "10", 0, 0, "Audit trail layout options", system, inifile_only);
StoreEntry("AUDKEEP", "3", 0, 0, "Previous audit trail generations kept", system, inifile_only);
StoreEntry("CODESA2E", "", "/256(+)", "ASCII->EBCDIC code page string", system, inifile_only); //V2.12
StoreEntry("CODESE2A", "", "/256(+)", "EBCDIC->ASCII code page string", system, inifile_only); //V2.12
StoreEntry("CSTFLAGS", "0", USHRT_MAX, 0, "Custom processing disabler flags", system);
StoreEntry("NCPUS", "", INT_MAX, 1, "Number of CPUs in host machine", system, never_resettable);
StoreEntry("RETCODE", "0", 0, 0, "Current anticipated host system return code", system);
StoreEntry("SYSNAME", "DPToolkit", "@/0-30(+)", "Host system name", system, inifile_only);
StoreEntry("VERSDPT", "3.06", "*", "DPT version", system, never_resettable); //BBVERSION

//**************************************************************************************
//Extra tier parms
//**************************************************************************************
#ifdef _BBDBAPI
	DBAPIExtraConstructor();
#endif
#ifdef _BBHOST
	FullHostExtraConstructor();
#endif
}

//**************************************************************************************
//Inquiry function, used for validation in various situations
//**************************************************************************************
const ParmRefInfo& ParmRefTable::GetRefInfo(const std::string& parmname) const 
{
	std::map<std::string, ParmRefInfo>::const_iterator mci = data.find(parmname);

	//Throw it out if the parameter is invalid
	if (mci == data.end()) {
		throw Exception(PARM_BADNAME, 
			std::string("No such parameter: ").append(parmname));
	}

	//Also throw it out (but more nicely) if the parameter is not emulated
	if (mci->second.type == not_emulated) {
		throw Exception(PARM_NOT_EMULATED, 
			std::string("Parameter ")
			.append(parmname)
			.append(" is unsupported (")
			.append(mci->second.description).append(1, ')'));
	}
	
	return mci->second;
}

//****************************************************************************************
int ParmRefTable::GetParmsInCategory(std::vector<std::string>& result, 
									  const std::string& category) const
{
	ParmCategory c;
	if		(category == "SYSTEM")	
		c = system;
	else if (category == "USER")	
		c = user;
	else if (category == "FPARMS")	
		c = fparms;
	else if (category == "TABLES" || category == "TABLE")	
		c = tables;
	else if (category == "ALL")		
		c = all;
	else if (category == "UNSUPPORTED")		
		c = nocat;
	
	//Empty return set means the input parm is not a valid category, but may be a parm name
	else 
		return 0;

	int numparms = 0;

	std::map<std::string, ParmRefInfo>::const_iterator ci;
	for (ci = data.begin(); ci != data.end(); ci++) {
		ParmCategory thisparmcat = (ci->second).category;

		//don't return non-emulated parm names
		if (thisparmcat == nocat && c != nocat) 
			continue;	

		if (c == all || c == thisparmcat) {
			result.push_back(ci->first);
			numparms++;
		}
	}

	return numparms;
}

//****************************************************************************************
//This checks the parameter name and that the value is acceptable.  If
//the value is outside the valid numeric range, it substitutes the max or min.
//****************************************************************************************
std::string ParmRefTable::ValidateResetDetails
(const std::string& parmname, const std::string& newval, bool from_inifile) const
{
	//The returned value may get tweaked to be the maximum or minimum
	std::string actualval = newval;

	//Is it a valid parameter?  (If not, the exception will pass straight through here)
	ParmRefInfo ref = GetRefInfo(parmname);

	//This saves every parameterized object having the same bit of code in there.
	if (ref.resettability == never_resettable)
		throw Exception(PARM_NOTRESETTABLE, std::string
			(parmname).append(" can not be reset"));

	//Similarly...
	if (from_inifile && ref.resettability == not_inifile)
		throw Exception(PARM_NOTRESETTABLE, std::string
			(parmname).append(" can not be set in the ini file (but you can use RESET)"));

	if (!from_inifile && ref.resettability == inifile_only)
		throw Exception(PARM_NOTRESETTABLE, std::string
			(parmname).append(" is not resettable (but can be set in the ini file)"));


	//Is the new value allowable? (Different validation for alpha/num).  Perhaps this is
	//not good C++ style, but simplifies processing when we want to use the maximum,
	//say, when the user gives a too-high value. 
	if (ref.type == num) {

		//V3.0 Null string was allowed through as zero which isn't bad really, but it
		//should at least give the warning message.  Never noticed outside the API 
		//situation because at command level you have to say: RESET parm C''
		if (newval.length() == 0)
			actualval = "0";

		//convert to a number for checking
		char* terminator;
		_int32 newnum = strtoul(newval.c_str(),&terminator,10);
		if (*terminator) {
			throw Exception 
				(PARM_BADVALUE, std::string
				("Parameter ")
				.append(parmname)
				.append(" not set - value is not numeric: ").append(newval));
		}

		//Is it within the allowed range?  If not use the maximum/minimum
		if (newnum < ref.minimum_value)
			actualval = util::IntToString(ref.minimum_value);

		else if (ref.maximum_value != 0 && newnum > ref.maximum_value)
			actualval = util::IntToString(ref.maximum_value);
	}
	//Alpha valued parameters
	else {
		util::Pattern p;

		//This should only ever go wrong during testing
		try {
			p.SetPattern(ref.validation_pattern);
		}
		catch (Exception& e) {
			throw Exception(BUG_MISC, 
			std::string("Bug: bad parm ref info: ").append(e.What()));
		}

		if (!p.IsLike(newval)) {
			throw Exception(PARM_BADVALUE, std::string
				("Parameter ")
				.append(parmname)
				.append(" not set - value '")
				.append(newval)
				.append("' is not of the format '")
				.append(ref.validation_pattern)
				.append(1, '\''));
		}
	}

	//Looks OK then
	return actualval;
}

//****************************************************************************************
//Reference info table entry object.  Various constructors for neatness above.
//****************************************************************************************
ParmRefInfo::ParmRefInfo(
  const char*	d, 
  _int32		mx, 
  _int32		mn,
  const char*	s,	
  ParmCategory	ct,
  ParmResettability	r
)
: default_value			(d),
  validation_pattern	(""),
  maximum_value			(mx),
  minimum_value			(mn),
  description			(s),
  type					(num),
  category				(ct),
  resettability			(r)
{}

//****************************************************************************************
ParmRefInfo::ParmRefInfo(
  const char*	d, 
  const char*	vp,	
  const char*	s,	
  ParmCategory	ct,
  ParmResettability	r
)
: default_value			(d),
  validation_pattern	(vp),
  maximum_value			(0),
  minimum_value			(0),
  description			(s),
  type					(alpha),
  category				(ct),
  resettability			(r)
{}

//****************************************************************************************
ParmRefInfo::ParmRefInfo(
  const char*	s,	
  ParmType		t,
  ParmCategory	ct
)
: default_value			(""),
  validation_pattern	(""),
  maximum_value			(0),
  minimum_value			(0),
  description			(s),
  type					(t),
  category				(ct),
  resettability			(never_resettable)
{}


} //close namespace

