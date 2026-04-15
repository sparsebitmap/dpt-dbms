//*****************************************************************************************
//Virtual base class for the various cursor used in the API.
//*****************************************************************************************/

#if !defined(BB_API_CURSOR)
#define BB_API_CURSOR

#include "apiconst.h"

namespace dpt {

class DBCursor;

class APICursor {
public:
	DBCursor* target;
	APICursor(DBCursor* t) : target(t) {}
	APICursor(const APICursor& t) : target(t.target) {}
	//-----------------------------------------------------------------------------------

	void GotoFirst();
	void GotoLast();
	void Advance(int = 1);

	bool Accessible();
	bool CanEnterLoop() {return Accessible();} //more readable sometimes

	void SetOptions(CursorOptions);
};

} //close namespace

#endif
