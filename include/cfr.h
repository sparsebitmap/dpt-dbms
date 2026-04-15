
#ifndef BB_CFR
#define BB_CFR

#include <string>
#include "resource.h"

namespace dpt {

class DatabaseFile;
class DatabaseServices;

//**********************************************************************************
class CriticalFileResource : public Resource {

	DatabaseFile* parent;

public:
	CriticalFileResource(DatabaseFile* f, const std::string& rn)
		: Resource(rn), parent(f) {}

	//Override to allow us to set WT=24/25
	void Get(DatabaseServices*, bool required_lock);

	bool UpgradeToExcl();
};

//**********************************************************************************
class CFRSentry {
	DatabaseServices* dbapi;
	CriticalFileResource* res;
	mutable bool enabled;  //ugly but saves recoding std::vector::insert

	void CopyFrom(const CFRSentry&);

public:
	CFRSentry() : enabled(false) {}
	CFRSentry(DatabaseServices*, CriticalFileResource*, bool);
	CFRSentry(DatabaseServices*, CriticalFileResource*, CriticalFileResource*);
	~CFRSentry() {Release();}

	CFRSentry(const CFRSentry& s) : enabled(false) {CopyFrom(s);}
	CFRSentry& operator=(const CFRSentry& s) {CopyFrom(s); return *this;}

	void Release();
	void Get(DatabaseServices*, CriticalFileResource*, bool);
};

} //close namespace

#endif
