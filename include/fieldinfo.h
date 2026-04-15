
#if !defined(BB_FIELDINFO)
#define BB_FIELDINFO

#include <vector>
#include <string>
#include "dbcursor.h"
#include "garbage.h"
#include "fieldatts.h"

namespace dpt {

class DatabaseFileContext;
class SingleDatabaseFileContext;
class GroupDatabaseFileContext;
class FieldAttributePage;

typedef short FieldID;

//*********************************************************************************
struct PhysicalFieldInfo {
	std::string name;
	FieldAttributes atts;
	void* extra; //V2.14 Jan 09. Arbitrary cached info.

	//Physical file details
	FieldID id;
	int btree_root;

	PhysicalFieldInfo(const std::string& n, const FieldAttributes& a, FieldID i, int r) 
		: name(n), atts(a), extra(NULL), id(i), btree_root(r) {}
};

//*********************************************************************************
class FieldAttributeCursor : public DBCursor, public Destroyable {
	DatabaseFileContext* context;

	//Two types of cursor in dbcontext
	void RequestReposition(int param) {if (param == 1) needs_reposition = true;}

protected:
	//A copy of the PFI is made visible here after all, merely to support $LSTFLD
	//and $FDEF which want to show the field ID.  The group version of this cursor
	//has a dummy field ID if you use it (those $funcs only do single file contexts).
	std::vector<PhysicalFieldInfo> cached_field_table;
	int ix;
	virtual void CacheContextFieldAtts() = 0;

	FieldAttributeCursor(DatabaseFileContext*);
	virtual ~FieldAttributeCursor() {}

	friend class Destroyable;
	static void IndirectDestroy(Destroyable*);

public:
	bool Accessible();
	void GotoFirst();
	void GotoLast();
	void Advance(int n);

	int NumFields() {return cached_field_table.size();}

	const std::string* Name();
	const FieldAttributes* Atts();
	const FieldID* FID(); //see comment above
};

//*********************************************************************************
class FieldAttributeCursor_Single : public FieldAttributeCursor {
	SingleDatabaseFileContext* context;
	void CacheContextFieldAtts();

	friend class SingleDatabaseFileContext;
	FieldAttributeCursor_Single(SingleDatabaseFileContext*, bool gotofirst = true);
	~FieldAttributeCursor_Single() {}
};

//*********************************************************************************
class FieldAttributeCursor_Group : public FieldAttributeCursor {
	GroupDatabaseFileContext* context;
	void CacheContextFieldAtts();

	friend class GroupDatabaseFileContext;
	FieldAttributeCursor_Group(GroupDatabaseFileContext*, bool gotofirst = true);
	~FieldAttributeCursor_Group() {}
};

} //close namespace

#endif
