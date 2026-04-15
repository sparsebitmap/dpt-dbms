
#include "stdafx.h"

#include "progress.h"

//Constants
//Utils
//API Tiers
//Diagnostics
#include "except.h"
#include "msg_util.h"

namespace dpt {

//**********************************************************************************
ProgressReportableActivity::ProgressReportableActivity
(const std::string& n, ProgressFunction pf, void* pc, ProgressReportableActivity* p)
: progress_function(pf), progress_context(pc), parent(p), name(n)
{
	if (!parent) 
		type = PROGRESS_OPERATION;
	else if (!parent->parent) 
		type = PROGRESS_ACTIVITY_GROUP;
	else if (!parent->parent->parent)
		type = PROGRESS_OPERATION;
	else
		throw Exception(UTIL_PROGRESS_ERROR, "Too many nested progress activity levels");

	//Allow progress reporting to be optional
	if (progress_function == NULL)
		return;

	return_options = PROGRESS_PROCEED;
	int rc = progress_function(PROGRESS_START, this, progress_context);
	ValidateReturnOption(rc);
}

//**********************************************************************************
ProgressReportableActivity::~ProgressReportableActivity()
{
	//Allow progress reporting to be optional
	if (progress_function == NULL)
		return;

	return_options = PROGRESS_PROCEED;
	progress_function(PROGRESS_END, this, progress_context);
}

//**********************************************************************************
int ProgressReportableActivity::ProgressReport(const int r)
{
	//Allow progress reporting to be optional
	if (progress_function == NULL)
		return PROGRESS_PROCEED;

	return_options = r;
	int rc = progress_function(PROGRESS_UPDATE, this, progress_context);
	ValidateReturnOption(rc);
	return rc;
}

//**********************************************************************************
void ProgressReportableActivity::ValidateReturnOption(const int rc)
{
	const char* badmsg = 
		"Invalid return code from custom-installed progress reporting function";

	//The return value must be one of those allowed, and only one bit is allowed too
	if (rc != PROGRESS_PROCEED && 
		rc != PROGRESS_CANCEL_ACTIVITY && 
		rc != PROGRESS_CANCEL_ACTIVITY_GROUP && 
		rc != PROGRESS_CANCEL_OPERATION && 
		rc != PROGRESS_ABORT_OPERATION) 
		throw Exception(UTIL_PROGRESS_ERROR, badmsg);

	if (! (rc & return_options))
		throw Exception(UTIL_PROGRESS_ERROR, badmsg);
}

} // close namespace
