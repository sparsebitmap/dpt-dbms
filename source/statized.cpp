
#include "stdafx.h"

#include "statized.h"

//API Tiers
#include "statview.h" 
//Diagnostics
#include "except.h" 
#include "msgcodes.h" 

namespace dpt {

//**************************************************************************************
void Statisticized::RegisterHolder(StatViewer* v)
{
	v->RegisterHolder(this);
}

//**************************************************************************************
void Statisticized::RegisterStat(const std::string& s, StatViewer* v)
{
	v->RegisterStat(s, this);
}

//**************************************************************************************
void Statisticized::StatWrongObject(const std::string& stat)
{
	throw Exception(BUG_MISC, std::string
		("Bug: VIEW passed to wrong object for stat: ").append(stat));
}

//**************************************************************************************
//**************************************************************************************
_int64 MultiStat::Value(StatLevel lev) const
{
	if (lev == STATLEVEL_SYSTEM_FINAL) 
		return sys.Value();

	if (lev == STATLEVEL_USER_LOGOUT) 
		return user.Value();

	return sl.Value();
}

} //close namespace

