
#if !defined(BB_RECORD)
#define BB_RECORD

#include <set>
#include <string>
#include "bbfloat.h"
#include "recdata.h"
#include "recread.h"
#include "fieldval.h"

namespace dpt {

class SingleDatabaseFileContext;
class DatabaseFile;
class RecordCopy;
class DatabaseFileDataManager;
class RecordDataAccessor;
struct PhysicalFieldInfo;
class FieldAttributes;
class FieldValue;

//***************************************************************************************
class Record : public ReadableRecord {
	DatabaseFileContext* source_set_context; //added for when group field atts mismatch
	SingleDatabaseFileContext* home_context;

	friend class DatabaseFileDataManager;
	friend class DatabaseFileIndexManager;
	RecordDataAccessor data_access;

	mutable bool ebm_check_required;
	void CheckEBM() const;

	friend class RecordDataAccessor;
	void ThrowNonexistent() const;

	friend class DatabaseFile; //STORE
	int DoubleAtom_AddField(PhysicalFieldInfo*, const FieldValue&, const FieldValue&, 
							bool = false, bool = false);

	void ValidateAndConvertTypes(PhysicalFieldInfo*, const FieldValue&, 
		FieldValue*, const FieldValue**, FieldValue*, const FieldValue**) const;
	static void ConvertPreChangeDataType(FieldValue&, PhysicalFieldInfo*);

	//Change and delete can be called by occurrence or value.  This is the shared code.
	int ChangeField_S(bool, const std::string&, const FieldValue&, 
						const int, const FieldValue*, FieldValue*);
	int DeleteField_S(bool, const std::string&, 
						const int, const FieldValue*, FieldValue*);

	friend class BitMappedRecordSet;
	//bool GetNextFVPair_ID(short&, FieldValue&, int& fvpix) const; //V3
	bool GetNextFVPair_ID(short*, FieldValue*, FieldValue*, int& fvpix) const;

public:
	Record(SingleDatabaseFileContext*, int, bool, bool, DatabaseFileContext* = NULL);

	SingleDatabaseFileContext* HomeFileContext() const {return home_context;}
	
	//Field reading - see base class
	bool GetFieldValue(const std::string& fname, FieldValue&, int occ = 1) const;
	int CountOccurrences(const std::string&) const;
	void CopyAllInformation(RecordCopy&) const;
	bool GetNextFVPair(std::string&, FieldValue&, int& fvpix) const;

	//Field functions return information:  In all cases the return value indicates an
	//occurrence number, with specific meaning differing as follows.  In some cases you
	//can also get back the pre-change value.
	
	//Add, insert: retval means the occurrence added/inserted.
	int AddField(const std::string& fname, const FieldValue& add_value);
	int InsertField(const std::string& fname, const FieldValue& newval, const int newocc = 1);

	//Change and delete by occurrence: retval is the old occurrence that was affected.  So
	//if occ existed, same as input, otherwise -1.  -1 means either delete did nothing, change 
	//worked like add, or field is invisible (index work may or may not have been done).
	//Note that with UAE fields, the replaced occ may not be the same as that changed.
	int ChangeField(const std::string& fname, const FieldValue& newval, 
									const int occ = 1, FieldValue* outoldval = NULL) {
		return ChangeField_S(false, fname, newval, (occ >= 1)?occ:INT_MAX, NULL, outoldval);}
	int DeleteField(const std::string& fname, const int occ = 1, FieldValue* ov = NULL) {
						return DeleteField_S(false, fname, occ, NULL, ov);}

	//Change and delete by value: retval is the old occurrence that got affected, as above.
	int ChangeFieldByValue(const std::string& fname, const FieldValue& newval, 
									const FieldValue& oldval) {
				return ChangeField_S(true, fname, newval, -1, &oldval, NULL);}

	int DeleteFieldByValue(const std::string& fname, const FieldValue& oldval) {
						return DeleteField_S(true, fname, -1, &oldval, NULL);}

	//Retval is the number of occs deleted
	int DeleteEachOccurrence(const std::string&);

	//Delete the record
	void Delete();

	//Potentially useful diagnostics - accessible in UL via the *RECINFO print item
	std::string ShowPhysicalInformation();

	//V3.0. BLOBs.  None of the above have any special BLOB processing (treated just like STRING).
	//These versions allow retrieval of the value and/or the descriptor or neither.
	//If just the descriptor is requested, *pdesc gets the value if it's a non-BLOB field.
	bool GetNextFVPairAndOrBLOBDescriptor(std::string&, FieldValue* pval, FieldValue* pdesc, int& fvpix) const;
	//This one returns the normal value if it's a non-BLOB field.
	bool GetBLOBDescriptor(const std::string& fname, FieldValue&, int occ = 1) const;
};

} //close namespace

#endif
