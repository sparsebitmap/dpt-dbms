
#include "stdafx.h"

#include "fastload.h"
#include "fastunload.h"

#include "windows.h"

//Utils
#include "winutil.h"
#include "charconv.h"
#include "dataconv.h"
#include "lineio.h"
#include "liostdio.h"
//API tiers
#include "loaddiag.h"
#include "molecerr.h"
#include "update.h"
#include "bmset.h"
#include "page_b.h"
#include "fieldinfo.h"
#include "dbf_ebm.h"
#include "dbf_data.h"
#include "dbf_field.h"
#include "dbf_index.h"
#include "du1step.h"
#include "dbfile.h"
#include "dbctxt.h"
#include "dbserv.h"
#include "msgroute.h"
#include "statview.h"
#include "core.h"
//Diagnostics
#include "except.h"

//For eyeball mode
#ifdef _BBHOST
#include "iodev.h"
#endif

namespace dpt {

void FastLoadRequest::ThrowBad(const std::string& msg, int badcode) const {
	throw Exception(badcode, msg);
}

//**********************************************************************************************
FastLoadRequest::FastLoadRequest
(SingleDatabaseFileContext* c, const FastLoadOptions& o, int e, 
 BB_OPDEVICE* ed, const std::string& d, bool r)
: context(c), eyeball(e), opts(o), eyedest(ed), loaddir(d), reorging(r),
	tapef(NULL), taped(NULL), combined_tapei(NULL), errtape(NULL), loaded_recset(NULL),
	recnum_xref_opbuff_current(-1), recnum_xref_oldrn_lowest(INT_MAX), recnum_xref_oldrn_highest(0),
	recnum_xref_file(NULL), recnum_xref_table(NULL), any_orphan_ilrec(false), completed_ok(false)
{
	DatabaseServices* dbapi = context->DBAPI();

#ifndef _BBHOST
	if (!eyedest) eyedest = dbapi->Core()->Output();
#endif

	fieldinfos_by_tapef_id.resize(4000, NULL);

	//Scan for input files so we can decide what we're going to do
	std::string filename = context->GetDBFile()->FileName(context);

	std::string pref = loaddir;
	if (loaddir.find(':') == std::string::npos) {
		pref = util::StartupWorkingDirectory();
		pref.append("\\").append(loaddir);
	}
	pref.append("\\");
	
	std::string filepatt = std::string(pref).append(filename).append("_TAPE").append("*.DAT");
	recnum_xref_table_filename = std::string(pref).append(filename).append("_RECXREF.DAT");

	WIN32_FIND_DATA fileinfo;
	HANDLE handle = FindFirstFile(filepatt.c_str(), &fileinfo);

	int more = 1;
	while (more) {
		std::string dosfile = fileinfo.cFileName;
		int tpos = dosfile.find("_TAPE");
		std::string part = dosfile.substr(tpos + 5, dosfile.length() - filename.length() - 9);

		if (part.length() > 0) {
			std::string dospath = std::string(pref).append(dosfile);

			if (part == "F")
				tapef = new FastLoadInputFile(this, dospath);
			else if (part == "D")
				taped = new FastLoadInputFile(this, dospath);
			else if (part == "I")
				combined_tapei = new FastLoadInputFile(this, dospath);
			else if (part.length() > 2 && part.substr(0,2) == "I_") {
				std::string fieldname = part.substr(2);
				tapeis[fieldname] = new FastLoadInputFile(this, dospath, fieldname);
			}
		}
		more = FindNextFile(handle, &fileinfo);
	}
	FindClose(handle);

	int level = dbapi->GetParmLOADDIAG();

	//Minimal diagnostics for this operation by default.
	//Also if eyeball mode is on, the printed data will *be* the diagnostics
	if (level == LOADDIAG_DEFAULT || eyeball)
		level = LOADDIAG_NONE;

	diags = new LoadDiagnostics(level);
	diags->SetCorePtr(dbapi->Core());

	loadctl_flags = dbapi->GetParmLOADCTL();
}

//*************************************
FastLoadRequest::~FastLoadRequest()
{
	for (size_t x = 0; x < fieldinfos.size(); x++)
		delete fieldinfos[x];
	if (loaded_recset)
		delete loaded_recset;
	if (recnum_xref_file)
		delete recnum_xref_file;
	if (recnum_xref_table)
		delete[] recnum_xref_table;
	if (diags)
		delete diags;

	DisposeTapeFiles();
}

//*************************************
void FastLoadRequest::DisposeTapeFiles()
{
	bool clean = (opts & FLOAD_CLEANAFTER) && completed_ok;

	if (tapef) {
		FastLoadInputFile::Destroy(tapef, clean);
		tapef = NULL;
	}
	if (taped) {
		FastLoadInputFile::Destroy(taped, clean);
		taped = NULL;
	}
	while (!tapeis.empty()) {
		FastLoadInputFile::Destroy(tapeis.begin()->second, clean);
		tapeis.erase(tapeis.begin());
	}
	if (combined_tapei) {
		FastLoadInputFile::Destroy(combined_tapei, clean);
		combined_tapei = NULL;
	}
}


//********************************************************************************************
void FastLoadRequest::Perform_Part1()
{
	DatabaseServices* dbapi = context->DBAPI();
	CoreServices* core = dbapi->Core();
	MsgRouter* router = core->GetRouter();

	if (eyeball) {
		std::string msg("Fast Load Preview Mode (show ");
		if (eyeball > 0 && eyeball != INT_MAX)
			msg.append(util::IntToString(eyeball));
		else {
			eyeball = INT_MAX;
			msg.append("ALL");
		}
		msg.append(" entries)");
		eyedest->WriteLine(msg);
	}
	else {
		router->Issue(DBA_UNLOAD_INFO, std::string("Fast load invoked")
			.append((reorging) ? " (reorg phase 2)" : ""));

		if (diags->Any())
			router->Issue(DBA_LOAD_INFO_TERM, "...see audit for diagnostics");

		diags->AuditLowSep();
		diags->AuditLow("Fast Load");
		diags->AuditLow(std::string("Started on ").append(win::GetCTime()));
	}

	//List the input files we found
	diags->AuditLow("Input files");
	diags->AuditLow(std::string("  Directory: ").append(loaddir));

	if (tapef) 
		diags->AuditLow("  TAPEF");
	if (taped) 
		diags->AuditLow("  TAPED");

	//Split out a combined TAPEI first
	if (combined_tapei) {
		diags->AuditLow("  Combined TAPEI");
		if (tapeis.size() != 0)
			ThrowBad("Combined TAPEI conflicts with one or more separate TAPEIs");

		combined_tapei->SplitCombinedTapeI();
	}

	if (!tapef && !taped && tapeis.size() == 0)
		ThrowBad("No input files to process");

	if (tapeis.size() != 0) {
		diags->AuditLow(std::string("  TAPEI for ")
			.append(util::IntToString(tapeis.size())).append(" fields"));
	}

	//Record number Xref temp file
	if (taped && tapeis.size() != 0) {
		recnum_xref_file = new util::BBStdioFile(recnum_xref_table_filename.c_str(), util::STDIO_CCLR);
		recnum_xref_file->SetDeleteAtCloseFlag();
	}
}

//********************************************************************************************
void FastLoadRequest::Perform_Part2()
{
	DatabaseServices* dbapi = context->DBAPI();
	DatabaseFile* file = context->GetDBFile();
	StatViewer* statview = dbapi->Core()->GetStatViewer();

	double stime = win::GetFTimeWithMillisecs();
	diags->StartSLStats(statview, "LOAD");

	try {
		if (opts & FLOAD_NOACTION) {
			completed_ok = true;
			diags->AuditLow("Stopping now because 'no action' mode selected");
		}
		else {
			completed_ok = Perform_Part2a();
		}

		Perform_Part2b(stime);
	}
	MOLECULE_CATCH(file, dbapi->GetUU());

	statview->EndActivity();
}

//***********************************
bool FastLoadRequest::Perform_Part2a()
{
	DatabaseServices* dbapi = context->DBAPI();
	CoreServices* core = dbapi->Core();
	MsgRouter* router = core->GetRouter();
	DatabaseFile* file = context->GetDBFile();

	//-----------------------
	//Field information
	//-----------------------

	//Note any info in table A beforehand
	std::vector<PhysicalFieldInfo> pre_fi;
	file->GetFieldMgr()->TakeFieldAttsTableCopy(context, &pre_fi);
	
	//Read and load TAPEF
	int newfields = 0;
	if (tapef) {
		newfields = file->GetFieldMgr()->Load(this, eyedest);			
		if (newfields == -1)
			return false;
	}

	//Make an amalgamated set of the two sources of field info
	for (size_t x = 0; x < pre_fi.size(); x++) {
		const PhysicalFieldInfo& fi = pre_fi[x];

		AddFieldInfo(fi.name, &fi, false, false);
	}
	
	//-----------------------
	//Planning phase
	//-----------------------

	if (taped || AnyTapeI()) {

		//No new fields defined earlier means errors in this part can be benign.
		int badcode = (newfields == 0 && !reorging) ? TXN_BENIGN_ATOM : DBA_LOAD_ERROR;

		if (!eyeball)
			diags->AuditHigh("Strategy planning phase");

		//Now we know what fields we have, close any irrelevant TAPEIs
		std::map<std::string, FastLoadInputFile*> tapeis_final;
		std::map<std::string, FastLoadInputFile*>::iterator iter;
		for (iter = tapeis.begin(); iter != tapeis.end(); iter++) {

			FastLoadInputFile* ti = iter->second;
			const std::string& fname = ti->TapeIFieldName();

			FastLoadFieldInfo* info = GetLoadFieldInfoByName(fname, false);

			bool indexed = false;
			if (info)
				indexed = info->TableA_Atts().IsOrdered(); //actual atts now

			if (info && indexed)
				tapeis_final[iter->first] = iter->second;
			else {
				const char* reason = (!info) ? "nonexistent" : "non-indexed";
				router->Issue(DBA_LOAD_INFO, 
					std::string("Warning: TAPEI file was found for ").append(reason)
					.append(" field '").append(fname).append("' - will ignore"));
				FastLoadInputFile::Destroy(iter->second, false);
			}

		}
		tapeis = tapeis_final;

		//Make sure we can handle all indexed fields somehow
		int num_fields = fieldinfos.size();
		int num_indexed_fields = 0;
		std::map<std::string, std::string> warntapeifields;

		for (int i = 0; i < num_fields; i++) {
			FastLoadFieldInfo* info = fieldinfos[i];
			const std::string& fname = info->Name();
			const FieldAttributes& atts = info->TableA_Atts();

			if (atts.IsOrdered()) {
				num_indexed_fields++;

				//See if we have a TAPEI for the field
				std::map<std::string, FastLoadInputFile*>::iterator iter = tapeis.find(fname);
				if (iter != tapeis.end())
					info->SetTapeI(iter->second);

				//Missing TAPEI might be OK
				else {
					if (atts.IsInvisible()) 
						warntapeifields[fname] = "the field is invisible";
					else if (!taped)
						warntapeifields[fname] = "no TAPED is present";
				}
			}
		}
		int num_dynamic = num_indexed_fields - tapeis.size() - warntapeifields.size();

		//Check for changes that aren't currently handled
		ValidateFieldDefChanges(badcode);
		int num_data_reformats = NumDataFormatChanges();

		diags->AuditHigh(std::string("  Total fields now: ").append(util::IntToString(num_fields)));
		diags->AuditHigh(std::string("  Indexed: ").append(util::IntToString(num_indexed_fields)));
		if (num_indexed_fields) {
			diags->AuditHigh(std::string("    ").append(util::IntToString(tapeis.size()))
				.append(" with TAPEI present"));

			if (taped) {
				diags->AuditHigh(std::string("    ").append(util::IntToString(num_dynamic))
					.append(" will get index entries built during the data load"));
			}
		}

		if (warntapeifields.size() != 0) {
			diags->AuditHigh(std::string("    ").append(util::IntToString(warntapeifields.size()))
				.append(" with no TAPEI and can't build index from data:"));

			std::map<std::string, std::string>::const_iterator iter;
			for (iter = warntapeifields.begin(); iter != warntapeifields.end(); iter++) {
				diags->AuditHigh(std::string("      ").append(iter->first)
					.append(" (").append(iter->second).append(")"));
			}
		}

		if (taped) {
			diags->AuditHigh(std::string("  Fields changing table B storage format: ")
				.append(util::IntToString(num_data_reformats)));
		}
	}

	//-----------------------
	//Record data
	//-----------------------

	if (taped) {

		//Read input and load
		if (file->GetDataMgr()->Load(this, eyedest) == -1)
			return false;

		//Flick all the EBM bits for records just loaded
		if (loaded_recset) {
			file->GetEBMMgr()->ExistizeLoadedRecords(dbapi, loaded_recset);

			//Not a huge amount but reclaim this memory before index load
			delete loaded_recset;
			loaded_recset = NULL;
		}
	}

	//-----------------------
	//Indexes
	//-----------------------

	//Might be a pre-supplied file and/or DU1 entries generated above for different fields
	if (AnyTapeI() || file->IsInDeferredUpdateMode()) {
		BuildRecNumXrefTable();

		if (file->GetIndexMgr()->Load(this, eyedest) == -1)
			return false;

		if (any_orphan_ilrec)
			router->Issue(DBA_LOAD_INFO, "Warning: orphan index entries were stored (LOADCTL X'02')");
	}

	return true;
}

//***********************************
void FastLoadRequest::Perform_Part2b(double stime)
{
	DatabaseServices* dbapi = context->DBAPI();
	MsgRouter* router = dbapi->Core()->GetRouter();

	const char* delmsg = "Deleting input files";

	if (eyeball) {
		eyedest->WriteLine(Sep());
		if (opts & FLOAD_CLEANAFTER)
			eyedest->WriteLine(delmsg);
	}
	else {

		//Finally commit loaded data
		dbapi->GetUU()->EndOfMolecule(true);

		if (opts & FLOAD_CLEANAFTER)
			diags->AuditLow(delmsg);

		double etime = win::GetFTimeWithMillisecs();
		double elapsed = etime - stime;
		diags->AuditLow(std::string("Fast load finished on ").append(win::GetCTime())
			.append(" (total elapsed: ").append(RoundedDouble(elapsed)
			.ToStringWithFixedDP(1)).append("s)"));
	
		diags->AuditLowSep();

		router->Issue(DBA_LOAD_INFO, "Fast load complete");
	}
}






//**********************************************************************************************
void FastLoadRequest::AddFieldInfo
(const std::string& fname, const PhysicalFieldInfo* pfi, bool d, 
 bool was_in_tapef, FieldID tfid, const FieldAttributes& atts)
{
	//We've already got everything we need if the field existed before the load
	if (fieldinfos_by_name.find(fname) != fieldinfos_by_name.end()) {
		if (was_in_tapef)
			ThrowBad(std::string("Input field name appears twice: ").append(fname));
		return;
	}

	//Validate the pseudo field IDs coming in from TAPEF, since they might be user-supplied
	if (tfid < -1 || tfid > 4000) {
		ThrowBad(std::string("Invalid field code (").append(util::IntToString(tfid))
			.append(") - must be in range 0 to 4000"));
	}
	if (tfid != -1 && fieldinfos_by_tapef_id[tfid] != NULL) {
		ThrowBad(std::string("Input field code has been used already: ")
			.append(util::IntToString(tfid)));
	}

	//Create info object
	fieldinfos.push_back(new FastLoadFieldInfo(fname, pfi, d, was_in_tapef, tfid, atts));
	FastLoadFieldInfo* info = fieldinfos.back();

	//Only store lookup by FID if TAPEF contained one
	if (tfid != -1)
		fieldinfos_by_tapef_id[tfid] = info;

	//Build lookup by name
	fieldinfos_by_name[fname] = info;
}

//************************************
FastLoadFieldInfo::FastLoadFieldInfo
(const std::string& n, const PhysicalFieldInfo* p, bool d, 
 bool tf, FieldID tfi, const FieldAttributes& a) 
: name(n), local_pfi(n, a, tfi, -1), actual_pfi(p), defined_this_load(d), 
	was_in_tapef(tf), tapef_id(tfi), tapedi_atts(a), tapei(NULL)
{
	//Most cases - either field info as pre-existing, or as loaded from TAPEF.  The
	//only case where there is no pfi pointer is eyeball mode and the file is empty.
	if (actual_pfi) {
		local_pfi.atts       = actual_pfi->atts;
		local_pfi.id         = actual_pfi->id;
		local_pfi.btree_root = actual_pfi->btree_root;
		local_pfi.extra      = actual_pfi->extra;
	}

	//In loads with no TAPEF, the TAPED and TAPEI input formats will be as per the real atts.
	//Note however (see GotAllTapeFIDs()) that if using FIDs in TAPED, we require TAPEF FIDs.
	if (!was_in_tapef)
		tapedi_atts = local_pfi.atts;
}


//************************************
FastLoadFieldInfo* FastLoadRequest::GetLoadFieldInfoByTapeFID
(int fid, bool throwit) const
{
	if (fid < 0 || fid > 4000) {
		if (throwit)
			ThrowBad(std::string("Invalid field code (").append(util::IntToString(fid))
				.append(") - must be in range 0 to 4000"));
		else
			return NULL;
	}

	FastLoadFieldInfo* info = fieldinfos_by_tapef_id[fid];

	if (info == NULL) {
		if (throwit)
			ThrowBad(std::string("No field information available with TAPEF ID = ")
				.append(util::IntToString(fid)));
		else
			return NULL;
	}

	return info;
}

//************************************
FastLoadFieldInfo* FastLoadRequest::GetLoadFieldInfoByName
(const std::string& name, bool throwit) const
{
	std::map<std::string, FastLoadFieldInfo*>::const_iterator i;
	i = fieldinfos_by_name.find(name);

	if (i == fieldinfos_by_name.end()) {
		if (throwit)
			ThrowBad(std::string("No field information available for ").append(name));
		else
			return NULL;
	}

	return i->second;
}

//************************************
bool FastLoadRequest::AnyDynamicIndexBuild() const
{
	for (size_t x = 0; x != fieldinfos.size(); x++) {
		if (fieldinfos[x]->DynamicIndexBuild())
			return true;
	}
	return false;
}

//************************************
bool FastLoadRequest::AnyBLOBs() const
{
	for (size_t x = 0; x != fieldinfos.size(); x++) {
		if (fieldinfos[x]->IsBLOB())
			return true;
	}
	return false;
}

//************************************
bool FastLoadRequest::GotAllTapeFIDs() const
{
	for (size_t x = 0; x != fieldinfos.size(); x++) {
		FastLoadFieldInfo* info = fieldinfos[x];

		if (!info->WasInTapeF())
			continue;

		if (info->TAPEF_ID() == -1)
			return false;
	}
	return true;
}

//************************************
bool FastLoadRequest::AnyTapeFIDChanges() const
{
	for (size_t x = 0; x != fieldinfos.size(); x++) {
		const FastLoadFieldInfo* info = fieldinfos[x];

		if (!info->WasInTapeF())
			continue;

		if (info->TAPEF_ID() != info->TableA_ID())
			return true;
	}
	return false;
}

//************************************
int FastLoadRequest::NumDataFormatChanges() const
{
	int result = 0;

	for (size_t x = 0; x != fieldinfos.size(); x++) {
		const FastLoadFieldInfo* info = fieldinfos[x];

		if (!info->WasInTapeF())
			continue;

		const FieldAttributes& atts_tape = info->TAPEDI_Atts();
		const FieldAttributes& atts_reqd = info->TableA_Atts();

		if (atts_tape.IsInvisible() != atts_reqd.IsInvisible())
			result++;
		else if (atts_tape.IsFloat() != atts_reqd.IsFloat())
			result++;
	}

	return result;
}

//************************************
void FastLoadRequest::ValidateFieldDefChanges(int badcode) const
{
	for (size_t x = 0; x != fieldinfos.size(); x++) {
		const FastLoadFieldInfo* info = fieldinfos[x];

		if (!info->WasInTapeF())
			continue;
		
		const FieldAttributes& atts_tapef = info->TAPEDI_Atts();
		const FieldAttributes& atts_reqd = info->TableA_Atts();

		const char* err = NULL;

		//Index collating order
		if (atts_tapef.IsOrdered() && atts_reqd.IsOrdered() &&
			atts_tapef.IsOrdNum() != atts_reqd.IsOrdNum())
		{
			//If the user sneakily removes TAPEI after the unload they can effect
			//an order change that way (although using REDEFINE would be quicker).
			if (info->TapeI())
				err = "ORDERED type";
		}

		//Invisible to visible
		if (atts_tapef.IsInvisible() && atts_reqd.IsVisible())
			err = "INVISIBLE to VISIBLE";

		if (err) {
			std::string msg = "Unsupported field format change for '";
			msg.append(info->Name()).append("' (").append(err).append(")");
			msg.append(" - use the REDEFINE command instead");
			ThrowBad(msg, badcode);
		}
	}
}







//********************************************************************************************
void FastLoadRequest::NoteStoredRecNum(int recnum_in, int recnum_stored)
{
	//Bitmap for flicking EBP bits at the end of TAPED
	if (!loaded_recset)
		loaded_recset = new BitMappedFileRecordSet(context);
	
	loaded_recset->FastAppend(recnum_stored);

	if (!AnyTapeI())
		return;

	//Xref table for handling TAPEI contents later. These are written to a file so as to 
	//not create a huge table in memory which would nerf the deferred index updates, which 
	//need a lot of memory. The table is read back in at the end and built into a lookup table.
	recnum_xref_opbuff_current++;

	//(a little extra buffering since this needs to be a fast function)
	if (recnum_xref_opbuff_current == RECNUM_XREF_BUFFSIZE) {
		recnum_xref_file->Write(recnum_xref_opbuff, RECNUM_XREF_BUFFSIZE * 8);
		recnum_xref_opbuff_current = 0;
	}
	recnum_xref_opbuff[recnum_xref_opbuff_current] = RecNumXrefOpEntry(recnum_in, recnum_stored);

	//Maintain LWM and HWM for array size later
	if (recnum_in > recnum_xref_oldrn_highest)
		recnum_xref_oldrn_highest = recnum_in;
	
	if (recnum_in < recnum_xref_oldrn_lowest)
		recnum_xref_oldrn_lowest = recnum_in;
}

//*************************************
void FastLoadRequest::BuildRecNumXrefTable()
{
	//No TAPED records loaded at all
	if (recnum_xref_opbuff_current == -1)
		return;

	//Flush our little part-full buffer
	recnum_xref_file->Write(recnum_xref_opbuff, (recnum_xref_opbuff_current+1) * 8);

	//Create memory array and initialize to -1's (see next function for why)
	int entries = recnum_xref_oldrn_highest - recnum_xref_oldrn_lowest + 1;
	recnum_xref_table = new int[entries];
	memset((void*)recnum_xref_table, 0xFF, entries * sizeof(int));

	//Populate with pairs read back in from the file
	RecNumXrefOpEntry pair;

	recnum_xref_file->Seek(0);
	for (;;) {
		if (recnum_xref_file->Read(&pair, 8) != 8)
			break;
		
		int offset = pair.oldrn - recnum_xref_oldrn_lowest;
		recnum_xref_table[offset] = pair.newrn; 
	}
}

//*************************************
int FastLoadRequest::XrefRecNum(int oldrn)
{
	//If just loading indexes, forget this part.  This might commonly happen when doing 
	//single-field reorgs.
	if (!taped)
		return oldrn;

	//Otherwise check that we loaded the record from TAPED.
	int xrn = -1;

	if (oldrn >= recnum_xref_oldrn_lowest && oldrn <= recnum_xref_oldrn_highest) {
		int offset = oldrn - recnum_xref_oldrn_lowest;
		xrn = recnum_xref_table[offset];
	}

	if (xrn == -1) {

		if (! (loadctl_flags & LOADCTL_LOAD_ORPHAN_ILRECS)) {
			ThrowBad(std::string("Inverted list record number #")
				.append(util::IntToString(oldrn))
				.append(" matches no record just loaded - reset LOADCTL parameter to override"));
		}

		//Assume the user knows what they're doing
		xrn = oldrn;
		any_orphan_ilrec = true;
	}
	
	return xrn;
}








//********************************************************************************************
//********************************************************************************************
FastLoadInputFile::FastLoadInputFile
(FastLoadRequest* r, const std::string& dosfile, const std::string& tif)
: file(dosfile.c_str(), util::STDIO_RDONLY), tapei_field(tif), request(r), 
	opts(0), data_start(0), delete_file(false)
{
	//We open the file at this early stage to save breaking the DB file if for some reason
	//all the tape files can't be opened - e.g. in use somewhere.  But close again till
	//needed as there may be issues with file handles.
	CloseFile();
}

//**************************
FastLoadInputFile::~FastLoadInputFile()
{
	//Should be closed by now but just in case
	CloseFile();

	if (delete_file)
		file.DeleteTheFile();
}


//**************************
bool FastLoadInputFile::Initialize(bool save_comments)
{
	OpenFile();

	static const int MAXCOMLEN = 10000;
	char buff[MAXCOMLEN];
	int rdlen = file.Read(buff, MAXCOMLEN);
	if (rdlen == 0) {
		CloseFile();
		return false;
	}

	std::string sbuff(buff, rdlen);

	//Rather awkward parsing of the comment block.  The idea was to have something easy
	//to type and read as the convention for introducing comments, that was extremely
	//unlikely to feature as data (hence delimiters are at least 22 characters long!)
	const static int ASTER_REQD = 20;
	std::string term_ascii = std::string(ASTER_REQD, '*').append("\r\n");
	std::string term_ebcdic = util::AsciiToEbcdic(term_ascii.c_str());

	size_t termposa = sbuff.find(term_ascii);
	size_t termpose = sbuff.find(term_ebcdic);

	//No comments
	if (termposa == std::string::npos && termpose == std::string::npos) {
		file.SeekI64(0);
		return true;
	}

	//The following isn't foolproof if people mix and match ascii and ebcdic asterisks
	//but why would they do that?
	size_t termpos = termposa;
	char asterisk = '*';
	std::string term = term_ascii;
	if (termposa == std::string::npos) {
		term = term_ebcdic;
		termpos = termpose;
		asterisk = util::AsciiToEbcdic('*');
	}

	//The required format is the first line must be entirely asterisks
	if (sbuff.substr(0, termpos) != std::string(termpos, asterisk)) {

		//Rewind (the header indicator must have been found inside a data field)
		file.SeekI64(0);
		return true;
	}

	int comments_start = termpos + ASTER_REQD + 2;

	//OK now find the end of the comment block
	termpos = sbuff.find(term, comments_start);
	if (termpos == std::string::npos)
		request->ThrowBad("Invalid header/comment block in input file (more than 10K of comments?)");

	//File is all comments and nothing else
	data_start = termpos + ASTER_REQD + 2;
	if (data_start == rdlen) {
		CloseFile();
		return false;
	}

	//There is data and comments.  Scan comments for options to apply to the data.
	std::string comments = sbuff.substr(comments_start, termpos - comments_start);
	if (save_comments)
		saved_comments = comments;

	//We want to search for EBCDIC keywords too
	comments.append(util::EbcdicToAscii(comments.c_str())); 

	if (comments.find("+FNAMES") != std::string::npos) opts |= FUNLOAD_FNAMES;  
	if (comments.find("+NOFLOAT") != std::string::npos) opts |= FUNLOAD_NOFLOAT;  
	if (comments.find("+EBCDIC") != std::string::npos) opts |= FUNLOAD_EBCDIC;  
	if (comments.find("+ENDIAN") != std::string::npos) opts |= FUNLOAD_ENDIAN;  
	if (comments.find("+IENDIAN") != std::string::npos) opts |= FUNLOAD_IENDIAN;  
	if (comments.find("+FENDIAN") != std::string::npos) opts |= FUNLOAD_FENDIAN;  
	if (comments.find("+CRLF") != std::string::npos) opts |= FUNLOAD_CRLF;  
	if (comments.find("+PAI") != std::string::npos) opts |= FUNLOAD_PAI;  

	//Now reposition at the start of the data proper (known to be at least 1 byte now)
	file.SeekI64(data_start);

	return true;
}

//**************************
std::string FastLoadInputFile::MakeOptsDiagSummary(int nspaces)
{
	return FastUnloadRequest::MakeOptsDiagSummary(opts, nspaces);
}

//**************************
void FastLoadInputFile::UnexpectedEOF(int required)
{
	std::string msg("Unexpected EOF reading trying to read ");
	if (required >= 0)
		msg.append(util::IntToString(required)).append(" bytes");
	else
		msg.append(" a line");
	msg.append(" from ").append(file.Name());
	request->ThrowBad(msg);
}

//**************************
//A rudimentary attempt at some useful features of full session line input.
//Can't use the real thing cos the load functions need to work for API level users.
void FastLoadInputFile::ReadCommandTextLine(std::string& cmd, bool* eof_flag)
{
	std::string line = ReadTextLine(eof_flag);
	if (eof_flag && *eof_flag)
		return;

	//Ignore blank lines or comments - effectively line continuations here.
	util::ReplaceChar(line, '\t', ' ');
	util::UnBlank(line, false);
	util::ToUpper(line);

	//In which case read another physical line
	if (line.length() == 0 || line[0] == '*')
		ReadCommandTextLine(cmd, eof_flag);
	
	else {

		//Line continuation with hyphen.  Annoying but we need this because the
		//D FIELD (DDL) command on Model 204 generates output with continued lines.
		char lastch = line[line.length()-1];
	
		if (lastch == '-')
			line.resize(line.length()-1);

		cmd.append(line);

		if (lastch == '-')
			ReadCommandTextLine(cmd, eof_flag);
	}
}

//**************************
//Note that we assume 'CRLF' is always ASCII, since on the mainframe a sequential file would
//be arranged with RECFM/LRECL etc.   Hence we can use our usual ReadLine() function here
//and only convert the actual text.
std::string FastLoadInputFile::ReadTextLine(bool* eof_flag, char pretermchar)
{
	std::string s;

	//EOF
	if (file.ReadLine(s, false, pretermchar))
		HitEOF(eof_flag, -1);

	if (EbcdicOption())
		util::EbcdicToAscii(s);

	return s;
}

//**************************
int FastLoadInputFile::ReadBinaryInt32(bool* eof_flag)
{
	char buff[4];
	ReadBytes(buff, 4, eof_flag);

	int result = *(reinterpret_cast<int*>(buff));
	if (opts & FUNLOAD_IENDIAN)
		util::ReverseInt32(&result);

	return result;
}

//**************************
short FastLoadInputFile::ReadBinaryInt16(bool* eof_flag)
{
	char buff[2];
	ReadBytes(buff, 2, eof_flag);

	short result = *(reinterpret_cast<short*>(buff));
	if (opts & FUNLOAD_IENDIAN)
		util::ReverseInt16(&result);

	return result;
}

//**************************
unsigned short FastLoadInputFile::ReadBinaryUint16(bool* eof_flag)
{
	char buff[2];
	ReadBytes(buff, 2, eof_flag);

	unsigned short result = *(reinterpret_cast<unsigned short*>(buff));
	if (opts & FUNLOAD_IENDIAN)
		util::ReverseInt16(reinterpret_cast<short*>(&result));

	return result;
}

//**************************
void FastLoadInputFile::ReadBinaryUint16Array
(unsigned short* buff, int nentries, bool* eof_flag)
{
	int nbytes = 2 * nentries;
	ReadBytes(buff, nbytes, eof_flag);

	if (! (opts & FUNLOAD_IENDIAN) )
		return;

	//See comments in the unload version on the endianizing algorithm
	unsigned short* send = buff + nentries;
	while (buff != send) {
		util::ReverseInt16((short*)buff);
		buff++;
	}
}

//**************************
unsigned _int8 FastLoadInputFile::ReadBinaryUint8(bool* eof_flag)
{
	char c;
	ReadBytes(&c, 1, eof_flag);

	//No ebcdic or endianism to do with this one
	return c;
}

//**************************
double FastLoadInputFile::ReadBinaryDouble(bool* eof_flag)
{
	char buff[8];
	ReadBytes(buff, 8, eof_flag);

	double result = *(reinterpret_cast<double*>(buff));
	if (opts & FUNLOAD_FENDIAN)
		util::ReverseDouble(&result);

	return result;
}

//**************************
void FastLoadInputFile::ReadRawData(void* dest, int len, bool* eof_flag)
{
	ReadBytes(dest, len, eof_flag);
}

//**************************
void FastLoadInputFile::ReadChars(void* dest, int len, bool* eof_flag)
{
	ReadBytes(dest, len, eof_flag);

	if (opts & FUNLOAD_EBCDIC)
		util::EbcdicToAscii((char*)dest, len);
}

//**************************
void FastLoadInputFile::ReadCRLF()
{
	char buff[3] = {0,0,0};
	ReadRawData(buff, 2);

	if (buff[0] == '\r' && buff[1] == '\n')
		return;

	request->ThrowBad(std::string("Expected CRLF terminator, not ")
		.append(util::AsciiStringToHexString(buff, true)));
}

//**************************
std::string FastLoadInputFile::FilePosDetailed()
{
	_int64 pos = file.TellI64();
	std::string result = util::Int64ToString(pos);
	result.append("(");
	result.append(util::Int64ToHexString(pos, 0, true));
	result.append(")");
	return result;
}

//**************************
void FastLoadInputFile::SplitCombinedTapeI() 
{
	LoadDiagnostics* diags = request->Diags();
	std::map<std::string, FastLoadInputFile*>* tapeis = request->TapeIs();

	if (!Initialize(true)) {
		diags->AuditLow("    (empty)");
		return;
	}

	if (!PaiOption())
		request->ThrowBad("Combined TAPEI is only valid with 'PAI' format");

	diags->AuditLow("    (Splitting...");

	//NB. The file scan/copy functions used below were written for plain stdio files, 
	//not the local-buffered WinAPI version, so close and reopen to save rewrites.
	_int64 filepos = file.TellI64();
	const char* ctiname = file.Name().c_str();
	file.Close();
	util::BBStdioFile cti(ctiname, util::STDIO_RDONLY);
	cti.SeekI64(filepos);

	std::map<std::string, std::string> tempfiles;
	int ctihandle = cti.Handle();
	static const std::string twoblanklines = "\r\n\r\n\r\n";

	try {

		//Process for each field in the combined file
		for (;;) {

			//The field name is a text line
			bool eof = false;
			std::string fieldname;
			while (!eof && fieldname.length() == 0)
				eof = cti.ReadLine(fieldname);
			if (eof)
				break;

			if (EbcdicOption())
				util::EbcdicToAscii(fieldname);

			//Check field not encountered before (either in combined file or on its own)
			if (tempfiles.find(fieldname) != tempfiles.end() ||
				tapeis->find(fieldname) != tapeis->end())
			{
				request->ThrowBad(std::string("Duplicate field name chunk for ")
					.append(fieldname).append(" in combined TAPEI"));
			}

			_int64 startpos = cti.TellI64();

			//The entries for each field are delimited by 2 blank lines
			_int64 endpos = util::StdioFindString(ctihandle, ctiname, twoblanklines, startpos);
			if (endpos == -1) {
				request->ThrowBad(std::string("Could not find two blank lines after '")
					.append(fieldname).append("' entries in combined TAPEI"));
			}

			//Create a fresh output file
			std::string tapei_filename = std::string(cti.Name().substr(0, cti.Name().length() - 4))
				.append("_").append(fieldname).append(".DAT");
			util::BBStdioFile temp(tapei_filename.c_str(), util::STDIO_NEW);
			tempfiles[fieldname] = tapei_filename;

			//Medatada header section, including a copy of the original
			temp.WriteLine(std::string(70, '*'));
			temp.WriteLine("* DPT fast load input file split out from combined TAPEI");
			temp.WriteLine(std::string("* - ").append(ctiname));
			temp.WriteLine(std::string("* - ").append(win::GetCTime()));
			if (saved_comments.length() != 0) {
				temp.WriteLine("* Original comments block:");
				temp.Write(saved_comments.c_str(), saved_comments.length());
			}
			temp.WriteLine(std::string(70, '*'));

			//Now a big chunky copy of the data from the field name to the two blank lines.
			//Technically we could have done this while searching for the CRLFCRLF, but that
			//would have been a little messy if the CRLFCRLF was not be found.
			util::StdioCopyFile(temp.Handle(), temp.Name().c_str(), ctihandle, ctiname, 
					startpos, endpos - startpos + 4); //take one of the blank lines

			//Position at the next field name if possible
			cti.SeekI64(endpos + 6, SEEK_SET);
		}

		cti.Close();

		//Split-out is complete, so we can now make the temp files fully-fledged single TAPEIs
		std::map<std::string, std::string>::iterator i;
		for (i = tempfiles.begin(); i != tempfiles.end(); ++i)
			(*tapeis)[i->first] = new FastLoadInputFile(request, i->second, i->first);

		diags->AuditLow("    ...OK)");
	}
	catch (...) {
		cti.Close();

		//Delete the temp files again if we never got a complete split-out done
		std::map<std::string, std::string>::iterator i;
		for (i = tempfiles.begin(); i != tempfiles.end(); ++i)
			util::StdioDeleteFile(i->second.c_str());

		throw;
	}
}







//************************************************************************************
void FastLoadRecordBuffer::InitReformattingBuffer(bool tbn)
{
	throw_badnums = tbn;

	//The only time 3* size would be insufficient for a float->string conversion 
	//is if every field on the record was absurdly small with lots of leading
	//zeros after its decimal point.  Let's take the hit of a possible crash there.
	tempbuff = new char[TotalLen() * 3]; 
	putptr = tempbuff;
}

//**************************
void FastLoadRecordBuffer::CommitReformatting()
{

	Clear(); 
	AdoptChunk(tempbuff, putptr - tempbuff); 
	tempbuff = NULL;
}

//**************************
void FastLoadRecordBuffer::GetFVPair(FieldID& fid, FieldValue& fval)
{
	Extract(&fid, 2);

	if (RecordDataPage::RealFieldIsFloat(fid)) {
		fid = RecordDataPage::RealFieldCode(fid);

		double floatbuff; 
		Extract(&floatbuff, 8);

		fval = floatbuff;
	}

	//Note that BLOB fields never come through here (chunk Xfer mode is disabled
	//if there are any in the file), so 1 char length is always the case.
	//else if (RecordDataPage::RealFieldIsBLOB(fid)) {
	//	;
	//}

	else {
		unsigned _int8 stringlen;
		Extract(&stringlen, 1);

		char* buff = fval.AllocateBuffer(stringlen);
		Extract(buff, stringlen);
	}
}

//**************************
//This is only called if we have to extend the record.
int FastLoadRecordBuffer::FindHighestFVPBoundaryAtOrBelow(int pageavail)
{
	//First call do a one-off run through the buffer to find the FVP boundaries, which
	//we can then use and not have to look ahead/rewind etc. later in the extraction.
	if (!got_fvp_boundaries) {
		fvp_boundaries.reserve(8000);

		while (RemainingToExtract()) {

			FieldID fid;
			Extract(&fid, 2);

			if (RecordDataPage::RealFieldIsFloat(fid)) {
				Advance(8);
			}
			else {
				unsigned _int8 stringlen;
				Extract(&stringlen, 1);

				Advance(stringlen);
			}

			fvp_boundaries.push_back(BytesExtracted());
		}

		got_fvp_boundaries = true;
		ResetExtraction();
	}

	int record_curpos = BytesExtracted();
	int record_breakpos = record_curpos;
	int scanlimit = record_curpos + pageavail;

	//Each call after that scan through the cached pointers.  Since we are extending a
	//record this part is not critical so just scan from the start.
	for (size_t x = 0; x < fvp_boundaries.size(); x++) {
		int fvpend = fvp_boundaries[x];
		
		if (fvpend > scanlimit)
			break;
		else
			record_breakpos = fvpend;
	}

	//Return the length of the extent we can store - at or below that requested
	return record_breakpos - record_curpos;
}

//**************************
void FastLoadRecordBuffer::PutReformattedFVPairFromChunk
(const PhysicalFieldInfo* pfi, FieldValue& taped_fval)
{
	bool tableb_visible_reqd = pfi->atts.IsVisible();

	//Build reformatted buffer data as per how table B will want it
	if (tableb_visible_reqd) {

		bool tableb_float_reqd = pfi->atts.IsFloat();
		
		//FID
		FieldID tableb_fid = pfi->id;
		if (tableb_float_reqd)
			RecordDataPage::MakeNumericPageCode(tableb_fid);
	
		*(reinterpret_cast<FieldID*>(putptr)) = tableb_fid;
		putptr += 2;

		//Value
		FieldValue& tableb_fval = taped_fval;
		if (tableb_float_reqd) {

			//As with REDEFINE FIELD we take a streamlined view of the FMODL
			//issue and respect the 1 bit, but quietly, and with no other niceties.
			tableb_fval.ConvertToNumeric(throw_badnums);

			*(reinterpret_cast<RoundedDouble*>(putptr)) = *(tableb_fval.RDData());
			putptr += 8;
		}
		else {
			tableb_fval.ConvertToString();

			unsigned _int8 vallen = tableb_fval.StrLen();
			*putptr = vallen;
			putptr++;

			memcpy(putptr, tableb_fval.StrChars(), vallen);
			putptr += vallen;
		}
	}
}

} //close namespace


