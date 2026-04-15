//****************************************************************************************
// Base class for all objects using M204-style parameters.
// The main parm viewing/manipulating class is ViewerResetter.
//****************************************************************************************

#if !defined(BB_PARMIZED)
#define BB_PARMIZED

#include <map> 
#include <string> 
#include "lockable.h"

namespace dpt {

class ViewerResetter;
class ParmIniSettings;



//**********************************
class Parameterized {

	static ParmIniSettings* inisettings;

	friend class CoreServices;
	static void SetIni(ParmIniSettings* i) {inisettings = i;}

	friend class ViewerResetter;
	virtual std::string ResetParm(const std::string&, const std::string&) = 0;
	virtual std::string ViewParm(const std::string&, bool) const = 0;
	
protected:

	static std::string ResetWrongObject(const std::string& parm);
	static std::string ViewWrongObject(const std::string& parm);

	void RegisterParm(const std::string&, ViewerResetter*);

	const std::string& GetIniValueString(const std::string&, const std::string* = NULL);
	int GetIniValueInt(const std::string&, const int* = NULL);

public:
	virtual ~Parameterized() {};
};

}	//close namespace

#endif
