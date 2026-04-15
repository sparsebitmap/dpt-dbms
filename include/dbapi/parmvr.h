//****************************************************************************************
//An interface for viewing and resetting parameters.
//****************************************************************************************

#if !defined(BB_API_PARMVR)
#define BB_API_PARMVR

#include <string>

#include "dbctxt.h"

namespace dpt {

class ViewerResetter;

class APIViewerResetter {
public:
	ViewerResetter* target;
	APIViewerResetter(ViewerResetter* t) : target(t) {}
	APIViewerResetter(const APIViewerResetter& t) : target(t.target) {}
	//-----------------------------------------------------------------------------------

	//Returns the value actually set (NB. the max or min might be substituted)
	std::string Reset(const std::string& parmname, const std::string& newvalue, 
		const APIDatabaseFileContext& file = APIDatabaseFileContext(NULL));

	//Retrieve parm value from the appropriate object
	std::string View(const std::string& parmname, 
		const APIDatabaseFileContext& file = APIDatabaseFileContext(NULL),
		bool fancyformat = true) const;

	//Alternative
	int ViewAsInt(const std::string& p, 
		const APIDatabaseFileContext& file = APIDatabaseFileContext(NULL)) const;

	//See doc for valid categories - "TABLES", "ALL" etc.
	void GetParmsInCategory(std::vector<std::string>&, const std::string&) const;
};

} //close namespace

#endif
