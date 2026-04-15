
#include "stdafx.h"

#include "reccopy.h"
#include "except.h"
#include "msg_db.h"

namespace dpt {

//*********************************************************************************
void RecordCopy::Append(const std::string& s, const FieldValue& v)
{
	fnames.push_back(s);
	try {
		fvals.push_back(v);
	}
	catch (...) {
		Clear();
		throw;
	}
}

//*********************************************************************************
//Used by both the read functions
//*********************************************************************************
void RecordCopy::ScanFields
(const std::string& fname, int* pnumoccs, int* getocc, FieldValue* getval) const
{
	int dummy;
	int& numoccs = (pnumoccs) ? *pnumoccs : dummy;
	numoccs = 0;

	for (int x = 0; x < NumFVPairs(); x++) {
		if (fnames[x] != fname)
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
bool RecordCopy::GetFieldValue(const std::string& fname, FieldValue& fval, int getocc) const
{
	if (getocc > 0)
		ScanFields(fname, NULL, &getocc, &fval);

	if (getocc > 0)
		return true;
	
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
int RecordCopy::CountOccurrences(const std::string& fname) const 
{
	int numoccs;
	ScanFields(fname, &numoccs, NULL, NULL);
	return numoccs;
}

//*********************************************************************************
void RecordCopy::CopyAllInformation(RecordCopy& target) const
{
	target.Clear();
	target.SetRecNum(RecNum());

	for (int x = 0; x < NumFVPairs(); x++)
		target.Append(fnames[x], fvals[x]);
}

//*********************************************************************************
bool RecordCopy::GetNextFVPair(std::string& fname, FieldValue& fval, int& fvpix) const
{
	if (fvpix < 0)
		fvpix = 0;
	
	if (fvpix >= NumFVPairs()) {
		fvpix = 0;
		fval = std::string(); 
		return false;
	}

	fname = fnames[fvpix];
	fval = fvals[fvpix];
	fvpix++;
	return true;
}

//*********************************************************************************
//V2.18 May 09. New class.
void StoreRecordTemplate::Append(const std::string& s, const FieldValue& v)
{
	ClearFids();
	RecordCopy::Append(s, v);
}

//************************************
void StoreRecordTemplate::SetFieldName(unsigned int fix, const std::string& s)
{
	ClearFids();

	if (fnames.size() > fix)
		fnames[fix] = s;
	else if (fnames.size() == fix)
		fnames.push_back(s);
	else {
		fnames.resize(fix+1);
		fnames[fix] = s;
	}
}

//************************************
void StoreRecordTemplate::SetFieldValue(unsigned int fix, const FieldValue& v)
{
	if (fvals.size() > fix)
		fvals[fix] = v;
	else if (fnames.size() == fix)
		fvals.push_back(v);
	else {
		fvals.resize(fix+1);
		fvals[fix] = v;
	}
}

//************************************
void StoreRecordTemplate::AppendFid(PhysicalFieldInfo* pfi)
{
	fids.push_back(pfi);
	if (fids.size() == fnames.size())
		got_fids = true;
}

//********************************
void StoreRecordTemplate::Validate() const
{
	if (fnames.size() != fvals.size())
		throw Exception(DB_BAD_PARM_MISC, 
			"Invalid STORE template: field/value count mismatch");
}


} //close namespace
