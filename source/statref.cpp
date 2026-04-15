//**************************************************************************************
//This file is spli, just as with msgref and parmref.  See those cpp files for
//more comments.
//**************************************************************************************

#include "stdafx.h"

#include "statref.h"

//Diagnostics
#include "except.h"
#include "msg_core.h"

namespace dpt {

//**************************************************************************************
//define static class members
//**************************************************************************************
bool StatRefTable::created = false;

//**************************************************************************************
void StatRefTable::StoreEntry(const char* name, const char* desc, bool filestat)
{
	data[name] = StatRefInfo(desc, filestat);
}

//**************************************************************************************
StatRefTable::StatRefTable()
{

if (created) 
	throw Exception(MISC_SINGLETON, "There can be only one stats ref table object");
created = true;

StoreEntry("AUDIT", "Total lines written to audit trail");
StoreEntry("CNCT", "Elapsed wall clock time in millisec");
StoreEntry("CPU", "CPU time in millisec (if available from OS)");

//**************************************************************************************
//Extra tier stats
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
StatRefInfo StatRefTable::GetRefInfo(const std::string& statname) const 
{
	std::map<std::string, StatRefInfo>::const_iterator mci = data.find(statname);

	//Throw it out if the stat is invalid
	if (mci == data.end()) {
		throw Exception(STAT_BADNAME, 
			std::string("No such statistic: ").append(statname));
	}

	//Also throw it out (but more nicely) if the stat is not emulated
	//Not needed I think.  $VIEW returns zero either way.
//	if (mci->second.type == not_emulated) {
//		throw Exception(STAT_NOT_EMULATED, 
//			std::string("Statistic ")
//			.append(statname)
//			.append(" is not emulated. ")
//			.append(mci->second.description));
//	}
	
	return mci->second;
}

//**************************************************************************************
void StatRefTable::GetAllStatNames(std::vector<std::string>& result) const 
{
	std::map<std::string, StatRefInfo>::const_iterator i;
	for (i = data.begin(); i != data.end(); i++)
		result.push_back(i->first);

}

} //close namespace

