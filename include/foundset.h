
#if !defined(BB_FOUNDSET)
#define BB_FOUNDSET

#include "bmset.h"

namespace dpt {

//***************************************************************************************
class FoundSet : public BitMappedRecordSet {

	bool is_unlocked;

	friend class DatabaseFileContext;
	friend class SingleDatabaseFileContext;
	friend class GroupDatabaseFileContext;
	FoundSet(DatabaseFileContext* c) : BitMappedRecordSet(c), is_unlocked(true) {}
	~FoundSet() {} //just here to ensure private

	friend class DatabaseFileDataManager;
	friend class DatabaseFileIndexManager;
	friend class FindOperation;
	void AppendFileSet(int, BitMappedFileRecordSet*);

	void DirtyDeleteAdhocLockExcl(DatabaseServices*);
	void NotifyOfDirtyDelete(BitMappedRecordSet*);

public:
	//No UL interface for this, but could be useful to API progs
	void Unlock();
};

} //close namespace

#endif
