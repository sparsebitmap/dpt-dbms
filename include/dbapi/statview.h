//****************************************************************************************
//An interface for viewing statistics, much like the parameter viewer.
//****************************************************************************************

#if !defined(BB_API_STATVIEW)
#define BB_API_STATVIEW

#include <string>

#include "const_stat.h"
#include "dbctxt.h"

namespace dpt {

class StatViewer;

class APIStatViewer {
public:
	StatViewer* target;
	APIStatViewer(StatViewer* t) : target(t) {}
	APIStatViewer(const APIStatViewer& t) : target(t.target) {}
	//-----------------------------------------------------------------------------------	

	//A single stat.  
	unsigned int View(const std::string& s, const StatLevel l,
		const APIDatabaseFileContext& file = APIDatabaseFileContext(NULL),
	//NB.  Stats are 64 bit.  Optional parm 4 is populated with hi word.
	//This means user code compiler doesn't need 64 bit integer support.
		int* hiword = NULL);
//	_int64 View(const std::string& s, const StatLevel l,
//		APIDatabaseFileContext file = APIDatabaseFileContext()) const;

	//All nonzero stats
	std::string UnformattedLine(const StatLevel l, 
		const APIDatabaseFileContext& file = APIDatabaseFileContext(NULL));
	
	//A nice formatted line
	const std::string& TRequestCache();

	//Since-last grouping
	void StartActivity(const std::string& name);
	void EndActivity();
	const std::string& CurrentActivity();
};

} //close namespace

#endif
