
#include "stdafx.h"

#include "stlextra.h"

namespace dpt { 

//*********************************************************************************
//Interestingly the STL string doesn't have exactly this feature.  The closest
//is to locate a string starting at a certain point but it will scan all the
//way to the end of the string, which is wasteful in some cases.  
//*********************************************************************************
size_t FindSubstringInRange
(const std::string& s, const std::string& sub, size_t startcol, size_t numcols)
{
	//Should probably throw a standard STL exception.
	if (startcol < 0 || numcols < 1)
		return std::string::npos;

	int endcol = startcol + numcols - 1;

	for (int col = startcol;
			col <= endcol && s.length() - col >= sub.length();
			col++)
	{
		if (memcmp(s.c_str() + col, sub.c_str(), sub.length()) == 0)
			return col;
	}

	return std::string::npos;
}

}
