
#include "stdafx.h"

#include "statview.h"

//Utils
#include "dataconv.h"
//API tiers
#include "statref.h"
#include "statized.h"
#include "core.h"
//Diagnostics
#include "except.h"
#include "msg_core.h"
#include "msg_file.h"
#include "assert.h"

//Higher-tier functionality
#ifdef _BBDBAPI
#include "dbfile.h"
#include "dbctxt.h"
#ifdef _BBHOST
#include "ulsource.h"
#include "string2help.h" //V2.26
#endif
#endif

namespace dpt {

//define static members
StatRefTable* StatViewer::reftable = NULL;

#ifdef _WINDOWS
//#define _TRACE_STATS
#endif

//****************************************************************************************
void StatViewer::RegisterStat(const std::string& statname, Statisticized* holder)
{
	//Stat should be valid...
	reftable->GetRefInfo(statname);
	//...and not already registered
	assert(holder_xref.find(statname) == holder_xref.end());

	holder_xref[statname] = holder;
}

//****************************************************************************************
#ifdef _BBDBAPI
DatabaseFile* StatViewer::ConvertContext(DatabaseFileContext* context) const
{
	if (!context)
		return NULL;

	if (context->CastToGroup())
		throw Exception(CONTEXT_SINGLE_FILE_ONLY, 
			"File statistics can not be viewed in a group context");

	return context->CastToSingle()->GetDBFile();
}

//****************************************************************************************
_int64 StatViewer::View
(const std::string& statname, const StatLevel statlevel, DatabaseFile* f) const
#else
_int64 StatViewer::View
(const std::string& statname, const StatLevel statlevel) const
#endif
{
	//Check that the stat/level combination is valid in the overall reference table
	StatRefInfo ri = reftable->GetRefInfo(statname);

	//Locate the actual object that maintains this stat
	std::map<std::string, Statisticized*>::const_iterator mci = holder_xref.find(statname);
	if (mci == holder_xref.end()) { 
		//If there are session start-up errors all the statisticized objects may not
		//have been created yet;
		if (registration_complete)
			throw Exception(STAT_MISC, std::string
				("Bug! Statistic ")
				.append(statname)
				.append(" is not registered for VIEW/MONITOR"));
		else
			return 0;
	}

#ifdef _BBDBAPI
	//Not all stats are held at file level
	if (statlevel == STATLEVEL_FILE_CLOSE) {
		if (!ri.filestat)
			throw Exception(STAT_NOT_FILE, std::string
				("Statistic ").append(statname).append(" is not held at file level"));

		if (f == NULL)
			throw Exception(STAT_NEED_FILECONTEXT, 
				"File context is required to view file statistics");

		return f->ViewStat(statname);
	}
#endif

	Statisticized* obj = mci->second;
	return obj->ViewStat(statname, statlevel);
}

//****************************************************************************************
std::string StatViewer::UnformattedLine(const StatLevel lev
#ifdef _BBDBAPI
, DatabaseFile* f
#endif
) const
{
#ifdef _TRACE_STATS
	TRACE("Getting unformatted line\n");
#endif

	std::vector<std::string> statlist;
	reftable->GetAllStatNames(statlist);

	std::string result;
	for (size_t s = 0; s < statlist.size(); s++) {
		std::string sn(statlist[s]);

		//Ignore file non-file stats if file close level requested
		StatRefInfo ri = reftable->GetRefInfo(sn);
		if (lev == STATLEVEL_FILE_CLOSE && !ri.filestat)
			continue;

		_int64 sv = View(sn, lev
#ifdef _BBDBAPI
								,f
#endif
									);

		//This display only shows nonzero values
		if (sv > 0)
			result.append(sn).append(1, '=').append(util::Int64ToString(sv)).append(1, ' ');
	}

	return result;
}

//****************************************************************************************
void StatViewer::StartActivity(const std::string& newact, const ULSourceProgram* newsource)
{

#ifdef _TRACE_STATS
	TRACE("Start activity: %s \n", newact.c_str());
#endif

	//The compiler etc. should call EndActivity(), but if they don't it's OK - do it now.
	const ULSourceProgram* prevsource = current_source_code;
	if (current_activity.length() > 0)
		EndActivity();

	current_activity = newact;
	current_activity.resize(4, '*');

	if (newsource)
		current_source_code = newsource;
	else
		current_source_code = prevsource;

	//The CNCT stat is the only one that looks significantly wrong if we allow the "scraps" 
	//before a named activity to be bundled in with that activity, which is what happens.
	//This call clears some others, which is no bad thing (e.g. AUDIT), but less crucial.
	core->ClearSLStats();

#ifdef _TRACE_STATS
	TRACE("Started\n");
#endif
}

//****************************************************************************************
//V2.08.  Oct 07.  
//Temporary experimental patch for Mick Sheehy's occasional multi-thread crash problem.  
//It appears from audit trails he has sent that the bug may occur when one thread is 
//time-sliced in the middle of one of this function .  Reasoning:  Thread A crashes 
//reporting a corrupt stats ref table whilst collecting its SL stats, then thread B 
//reappears and successfully prints a set of SL stats before crashing next time.  My 
//hypothesis is that thread B had already collected its stats but was time sliced 
//before getting  to print them out.  If there is a problem with multithreading on STL 
//map iterators it could have quite serious implications for the rest of DPT.  
//Oh well - let's see if this stops the bug for now.
//****************************************************************************************
Lockable critsect_temp_endsl;

//****************************************************************************************
void StatViewer::EndActivity()
{
	LockingSentry ls(&critsect_temp_endsl);

#ifdef _TRACE_STATS
	TRACE("Ending activity: %s ...\n", current_activity.c_str());
#endif

	std::string last_activity = current_activity;

	current_activity = std::string();
	current_source_code = NULL;

	core->RefreshCPUStat(true);
	std::string statline = UnformattedLine(STATLEVEL_USER_SL);

	//Save for T REQUEST commands
	if (last_activity == "EVAL" || last_activity == "CMPL")
		t_request_cache = statline;

	//Write a SL line to the audit trail
	std::string auditline;
	auditline.append("$$$ USERID='").append(core->GetUserID());
	auditline.append("', LAST='").append(last_activity).append(1, '\'');

#ifdef _BBHOST
	const ULSourceProgram* last_source_code = current_source_code;
	if (last_source_code)
		auditline.append(", ").append("PROC='")
		.append(MakeString(last_source_code->GetESProcDetailsString(0))).append(1, '\'');
#endif

	core->AuditLine(auditline, "STT");
	core->AuditLine(statline, "STT");

	//Go round each registered statistic manager object, passing on this call.
	std::vector<Statisticized*>::const_iterator i;
	for (i = holders.begin(); i != holders.end(); i++)
		(*i)->ClearSLStats();

#ifdef _TRACE_STATS
	TRACE("Ended \n");
#endif
}

#ifdef _BBHOST
//****************************************************************************************
std::string StatViewer::CurrentProcName()
{
	if (current_source_code)
		return current_source_code->GetESProcName(0);
	else
		return std::string();
}

//****************************************************************************************
std::string StatViewer::CurrentProcFile()
{
	if (current_source_code)
		return current_source_code->GetESDirName(0);
	else
		return std::string();
}

//****************************************************************************************
std::string StatViewer::CurrentProcInfo()
{
	if (current_source_code)
		return MakeString(current_source_code->GetESProcDetailsString(0));
	else
		return std::string();
}
#endif

} //close namespace

