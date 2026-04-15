//*****************************************************************************************
//Generic database file functionality.  The context may or may not actually be a group.
//*****************************************************************************************/

#if !defined(BB_API_DBCTXT)
#define BB_API_DBCTXT

#include <string>
#include <vector>

#include "apiconst.h"
#include "fieldinfo.h"
#include "foundset.h"
#include "valset.h"
#include "valdirect.h"
#include "findspec.h"
#include "reclist.h"
#include "reccopy.h"
#include "sortset.h"

namespace dpt {

class DatabaseFileContext;
struct LoadFormatOptions;
struct UnloadFormatOptions;
namespace util {
	class LineOutput;
}

class APIDatabaseFileContext {
public:
	DatabaseFileContext* target;
	APIDatabaseFileContext(DatabaseFileContext* t) : target(t) {}
	APIDatabaseFileContext(const APIDatabaseFileContext& t) : target(t.target) {}
	//-----------------------------------------------------------------------------------

	std::string GetShortName() const;
	std::string GetFullName() const;
	std::string GetFullFilePath() const;
	bool IsGroupContext() const;

	//DBA stuff
	void Initialize(bool leave_fields = false);
	void Increase(int, bool tabled);
	void ShowTableExtents(std::vector<int>*);
	unsigned int ApplyDeferredUpdates(int = ZORDER_STRICT, int* hibyte = NULL);
	void Unload(const FastUnloadOptions& = FUNLOAD_DEFAULT, const BitMappedRecordSet* = NULL, 
				std::vector<std::string>* fnames = NULL, const std::string& dir = FUNLOAD_DIR);
	void Load(const FastLoadOptions& = FLOAD_DEFAULT, int eyeball = 0, 
				util::LineOutput* eyeball_altdest = NULL, const std::string& dir = FLOAD_DIR);

	//Field definitions
	void DefineField(const std::string&, const APIFieldAttributes&);
	void RedefineField(const std::string&, const APIFieldAttributes&);
	void RenameField(const std::string& from, const std::string& to);
	void DeleteField(const std::string&);

	//Field info
	APIFieldAttributes GetFieldAtts(const std::string&);
	APIFieldAttributeCursor OpenFieldAttCursor();
	void CloseFieldAttCursor(const APIFieldAttributeCursor&);

	//Record and record set processing
	int StoreRecord(const APIStoreRecordTemplate& = APIStoreRecordTemplate());

	//V2.13 Dec 08. Reformulate these without default parameters
	APIFoundSet FindRecords();
	APIFoundSet FindRecords(const APIFindSpecification&);
	APIFoundSet FindRecords(const APIFindSpecification&, FindEnqueueType);
	APIFoundSet FindRecords(const APIFindSpecification&, const APIBitMappedRecordSet&); //referback
	APIFoundSet FindRecords(const APIFindSpecification&, FindEnqueueType, const APIBitMappedRecordSet&);
	APIFoundSet FindRecords(const std::string& fname, FindOperator, const APIFieldValue& fval); 

	APIRecordList CreateRecordList();

	void DestroyRecordSet(const APIRecordSet&);
	void DestroyAllRecordSets();

	//Value and value set processing
	APIValueSet FindValues(const APIFindValuesSpecification&);
	unsigned int CountValues(const APIFindValuesSpecification&);
	APIValueSet CreateEmptyValueSet();
	void DestroyValueSet(const APIValueSet&);
	void DestroyAllValueSets();
	void FileRecordsUnder(const APIBitMappedRecordSet&, const std::string&, const APIFieldValue&);

	//Value loops directly on the b-tree (single file only - use value sets on groups)
	APIDirectValueCursor OpenDirectValueCursor(const APIFindValuesSpecification&, 
								CursorDirection = CURSOR_ASCENDING);
	void CloseDirectValueCursor(const APIDirectValueCursor&);

	//Misc
	void DirtyDeleteRecords(const APIBitMappedRecordSet&);
};

} //close namespace

#endif
