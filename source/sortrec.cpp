
#include "stdafx.h"

#include "sortrec.h"

//Utils
//API Tiers
#include "sortspec.h"
#include "reccopy.h"
#include "dbf_field.h"
#include "dbctxt.h"
#include "dbfile.h"
//Diagnostics
#include "except.h"
#include "msg_db.h"

namespace dpt {

//*********************************************************************************
int SortRecord::AppendNonKey(const FieldID& f, const FieldValue& v)
{
	fids.push_back(f);
	try {
		fvals.push_back(v);
	}
	catch (...) {
		Clear();
		throw;
	}

	return fids.size() - 1;
}

//*********************************************************************************
void SortRecord::InitializeKeys(int numkeys)
{
	key_infos.resize(numkeys, KeyInfo());
}

//*********************************************************************************
void SortRecord::AppendKey
(const SortRecordsFieldSpec& specfld, const FieldID& f, const FieldValue& v)
{
	//Keys appear in the record as normal
	int fvpix = AppendNonKey(f, v);

	//If the field is of the wrong type for sorting, we make a special extra converted copy
	bool sort_numeric = (specfld.key_type == SORT_NUMERIC);
	bool value_numeric = v.CurrentlyNumeric();

	if (sort_numeric && !value_numeric) {

//		std::string tempval = v.ExtractString();

		//String values that aren't numbers are treated the same as missing fields when
		//considered as sort keys (and therefore sort first)
		RoundedDouble d;
		try {
			d = v.ExtractRoundedDouble(true);

			converted_key_values.push_back(FieldValue(d));

	//		FieldValue& val = converted_key_values.back();
	//		std::string tempval = val.ExtractString();

			//These are indicated by a negative FV pair index (offset by 1 to disambiguate zero)
			//V2.04.  Mar 07.  VC2005 does this differently.
//			fvpix = -converted_key_values.size();
			fvpix = converted_key_values.size();
			fvpix = -fvpix;
		}
		catch (...) {
			fvpix = INT_MAX;
		}
	}
	else if (!sort_numeric && value_numeric) {
		std::string s = v.ExtractString();
		converted_key_values.push_back(FieldValue(s));
		//V2.04.  Mar 07.  VC2005 does this differently.
//		fvpix = -converted_key_values.size();
		fvpix = converted_key_values.size();
		fvpix = -fvpix;
	}

	//Point the key header block at either the real or the converted copy of the field
	KeyInfo& ki = key_infos[specfld.kix];
	ki.fvpix = fvpix;
	ki.dir = specfld.key_dir;
	ki.type = specfld.key_type;
}

//*********************************************************************************
bool SortRecord::operator< (const SortRecord& rhs) const 
{
	for (size_t kix = 0; kix < key_infos.size(); kix++) {

		const KeyInfo& ki = key_infos[kix];
		const KeyInfo& rki = rhs.key_infos[kix];

		//Missing keys always sort first regardless of direction or type
		if (ki.Missing()) {
			if (rki.Missing())
				continue;
			else
				return true;
		}
		if (rki.Missing())
			return false;

		const FieldValue& val = GetKeyVal(ki.fvpix);
		const FieldValue& rval = rhs.GetKeyVal(rki.fvpix);

//		std::string tempval = val.ExtractString();
//		std::string rtempval = rval.ExtractString();

		int cmp;
		if (ki.type == SORT_NUMERIC)
			cmp = val.CompareNumeric(rval);
		else if (ki.type == SORT_CHARACTER)
			cmp = val.CompareString(rval);
		else if (ki.type == SORT_NOCASE)
			cmp = val.CompareNoCaseString(rval);
		else
			cmp = val.CompareRightJustifiedString(rval);

		if (cmp < 0)
			return ki.dir == SORT_ASCENDING;
		if (cmp > 0)
			return ki.dir == SORT_DESCENDING;
	}

	return false; //All keys equal
}

//*********************************************************************************
void SortRecord::ScanFields
(const FieldID fid, int* pnumoccs, int* getocc, FieldValue* getval) const
{
	int dummy;
	int& numoccs = (pnumoccs) ? *pnumoccs : dummy;
	numoccs = 0;

	for (int x = 0; x < NumFVPairs(); x++) {
		if (fids[x] != fid)
			continue;

		numoccs++;

		//Return occurrence value if requested
		if (getocc) {
			if (numoccs == *getocc) {
				*getval = fvals[x];
				return;
			}
		}
	}

	//No luck
	if (getocc)
		*getocc = -1;
}

//*********************************************************************************
int SortRecord::GetFVPix(const FieldID& fid, FieldValue& fval) const
{
	//This is used to get the key value - always 1st occ
	int getocc = 1;
	ScanFields(fid, NULL, &getocc, &fval);
	return getocc;
}

//*********************************************************************************
bool SortRecord::GetFieldValue(const std::string& fname, FieldValue& fval, int getocc) const
{
	if (getocc > 0) {
		PhysicalFieldInfo* pfi = DatabaseFileFieldManager::GetAndValidatePFI
			(home_context, fname, false, false, false);

		if (pfi) {
			ScanFields(pfi->id, NULL, &getocc, &fval);

			if (getocc > 0)
				return true;
		}
	}
	
	//A recordcopy does is a stand-alone object and does not maintain any links to its
	//parent file or context (either of which may therefore close whilst the copy remains
	//in existence.  This means we do not know the type of requested fields that are
	//missing - assume space.
	//Compare this to sorted records which are very similar, but which are child objects
	//of their context, and can hold the field code which is used to look up the 
	//attributes and give a data type.  
	//Also note that for the same reason we can't check for undefined/invisible fields here.
	fval.AssignData("", 0);
	return false;
}

//*********************************************************************************
int SortRecord::CountOccurrences(const std::string& fname) const 
{
	PhysicalFieldInfo* pfi = DatabaseFileFieldManager::GetAndValidatePFI
		(home_context, fname, false, false, false);

	if (pfi) {
		int numoccs;
		ScanFields(pfi->id, &numoccs, NULL, NULL);
		return numoccs;
	}
	else 
		return 0;
}

//*********************************************************************************
void SortRecord::CopyAllInformation(RecordCopy& target) const
{
	target.Clear();
	target.SetRecNum(RecNum());

	for (int x = 0; x < NumFVPairs(); x++) {

		//In theory the field must be there
		PhysicalFieldInfo* pfi = home_context->GetDBFile()->GetFieldMgr()->
			GetPhysicalFieldInfo(home_context, fids[x]);

		if (!pfi)
			throw Exception(DB_ALGORITHM_BUG, "Bug: sorted record PFI has gone");

		target.Append(pfi->name, fvals[x]);
	}
}

//*********************************************************************************
bool SortRecord::GetNextFVPair(std::string& fname, FieldValue& fval, int& fvpix) const
{
	if (fvpix < 0)
		fvpix = 0;
	
	if(fvpix >= NumFVPairs()) {
		fvpix = 0;
		fval = std::string(); 
		return false;
	}

	//In theory the field must be there
	PhysicalFieldInfo* pfi = home_context->GetDBFile()->GetFieldMgr()->
		GetPhysicalFieldInfo(home_context, fids[fvpix]);

	if (!pfi)
		throw Exception(DB_ALGORITHM_BUG, "Bug: sorted record PFI has gone");

	fname = pfi->name;
	fval = fvals[fvpix];
	fvpix++;
	return true;
}

} //close namespace


