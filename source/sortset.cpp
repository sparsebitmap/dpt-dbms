
#include "stdafx.h"

#include "sortset.h"

#include <algorithm>

//Utils
//API Tiers
#include "record.h"
#include "sortspec.h"
#include "dbctxt.h"
#include "dbfile.h"
//Diagnostics
#include "except.h"
#include "msg_db.h"

namespace dpt {

//***************************************************************************************
void SortRecordSet::AppendRec(SortRecord* r)
{
	data.push_back(r);
	try {
		ptrs.push_back(SortRecordPtr(r));
	}
	catch (...) {
		DestroySetData();
		throw;
	}
}

//***************************************************************************************
void SortRecordSet::DestroySetData()
{
	for (size_t x = 0; x < data.size(); x++) {
		if (data[x])
			delete data[x];
		data[x] = NULL;
	}
	data.clear(); 
}

//***************************************************************************************
//This functionality is quite kooky
//***************************************************************************************
void SortRecordSet::GenerateAndAppendEachKeyRotations
(SortRecord* part_record, SortRecordsSpecification* sortspec, 
 std::vector<int>* ek_fixs, std::vector<std::vector<FieldValue> >* ek_vals, int ekix)
{
	try {
		//Use the appropriate field ID and set of values
		int							this_fix		= (*ek_fixs)[ekix];
		const SortRecordsFieldSpec& specfld			= sortspec->fields[this_fix];
		FieldID						this_fid		= specfld.fid;
		std::vector<FieldValue>&	this_ek_vals	= (*ek_vals)[ekix];

		//Generate one rotation for each occurrence of this key
		int numoccs = this_ek_vals.size();
		for (int oix = 0; oix < numoccs; oix++) {

			//Make a fresh copy of the record without this key's occurrences
			SortRecord* fuller_record = new SortRecord(part_record);

			try {
				//Add rotated occurrences
				for (int oix2 = 0; oix2 < numoccs; oix2++) {
					int valix = oix + oix2;
					if (valix >= numoccs)
						valix -= numoccs;

//					std::string tempfld = specfld.name;
//					std::string tempval = this_ek_vals[valix].ExtractString();

					//The first occurrence is the one actually used for sorting
					if (oix2 == 0)
						fuller_record->AppendKey(specfld, this_fid, this_ek_vals[valix]);
					else
						fuller_record->AppendNonKey(this_fid, this_ek_vals[valix]);
				}

				int next_ekix = ekix + 1;

				//Add the rotated occs of further EACH keys if there are any
				if ((size_t)next_ekix < ek_fixs->size())
					GenerateAndAppendEachKeyRotations
						(fuller_record, sortspec, ek_fixs, ek_vals, next_ekix);

				//This is the last (inner) one
				else
					AppendRec(fuller_record);
			}
			catch (...) {
				delete fuller_record;
				throw;
			}
		}

		//The partial record is not needed any more. Note that this means if any
		//the EACH keys have zero occurrences, no records will be added to the set.
		//Put another way, the number of records added to the set is the product of 
		//the occurrence counts of all the EACH keys.
		delete part_record;
	}
	catch (...) {
		delete part_record;
		throw;
	}
}

//***************************************************************************************
void SortRecordSet::PerformSort()
{
	std::stable_sort(ptrs.begin(), ptrs.end());
}

//***************************************************************************************
RecordSetCursor* SortRecordSet::OpenCursor(bool gotofirst)
{
	RecordSetCursor* c = new SortRecordSetCursor(this);
	RegisterNewCursor(c);

	if (gotofirst)
		c->GotoFirst();

	return c;
}

//***************************************************************************************
//V3.0.  See comments in RecordSet.
void SortRecordSet::GetRecordNumberArray_D(int* dest, int getmaxrecs)
{
	for (size_t x = 0; x < ptrs.size(); x++) {
		
		if (getmaxrecs == 0)
			return;
		else
			getmaxrecs--;

		SortRecordPtr recptr = ptrs[x];
		SortRecord* rec = recptr.rec;

		*dest = rec->RecNum();
		dest++;
	}
}

//V3.0.  This complements the real-record version and lets the caller get the
//sort record (i.e. perhaps a subset of fields in a RecordCopy structure).
SortRecord* SortRecordSet::AccessRandomSortRecord(int set_index)
{
	if (set_index > ptrs.size())
		throw Exception(DML_NONEXISTENT_RECORD, "Invalid sort set record index requested");
	
	return ptrs[set_index].rec;
}


//***************************************************************************************
//***************************************************************************************
//***************************************************************************************
//***************************************************************************************
void SortRecordSetCursor::GotoFirst()
{
	ix = 0;
	ValidateAdvancedPosition();
}

//***************************************************************************************
void SortRecordSetCursor::GotoLast()
{
	ix = Set()->Count() - 1; 
	ValidateAdvancedPosition();
}

//***************************************************************************************
void SortRecordSetCursor::Advance(int n)
{
	if (n == 0)
		return;

	ix += n;
	ValidateAdvancedPosition();

	//V2.06 Jul 07.
	PostAdvance(n);
}

//***************************************************************************************
void SortRecordSetCursor::ValidateAdvancedPosition()
{
	if (!InData())
		ix = -1;

	if (ix == -1) {
		SetLARInfo(-1, NULL);
		return;
	}

	//We're on an element in the vector
	ReadableRecord* r = SortSet()->ptrs[ix].rec;
	int recnum = r->RecNum();
	SingleDatabaseFileContext* homefile = r->HomeFileContext();;

	SetLARInfo(recnum, homefile);

	if (homefile)
//* * * Check the meaning of STRECDS on M204
		homefile->GetDBFile()->IncStatRECDS(homefile->DBAPI());
}

//***************************************************************************************
ReadableRecord* SortRecordSetCursor::AccessCurrentRecordForRead()
{
	//In User Language SORT RECORD KEYS the sorted records only contain the keys and
	//all requests for field values are passed through to the real records.
	if (SortSet()->pass_through_flag)
		return AccessCurrentRealRecord();

	if (!InData())
		return NULL;

	//No sense cacheing the previous pointer as nothing has to be created here
	return SortSet()->ptrs[ix].rec;
}

//***************************************************************************************
RecordSetCursor* SortRecordSetCursor::CreateClone()
{
	RecordSetCursor* clone = SortSet()->OpenCursor();

	//In most cases the clone could start processing at the same place
	ExportPosition(clone);

	return clone;
}

//***************************************************************************************
void SortRecordSetCursor::ExportPosition(RecordSetCursor* clone)
{
	SortRecordSetCursor* cast = static_cast<SortRecordSetCursor*>(clone);

	cast->ix = ix;
}

} //close namespace


