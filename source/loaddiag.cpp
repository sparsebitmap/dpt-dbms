
#include "stdafx.h"

#include "loaddiag.h"

//Utils
//API Tiers
#include "statview.h"
#include "core.h"
//Diagnostics

namespace dpt {

void LoadDiagnostics::Audit(CoreServices* core, const std::string& msg, int lev) 
{
	if (level >= lev) 
		core->AuditLine(msg, "INF");
}

void LoadDiagnostics::StartSLStats(StatViewer* stats, const std::string& activity) 
{
	if (level >= LOADDIAG_VERBOSE)
		stats->StartActivity(activity);
}

} //close namespace


