//****************************************************************************************
//The result of database record search
//****************************************************************************************

#if !defined(BB_API_FOUNDSET)
#define BB_API_FOUNDSET

#include "bmset.h"

namespace dpt {

class FoundSet;

class APIFoundSet : public APIBitMappedRecordSet {
public:
	FoundSet* target;
	APIFoundSet(FoundSet*);
	APIFoundSet(const APIFoundSet&);
	//-----------------------------------------------------------------------------------

	//No User Language equivalent for this, but might be handy for API programs.
	void Unlock();
};

} //close namespace

#endif
