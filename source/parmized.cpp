
#include "stdafx.h"

#include "parmized.h"
#include "parmvr.h"
#include "parmini.h"

#include "dataconv.h"
#include "msg_core.h"
#include "except.h"
#include "assert.h"

namespace dpt {

//Define static objects
ParmIniSettings*	Parameterized::inisettings = NULL;

//*****************************************
void Parameterized::RegisterParm(const std::string& parm, ViewerResetter* vr)
{
	vr->Register(parm, this);
}

//*****************************************
std::string Parameterized::ResetWrongObject(const std::string& parm)
{
	throw Exception(BUG_MISC, std::string
		("Bug: RESET passed to wrong object for parm: ").append(parm));
}

//*****************************************
std::string Parameterized::ViewWrongObject(const std::string& parm)
{
	throw Exception(BUG_MISC, std::string
		("Bug: VIEW passed to wrong object for parm: ").append(parm));
}

//*****************************************
//const std::string& Parameterized::GetIniValueString(const std::string& parm) //V3.0
const std::string& Parameterized::GetIniValueString
(const std::string& parm, const std::string* override_default)
{
	assert(!inisettings->ParmIsNumeric(parm));

	return inisettings->GetParmValue(parm, override_default);
}

//*****************************************
//int Parameterized::GetIniValueInt(const std::string& parm) //V3.0
int Parameterized::GetIniValueInt
(const std::string& parm, const int* override_default)
{
	assert(inisettings->ParmIsNumeric(parm));

	std::string soverride_default;
	std::string* pso = NULL;

	if (override_default) {
		soverride_default = util::IntToString(*override_default);
		pso = &soverride_default;
	}

	return util::StringToInt(inisettings->GetParmValue(parm, pso));
}

} //close namespace

