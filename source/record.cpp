
#include "stdafx.h"

#include "record.h"

//Utils
#include "molecerr.h"
#include "dataconv.h"
//API Tiers
#include "reccopy.h"
#include "dbserv.h"
#include "dbctxt.h"
#include "dbf_field.h"
#include "dbf_data.h"
#include "dbf_index.h"
#include "dbf_ebm.h"
#include "dbfile.h"
#include "du1step.h"
#include "update.h"
#include "seqfile.h"
#include "msgroute.h"
#include "core.h"
//Diagnostics
#include "except.h"
#include "msg_db.h"

namespace dpt {

//***************************************************************************************
Record::Record
(SingleDatabaseFileContext* hc, int r, bool tbo, bool ddflag, DatabaseFileContext* sc) 
: ReadableRecord(r), source_set_context(sc), home_context(hc),
  data_access(r, tbo), ebm_check_required(true)
{
	data_access.DelayedConstruct(this);

	//See comments below. Per-record EBM checks are rarely required in fact
	if (!ddflag)
		ebm_check_required = false;
}


//***************************************************************************************
void Record::CheckEBM() const
{
	if (!ebm_check_required)
		return;

/* * * 

Original function comment:
--------------------------
This is called once on the first real access via the record.  The idea is to produce
the behaviour whereby we can create a record MRO on a non-existent record but it
won't trigger an error message unless we actually try and get any of the data.  We can
however request general info such as the record number and file.  This makes it much 
easier to compile User Language "FOR EACH RECORD" loops, since at the top of the
loop we don't know whether any actual record data will be retrieved.

Subsequent comments:
--------------------
This whole area has become more complex, because of attempts to efficienetly allow 
concurrent access to the same record on different threads, as discussed in the tech docs.
See also code comments e.g. at 

DatabaseFileDataManager::FlagUpdatedRecordMROs()
DatabaseFileDataManager::FlagDeletedRecordMROs()
AtomicBackout_DeInsertFieldData::Perform()

and others.

The end result for here is that this function usually does nothing because it's largely
unnecessary, and a significant overhead in the common case of doing a loop on a
set which should in any case be locked, and if it's not (e.g. a list) will often
have record locks present elsewhere in the request, and the user would not expect
lots of EBM work to have to be done.

The current scheme has one problem in that it means we might end up showing extension 
record data if we have an unlocked set, and another thread deletes the record 
and then the slot is reused as an extension, then we come to the record in our
loop and access it.  As stated in the tech docs though we consider it to be the
user's lookout if they get obscure problems when using FDWOL, and in any case this 
would be really obscure, as it would entail RRN, extension records and a FDWOL in an 
updating environment, none of which are common.

So existence checking is now done by a combination of things:

1. The data accessor throws NER if the table B page slot is empty.  That is, no record
	with the desired number physically exists on the table B page.
2. When locked sets are used by everybody there is no problem because Record MROs
	are only accessible via record sets, and deleting a record requires EXCL.
3. Excecpt if we already have the Record MRO via an unlocked set at the point somebody
	else deletes the record, then we try and use the MRO again.  In that case the data
	accessor throws from UnlockedRecordDeletionCheck()).
4. Dirty delete doesn't come via this class but operates at the set level, so that
	check doesn't work.  With dirty delete, when other threads have unlocked record 
	sets this function kicks in as per its original intention.  So when a Record MRO is
	created via a set which was already in existence when the dirty delete was performed
	it is the only time we force it to check the EBP.  And then it has to check it for
	every call!  (Well strictly speaking we could dispense with it after an update if
	LPU was turned on but let's keep it simple).
	See NotifyOfDirtyDelete() and related code.
5. V3.0 note.  See now comments in Delete() below.  The above-described scheme catered
    for subsequent access by the same user via the same record MRO, but if the opened
	another one, then updates on the second one could crash in the middle of the update
	molecule.  A small extra check is now performed.
* * */

	DatabaseFile* file = home_context->GetDBFile();
	DatabaseServices* dbapi = home_context->DBAPI();

	if (!file->GetEBMMgr()->DoesPrimaryRecordExist(dbapi, primary_extent_recnum))
		ThrowNonexistent();
}

//***************************************************************************************
void Record::ThrowNonexistent() const
{
	//See comment in function above
	throw Exception(DML_NONEXISTENT_RECORD, 
		std::string("Non-existent record referenced: Record # ")
		.append(util::IntToString(primary_extent_recnum))
		.append(" in file ").append(home_context->GetDBFile()->FileName(home_context)));
}

//***************************************************************************************
void Record::ValidateAndConvertTypes
(PhysicalFieldInfo* pfi, const FieldValue& inval, 
 FieldValue* converted_dataval, const FieldValue** use_dataval, 
 FieldValue* converted_ixval, const FieldValue** use_ixval) const
{
	bool dataval_was_converted = false;
	bool invalid_dataval_zero = false;

	//---------------------------------
	//1. Convert data value for table B
	if (pfi->atts.IsFloat() != inval.CurrentlyNumeric()) {

		DatabaseFileFieldManager::ConvertValue
			(home_context, pfi, inval, converted_dataval, use_dataval, 
			false, &dataval_was_converted, &invalid_dataval_zero);
	}

	//-----------------------------------------------------
	//V3.0.  Non-BLOB check.  This is here because of the fact that FieldValue objects
	//can now be created with > 255 bytes in them, which we usually don't want to allow through.
	if ((*use_dataval)->CurrentlyString() && !pfi->atts.IsBLOB())
		(*use_dataval)->CheckStrLen255();

	//----------------------------------
	//2. Convert index value for table D
	bool ixval_was_converted = false;
	bool invalid_ixval_zero = false;

	if (pfi->atts.IsOrdered() && pfi->atts.IsOrdNum() != inval.CurrentlyNumeric()) {

		DatabaseFileFieldManager::ConvertValue
			(home_context, pfi, inval, converted_ixval, use_ixval, 
			true, &ixval_was_converted, &invalid_ixval_zero);
	}

	//-----------------------------------------------------
	//Consistency checks.  These are here to cater for the situation where the user
	//defines a field as STRING ORD NUM or FLOAT ORD CHAR, and supplies invalid
	//or non standard numeric values when FMODLDPT is not set appropriately to allow them.
	std::string msg;
	if (pfi->atts.IsOrdChar()) {
		if (invalid_dataval_zero) {
			*use_ixval = converted_ixval;
			converted_ixval->AssignData("0", 1);
			msg = "Warning: ORD CHAR index component also converted to '0' to match";
		}

		//Also if the user gives a string value in non-standard numeric format (e.g. '1E2') 
		//the string stored in the btree should match the standard format for the float 
		//value stored in table B (in this case '100').  Otherwise CHANGE and DELETE won't
		//work correctly with visibles - see later.  (V2.06 Also now see STRING ORD NUM below).
		else if (dataval_was_converted && !ixval_was_converted) {

			//Save doing the first half of the double conversion again
			*converted_ixval = *converted_dataval;
			converted_ixval->ConvertToString();

			if (converted_ixval->Compare(inval) != 0) {
				*use_ixval = converted_ixval;

				//Complex message!  Oh well it might discourage this att combo!
				msg = "Warning: ORD CHAR index component standardized to match FLOAT data value ('";
				msg.append(inval.ExtractString());
				msg.append("' -> '");
				msg.append(converted_ixval->ExtractString());
				msg.append("')");
			}
		}
	}

	//STRING ORD NUM
	else if (pfi->atts.IsString()) {
		if (invalid_ixval_zero) {
			*use_dataval = converted_dataval;
			converted_dataval->AssignData("0", 1);
			msg = "Warning: STRING data component also converted to '0' to match";
		}

		//See comment above.
		//V2.06.  Jul 07.  Now however if the user sets a new FMODLDPT flag we allow
		//nonstandard numeric literals in the STRING part in table B.  So long as we
		//remember to convert during change and delete (qv) there is not a problem with
		//locating the btree entry using the nonstandard value.  This issue was reported
		//by Mick Sheehy whose site commonly stores leading zeroes in STRING ORD NUM fields.
		else if (ixval_was_converted && !dataval_was_converted) {

			//Leave valid nonstandard numerics alone if requested (e.g. +007)
			if (! (home_context->DBAPI()->GetParmFMODLDPT() & 2)) {

				//Save doing the first half of the double conversion again
				*converted_dataval = *converted_ixval;
				converted_dataval->ConvertToString();

				if (converted_dataval->Compare(inval) != 0) {
					*use_dataval = converted_dataval;

					//Expensive message construction!  Oh well it might discourage this att combo!
					msg = "Warning: STRING data component standardized to match ORD NUM index value ('";
					msg.append(inval.ExtractString());
					msg.append("' -> '");
					msg.append(converted_dataval->ExtractString());
					msg.append("'), because FMODLDPT x02 bit is not set");
				}
			}
		}
	}

	if (msg.length() > 0)
		home_context->DBAPI()->Core()->GetRouter()->Issue(DML_NON_FLOAT_INFO, msg);
}

//***************************************************************************************
//Used when applying index updates in change and delete - see comments in ChangeField_S
//***************************************************************************************
void Record::ConvertPreChangeDataType(FieldValue& old_value, PhysicalFieldInfo* pfi)
{
	if (pfi->atts.IsOrdNum()) {
		if (!old_value.CurrentlyNumeric())
			old_value.ConvertToNumeric();
	}
	else {
		if (old_value.CurrentlyNumeric())
			old_value.ConvertToString();
	}
}

//***************************************************************************************
//Used both by the public AddField() and DatabaseFile::StoreRecord()
//***************************************************************************************
int Record::DoubleAtom_AddField
(PhysicalFieldInfo* pfi, const FieldValue& dataval, 
 const FieldValue& ixval, bool storing, bool du_mode)
{
	bool ix_reqd = false;
	int occ_added = -1;

	//Add field is valid for invisibles - just skip the data part here
	if (pfi->atts.IsInvisible())
		ix_reqd = true;
	
	else {
		//No need to do the pre-emptive dupe scan if the field's not even indexed
		bool* pix = (pfi->atts.IsOrdered()) ? &ix_reqd : NULL;

		//V2.03 - a couple of special flags to skip things during store and/or load
		//AtomicUpdate_InsertFieldData au(this, pfi, dataval, INT_MAX, pix);
		//AtomicUpdate_InsertFieldData au(this, pfi, dataval, INT_MAX, pix, storing, du_mode); //V3.0 BLOBs
		//V3.03. I am at a loss to why I would have made the 3.0 change. Testing maybe? 
		//Reversing now anyway as it breaks STR ORD NUM and FLOAT ORD CHAR cases.
		//AtomicUpdate_InsertFieldData au(this, pfi, ixval, INT_MAX, pix, storing, du_mode);
		AtomicUpdate_InsertFieldData au(this, pfi, dataval, INT_MAX, pix, storing, du_mode); 
		occ_added = au.Perform();
	}

	//This gets skipped if a duplicate f=v was just added to the record
	if (ix_reqd) {
		DatabaseFile* f = home_context->GetDBFile();
		DatabaseServices* dbapi = home_context->DBAPI();

		//Deferred index updates?
		if (f->IsInDeferredUpdateMode()) {

			//V2.14 Jan 2009.  Deferred updates may be collated in memory now.
			if (f->IsInOneStepDUMode())
				f->WriteOneStepDURecord(home_context, primary_extent_recnum, pfi, ixval);

			//Original style deferred updates to sequential files
			else
				f->WriteDeferredUpdateRecord(dbapi, primary_extent_recnum, pfi->id, ixval);
		}

		//Not deferred, so update the indexes right now
		else {
			AtomicUpdate_AddIndexValRec au(this, pfi, ixval, storing);
			au.Perform();
		}
	}

	return occ_added;
}
	

	
	
	
	


//***************************************************************************************
//***************************************************************************************
//***************************************************************************************
//Public reading functions.
//***************************************************************************************
//***************************************************************************************
//***************************************************************************************
int Record::CountOccurrences(const std::string& fname) const
{
	DatabaseFile* file = home_context->GetDBFile();

	CheckEBM();

	FieldAttributes grpatts;
	PhysicalFieldInfo* pfi = file->GetFieldMgr()->GetAndValidatePFI
		(home_context, fname, false, false, false, &grpatts, source_set_context);

	if (pfi)
		return file->GetDataMgr()->CountOccurrences(this, pfi);
	else
		//Group with missing field in the current file.
		return 0;
}

//***************************************************************************************
void Record::CopyAllInformation(RecordCopy& result) const
{
	DatabaseFile* file = home_context->GetDBFile();

	CheckEBM();

	file->GetDataMgr()->CopyAllInformation(this, result);
}

//***************************************************************************************
//bool Record::GetNextFVPair_ID(FieldID& fid, FieldValue& fval, int& fvpix) const
bool Record::GetNextFVPair_ID(FieldID* fid, FieldValue* fval, FieldValue* blobdesc, int& fvpix) const
{
	DatabaseFile* file = home_context->GetDBFile();

	CheckEBM();

//	return file->GetDataMgr()->GetNextFVPair(this, fid, fval, fvpix); //V3
	return file->GetDataMgr()->GetNextFVPair(this, fid, fval, blobdesc, fvpix);
}

//***************************************************************************************
bool Record::GetNextFVPair(std::string& fname, FieldValue& fval, int& fvpix) const
{
	//Same as the ID version above...
	FieldID fid;
	if (GetNextFVPair_ID(&fid, &fval, NULL, fvpix)) {

		//But we then look up the field name for the caller's convenience.
		PhysicalFieldInfo* pfi = home_context->GetDBFile()->GetFieldMgr()->
			GetPhysicalFieldInfo(home_context, fid);

		fname = pfi->name;
		return true;
	}
	else {
		fname = std::string();
		return false;
	}
}

//***************************************************************************************
//V3.0.  Mainly for the PAI statement with its various BLOB display options
bool Record::GetNextFVPairAndOrBLOBDescriptor
(std::string& fname, FieldValue* fval, FieldValue* blobdesc, int& fvpix) const
{
	//Much the same as above, just allowing the null parameters in
	FieldID fid;
	if (GetNextFVPair_ID(&fid, fval, blobdesc, fvpix)) {

		PhysicalFieldInfo* pfi = home_context->GetDBFile()->GetFieldMgr()->
			GetPhysicalFieldInfo(home_context, fid);

		fname = pfi->name;
		return true;
	}
	else {
		fname = std::string();
		return false;
	}
}

//***************************************************************************************
bool Record::GetFieldValue(const std::string& fname, FieldValue& fval, int occ) const
{
	DatabaseFile* file = home_context->GetDBFile();

	CheckEBM();

	FieldAttributes grpatts;
	PhysicalFieldInfo* pfi = file->GetFieldMgr()->GetAndValidatePFI
			(home_context, fname, false, false, false, &grpatts, source_set_context);

	if (pfi)
		return file->GetDataMgr()->ReadFieldValue(this, pfi, occ, fval);

	else {
		//Group with missing field in the current file but field exists in other member(s).
		//Return null value appropriate to the field in the other member(s).
		//NB. The UL evaluator will be using null string even for float fields, but we
		//provide the 0.0 for API progs that might want to know (also comments in recdata.cpp).
		if (grpatts.IsFloat())
			fval = 0.0;
		else
			fval.AssignData("", 0);;

		return false;
	}
}
	
//***************************************************************************************
FieldValue ReadableRecord::GetFieldValue(const std::string& fname, int occ) const
{
	FieldValue temp;
	GetFieldValue(fname, temp, occ);
	return temp;
}







//***************************************************************************************
//***************************************************************************************
//***************************************************************************************
//Public updating functions.
//***************************************************************************************
//***************************************************************************************
//***************************************************************************************
int Record::AddField(const std::string& fname, const FieldValue& inval)
{
	//This check primarily for file full
	home_context->GetDBFile()->CheckFileStatus(true, true, true, false);

	CheckEBM();

	DatabaseFile* file = home_context->GetDBFile();
	DatabaseServices* dbapi = home_context->DBAPI();

	//Some preliminaries - can fail before we start the update proper
	PhysicalFieldInfo* pfi = 
		file->GetFieldMgr()->GetAndValidatePFI(home_context, fname, true, true, false);

	//Validate/convert for storage in data and indexes
	const FieldValue* use_dataval = &inval;
	const FieldValue* use_ixval = &inval;
	FieldValue datatemp;
	FieldValue ixtemp;
	ValidateAndConvertTypes(pfi, inval, &datatemp, &use_dataval, &ixtemp, &use_ixval);

	//Commit any open non-backoutable updates such as DEFINE FIELD
	file->OperationDelimitingCommit(dbapi, false, true);

	UpdateUnit* uu = dbapi->GetUU();
	uu->PlaceRecordUpdatingLock(file, primary_extent_recnum);

	//V3.0. See Delete()
	data_access.PreUpdatePeek();

	try {
		//Call code shared with STORE
		int occ_added = DoubleAtom_AddField(pfi, *use_dataval, *use_ixval);

		uu->EndOfMolecule();

		return occ_added;
	}
	MOLECULE_CATCH(file, uu);
}

//***************************************************************************************
int Record::InsertField(const std::string& fname, const FieldValue& inval, const int inocc)
{
	DatabaseFile* file = home_context->GetDBFile();
	DatabaseServices* dbapi = home_context->DBAPI();

	//Negative subscript means do nothing, as on M204
	if (inocc < 0)
		return -1;
	
	//Zero subscript means the same as 1, as on M204
	int occ = inocc;
	if (occ == 0)
		occ = 1;

	//This check primarily for file full
	home_context->GetDBFile()->CheckFileStatus(true, true, true, true);

	CheckEBM();

	//Some preliminaries - can fail before we start the update proper
	PhysicalFieldInfo* pfi = 
		file->GetFieldMgr()->GetAndValidatePFI(home_context, fname, true, false, false);

	//Validate/convert for storage in data and indexes
	const FieldValue* use_dataval = &inval;
	const FieldValue* use_ixval = &inval;
	FieldValue datatemp;
	FieldValue ixtemp;
	ValidateAndConvertTypes(pfi, inval, &datatemp, &use_dataval, &ixtemp, &use_ixval);

	//Commit any open non-backoutable updates such as DEFINE FIELD
	file->OperationDelimitingCommit(dbapi, false, true);

	UpdateUnit* uu = dbapi->GetUU();
	uu->PlaceRecordUpdatingLock(file, primary_extent_recnum);

	//V3.0. See Delete()
	data_access.PreUpdatePeek();

	try {
		bool ix_reqd = false;
		int occ_inserted = -1;

		bool* pix = (pfi->atts.IsOrdered()) ? &ix_reqd : NULL;

		//NB insert is not valid on invisibles, so no test for that here
		AtomicUpdate_InsertFieldData au(this, pfi, *use_dataval, occ, pix);
		occ_inserted = au.Perform();

		//We could skip this if a duplicate f=v was just added to the record
		if (ix_reqd) {
			AtomicUpdate_AddIndexValRec au(this, pfi, *use_ixval);
			au.Perform();
		}

		uu->EndOfMolecule();

		return occ_inserted;
	}
	MOLECULE_CATCH(file, uu);
}

//***************************************************************************************
//Shared function with an amalgamation of the parameters from the 2 public versions
//***************************************************************************************
int Record::ChangeField_S
(bool by_value, const std::string& fname, const FieldValue& inval, 
 const int occ, const FieldValue* pinoldval, FieldValue* poutoldval)
{
	DatabaseFile* file = home_context->GetDBFile();
	DatabaseServices* dbapi = home_context->DBAPI();

	//This check primarily for file full
	home_context->GetDBFile()->CheckFileStatus(true, true, true, true);

	CheckEBM();

	//Some preliminaries - can fail before we start the update proper
	PhysicalFieldInfo* pfi = 
		file->GetFieldMgr()->GetAndValidatePFI(home_context, fname, true, by_value, false);

	//Validate/convert for storage in data and indexes
	const FieldValue* use_dataval = &inval;
	const FieldValue* use_ixval = &inval;
	FieldValue datatemp;
	FieldValue ixtemp;
	ValidateAndConvertTypes(pfi, inval, &datatemp, &use_dataval, &ixtemp, &use_ixval);

	//When changing by value the user may supply the old value as the "wrong" type,
	//in which case convert.  In this case treat invalid numeric strings as zero.
//* * * To check on M204.
	FieldValue old_value;
	if (by_value) {
		old_value = *pinoldval;
		if (pfi->atts.IsFloat() != pinoldval->CurrentlyNumeric()) {
			if (pfi->atts.IsFloat())
				old_value.ConvertToNumeric();
			else
				old_value.ConvertToString();
		}
	}

	//Commit any open non-backoutable updates such as DEFINE FIELD
	file->OperationDelimitingCommit(dbapi, false, true);

	UpdateUnit* uu = dbapi->GetUU();
	uu->PlaceRecordUpdatingLock(file, primary_extent_recnum);

	//V3.0. See Delete()
	data_access.PreUpdatePeek();

	try {
		bool ix_reqd_addval = false;
		bool ix_reqd_delval = false;
		int occ_changed = -1;

		//Changing invisible fields needs no data work
		if (pfi->atts.IsInvisible()) {
			if (old_value == *use_ixval) {
				ix_reqd_addval = false;
				ix_reqd_delval = false;
			}
			else {
				ix_reqd_addval = true;
				ix_reqd_delval = true;
			}
		}

		else {

			bool* pixa = (pfi->atts.IsOrdered()) ? &ix_reqd_addval : NULL;
			bool* pixd = (pfi->atts.IsOrdered()) ? &ix_reqd_delval : NULL;

			if (by_value) {
				AtomicUpdate_ChangeFieldData au
					(this, pfi, *use_dataval, &old_value, pixa, pixd);
				occ_changed = au.Perform(&old_value);
			}
			else {
				AtomicUpdate_ChangeFieldData au
					(this, pfi, *use_dataval, occ, pixa, pixd);
				occ_changed = au.Perform(&old_value);
			}
		}

		//Purely for the user - TBO will be done by occurrence if required.  Note that
		//if the data is string and the index is numeric or vice versa, the old value
		//returned has the type of the data component.  See also index comment below.
		if (poutoldval)
			*poutoldval = old_value;

		//Delete the old index entry first to give least chance of space probs.
		if (ix_reqd_delval) {

			//Convert old value first if required as per above comment.
			//V2.06 Jul 07.  
			ConvertPreChangeDataType(old_value, pfi);

			AtomicUpdate_RemoveIndexValRec au(this, pfi, old_value);

			//See comment in DeleteField below.
			if (!au.Perform())
				if (pfi->atts.IsVisible())
					throw Exception(DB_ALGORITHM_BUG, 
						"Bug: index value corresponding to pre-change FVP is missing");
		}

		//Then insert the entry for the new value
		if (ix_reqd_addval) {
			AtomicUpdate_AddIndexValRec au(this, pfi, *use_ixval);
			au.Perform();
		}

		uu->EndOfMolecule();

		return occ_changed;
	}
	MOLECULE_CATCH(file, uu);
}

//***************************************************************************************
int Record::DeleteField_S
(bool by_value, const std::string& fname, 
 const int occ, const FieldValue* pinoldval, FieldValue* poutoldval)
{
	DatabaseFile* file = home_context->GetDBFile();
	DatabaseServices* dbapi = home_context->DBAPI();

	home_context->GetDBFile()->CheckFileStatus(false, true, true, true);

	CheckEBM();

	//Some preliminaries - can fail before we start the update proper
	PhysicalFieldInfo* pfi = 
		file->GetFieldMgr()->GetAndValidatePFI(home_context, fname, true, by_value, false);

	//See comments above in ChangeField_S
	FieldValue old_value;
	if (by_value) {
		old_value = *pinoldval;
		if (pfi->atts.IsFloat() != pinoldval->CurrentlyNumeric()) {
			if (pfi->atts.IsFloat())
				old_value.ConvertToNumeric();
			else
				old_value.ConvertToString();
		}
	}

	//Commit any open non-backoutable updates such as DEFINE FIELD
	file->OperationDelimitingCommit(dbapi, false, true);

	UpdateUnit* uu = dbapi->GetUU();
	uu->PlaceRecordUpdatingLock(file, primary_extent_recnum);

	//V3.0. See Delete()
	data_access.PreUpdatePeek();

	try {
		bool ix_reqd = false;
		int occ_deleted = -1;

		if (pfi->atts.IsInvisible())
			ix_reqd = true;

		else {
			bool* pix = (pfi->atts.IsOrdered()) ? &ix_reqd : NULL;

			if (by_value) {
				AtomicUpdate_DeleteFieldData au(this, pfi, &old_value, pix);
				occ_deleted = au.Perform(&old_value);
			}
			else {
				AtomicUpdate_DeleteFieldData au(this, pfi, occ, pix);
				occ_deleted = au.Perform(&old_value);
			}
		}

		//Purely for the user (see also comment in ChangeField_S)
		if (poutoldval)
			*poutoldval = old_value;

		//Remove entry from index if appropriate.
		if (ix_reqd) {

			//V2.06 Jul 07.  
			ConvertPreChangeDataType(old_value, pfi);

			AtomicUpdate_RemoveIndexValRec au(this, pfi, old_value);

			//Note that the table B phase should have ensured that we only delete values
			//for which there is no longer a matching FVP on the record, so we should be
			//able to rely on this working.  This is another reason for insisting on 
			//standardized numeric literals with FLOAT ORD CHAR - see earlier.
			if (!au.Perform())
				if (pfi->atts.IsVisible())
					throw Exception(DB_ALGORITHM_BUG, 
						"Bug: index value corresponding to deleted FVP is missing");
		}

		//no need for a doubleatom func as only used here
		uu->EndOfMolecule();

		return occ_deleted;
	}
	MOLECULE_CATCH(file, uu);
}

//***************************************************************************************
int Record::DeleteEachOccurrence(const std::string& fname)
{
	DatabaseFile* file = home_context->GetDBFile();
	DatabaseServices* dbapi = home_context->DBAPI();

	home_context->GetDBFile()->CheckFileStatus(false, true, true, true);

	CheckEBM();

	//Some preliminaries - can fail before we start the update proper
	PhysicalFieldInfo* pfi = 
		file->GetFieldMgr()->GetAndValidatePFI(home_context, fname, true, false, false);

	//Commit any open non-backoutable updates such as DEFINE FIELD
	file->OperationDelimitingCommit(dbapi, false, true);

	UpdateUnit* uu = dbapi->GetUU();
	uu->PlaceRecordUpdatingLock(file, primary_extent_recnum);

	//V3.0. See Delete()
	data_access.PreUpdatePeek();

	try {
		int occs_deleted = 0;

		//Repeatedly delete the first occurrence, in a very similar way to a single
		//field delete by occurrence.  Note that it would not be safe to take a 
		//record copy here, since records too big for memory are perfectly possible.
		//Note also that using insert during TBO will reinstate the occurrences in
		//the correct order relative to each other, but not necessarily relative to
		//other fields on the record.  Same comment for delete record below.
		for (;;) {
			bool ix_reqd = false;
			FieldValue old_value;

			bool* pix = (pfi->atts.IsOrdered()) ? &ix_reqd : NULL;

			AtomicUpdate_DeleteFieldData au(this, pfi, 1, pix);
			int occ_deleted = au.Perform(&old_value);

			if (occ_deleted <= 0)
				break;

			occs_deleted++;

			//Remove entry from index if appropriate.
			if (ix_reqd) {
				ConvertPreChangeDataType(old_value, pfi);
				AtomicUpdate_RemoveIndexValRec au(this, pfi, old_value);
				au.Perform();
			}
		}

		uu->EndOfMolecule();

		return occs_deleted;
	}
	MOLECULE_CATCH(file, uu);
}

//***************************************************************************************
void Record::Delete()
{
	DatabaseFile* file = home_context->GetDBFile();
	DatabaseServices* dbapi = home_context->DBAPI();

	home_context->GetDBFile()->CheckFileStatus(false, true, true, true);

	CheckEBM();

	//Commit any open non-backoutable updates such as DEFINE FIELD
	file->OperationDelimitingCommit(dbapi, false, true);

	//Try this first outside the molecule - it can fail without us starting an update
	UpdateUnit* uu = dbapi->GetUU();
	uu->PlaceRecordUpdatingLock(file, primary_extent_recnum);

	//V3.0.  Two issues here.  In both cases trying to avoid EBP access for record updates,
	//since that is already the designed intention (just slightly glitched!).  First issue
	//is common to all record updates, namely the NER exception thrown out out the table B
	//page accessor in the middle of a molecular update breaks the file.  Second issue is 
	//much the same but here in delete record the EBP work happens first so the NER exception
	//is thrown out of there.  In both cases a little pre-molecule peek does the trick,
	//and has no extra IO cost since we're already going to table B anyway.
	data_access.PreUpdatePeek();

	try {
		//The sequence is largely the reverse of a store
		AtomicUpdate_DeExistizeDeletedRecord aue(home_context, primary_extent_recnum);
		aue.Perform();

		//Very similar to DeleteEachOccurrence - see above for comments
		for (int fnum = 0;;fnum++) {

			//The difference is that here we request the first occurrence of any field
			//rather than the first occurrence of a named field.
			FieldValue old_value;
			FieldID fid;

			//No more fields
			int dummy = 0;
			//if (!file->GetDataMgr()->GetNextFVPair(this, fid, old_value, dummy)) //V3.0
			if (!file->GetDataMgr()->GetNextFVPair(this, &fid, &old_value, NULL, dummy))
				break;

			PhysicalFieldInfo* pfi = 
				home_context->GetDBFile()->GetFieldMgr()->
					GetPhysicalFieldInfo(home_context, fid);

			bool ix_reqd = false;
			bool* pix = (pfi->atts.IsOrdered()) ? &ix_reqd : NULL;

			AtomicUpdate_DeleteFieldData au(this, pfi, 1, pix);
			au.Perform(&old_value);

			//Remove entry from index if appropriate.  Note that we would not have
			//processed invisible fields, so they are going to remain for this record.
			//Which is what happens on M204 - the user has to do them explicitly.
			if (ix_reqd) {
				ConvertPreChangeDataType(old_value, pfi);
				AtomicUpdate_RemoveIndexValRec au(this, pfi, old_value);
				au.Perform();
			}
		}

		AtomicUpdate_DeleteEmptyRecord aur(this);
		aur.Perform();

		ebm_check_required = true;
		uu->EndOfMolecule();
	}
	MOLECULE_CATCH(file, uu);
}

//***************************************************************************************
std::string Record::ShowPhysicalInformation()
{
	DatabaseFile* file = home_context->GetDBFile();

	CheckEBM();

	return file->GetDataMgr()->ShowPhysicalInformation(this);
}

//***************************************************************************************
//V3.0
bool Record::GetBLOBDescriptor(const std::string& fname, FieldValue& fval, int occ) const
{
	DatabaseFile* file = home_context->GetDBFile();

	CheckEBM();

	FieldAttributes grpatts;
	PhysicalFieldInfo* pfi = file->GetFieldMgr()->GetAndValidatePFI
			(home_context, fname, false, false, false, &grpatts, source_set_context);

	//Just get normal field value if it's a regular field
	if (pfi) {
		bool exists = file->GetDataMgr()->ReadFieldValue(this, pfi, occ, fval, false);
		fval.ConvertToString();
		return exists;
	}

	return false;
}

} //close namespace


