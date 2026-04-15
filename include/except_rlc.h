//****************************************************************************************
//A special subtype of exception with extra information for the $RLC functions.
//****************************************************************************************

#if !defined(BB_EXCEPT_RLC)
#define BB_EXCEPT_RLC

#include <string>
#include "except.h"

namespace dpt {

class RecordLock;

//****************************************************************************************
class Exception_RLC : public Exception {
	std::string		filename;
	int				recnum;
	std::string		userid;
	int				usernum;
	bool			during_find;

	//Used internally for wait management
	friend class DatabaseFileRLTManager;
	RecordLock*		blocker;

public:
	Exception_RLC(std::string, int, std::string, int, bool, RecordLock*);
	Exception_RLC(Exception_RLC* e) 
		: Exception(e->Code(), e->What()), 
		filename(e->filename), recnum(e->recnum), userid(e->userid), 
		usernum(e->usernum), during_find(e->during_find), blocker(e->blocker) {}

	const std::string&		FileName()		{return filename;}
	int						RecNum()		{return recnum;}
	const std::string&		UserID()		{return userid;}
	int						UserNum()		{return usernum;}

	bool					DuringFind()	{return during_find;}
};

}	//close namespace

#endif
