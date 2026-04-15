
#include "stdafx.h"

#include "fastunload.h"

//Utils
#include "winutil.h"
#include "dataconv.h"
#include "charconv.h"
#include "parsing.h"
#include "handles.h"
//API tiers
#include "cfr.h"
#include "foundset.h"
#include "dbf_field.h"
#include "dbf_data.h"
#include "dbf_index.h"
#include "dbfile.h"
#include "loaddiag.h"
#include "dbctxt.h"
#include "dbserv.h"
#include "statview.h"
#include "msgroute.h"
#include "core.h"
//Diagnostics
#include "except.h"
#include "msg_db.h"
#include "msg_file.h"

namespace dpt {

void FastUnloadRequest::ThrowBad(const std::string& msg) {throw Exception(DBA_UNLOAD_ERROR, msg);}


//**********************************************************************************************
FastUnloadRequest::FastUnloadRequest
(SingleDatabaseFileContext* c, const FastUnloadOptions& o, const BitMappedRecordSet* s, 
 const std::vector<std::string>* fna, const std::string& d, bool r)
: context(c), opts(o), baseset(s), fnames(NULL), unloaddir(d), 
	reorging(r), diags(NULL), tapef(NULL), taped(NULL), baseset_prepped(false), ebp(NULL)
{
	//Restricting set context must match current context
	if (baseset) {
		if (baseset->Context() != context)
			throw Exception(CONTEXT_MISMATCH, 
				"Context mismatch: unload subset is from a different context");
	}

	//You have to unload something
	if ( !(opts & FUNLOAD_ANYINFO))
		ThrowBad("Fast unload options error: No file areas selected");

	//You're not allowed to create system directories
	if (*(unloaddir.c_str()) == '#' && !util::OneOf(unloaddir.substr(0, 7), "#FASTIO/#REORGS"))
		ThrowBad("Directory names begining with '#' are reserved");

	//Dedupe input field names array
	if (fna) {
		std::set<std::string> deduped;
		
		for (int fnix = 0; fnix < fna->size(); fnix++)
			deduped.insert(fna->at(fnix));

		fnames = new std::vector<std::string>();
		for (std::set<std::string>::const_iterator i = deduped.begin(); i != deduped.end(); i++)
			fnames->push_back(*i);
	}

	//Prepare field include/exclude array indexed by ID for quick use throughout
	bool do_default = true;
	if (fnames && !(opts & FUNLOAD_EXCLUDE_FIELDS)) //specifically included fields only
		do_default = false;

	DatabaseFileFieldManager* fieldmgr = context->GetDBFile()->GetFieldMgr();
	fieldmgr->GetIndexedAttsArray(context, &fieldinfos);

	//An ideal case for using the "arbitrary other info" flag in the PFI objects
	for (size_t x = 0; x < fieldinfos.size(); x++) {
		PhysicalFieldInfo* pfi = fieldinfos[x];
		if (pfi)
			pfi->extra = do_default ? (void*)1 : (void*)0;
	}

	if (fnames) {
		for (size_t x = 0; x < fnames->size(); x++) {

			//This is where we validate the supplied names
			PhysicalFieldInfo* pfi = fieldmgr->GetPhysicalFieldInfo(context, (*fnames)[x]);

			fieldinfos[pfi->id]->extra = (opts & FUNLOAD_EXCLUDE_FIELDS) ? (void*)0 : (void*)1;
		}
	}

	//Minimal diagnostics for this operation by default
	int level = context->DBAPI()->GetParmLOADDIAG();
	if (level == LOADDIAG_DEFAULT)
		level = LOADDIAG_NONE;

	diags = new LoadDiagnostics(level);
	diags->SetCorePtr(context->DBAPI()->Core());
}

//*************************************
FastUnloadRequest::~FastUnloadRequest()
{
	if (fnames)
		delete fnames;
	if (diags)
		delete diags;
	if (ebp)
		context->DestroyRecordSet(ebp);
	CloseTapeFiles();
}

//*************************************
bool FastUnloadRequest::AnyBLOBs()
{
	for (size_t x = 0; x < fieldinfos.size(); x++) {
		PhysicalFieldInfo* pfi = fieldinfos[x];
		if (!pfi)
			continue;

		if (pfi->atts.IsBLOB())
			return true;
	}
	return false;
}

//********************************************************************************************
void FastUnloadRequest::CloseTapeFiles()
{
	if (tapef) {
		delete tapef;
		tapef = NULL;
	}
	if (taped) {
		delete taped;
		taped = NULL;
	}
	for (size_t x = 0; x < tapei_array.size(); x++) {
		if (tapei_array[x]) {
			delete tapei_array[x];
			tapei_array[x] = NULL;
		}
	}
	tapei_array.clear();
}

//********************************************************************************************
void FastUnloadRequest::Perform()
{
	DatabaseServices* dbapi = context->DBAPI();
	CoreServices* core = dbapi->Core();
	MsgRouter* router = core->GetRouter();
	StatViewer* statview = core->GetStatViewer();
	DatabaseFile* file = context->GetDBFile();

	router->Issue(DBA_UNLOAD_INFO, std::string("Fast unload invoked")
		.append((reorging) ? " (reorg phase 1)" : ""));

	if (diags->Any())
		router->Issue(DBA_UNLOAD_INFO_TERM, "...see audit for diagnostics");

	diags->AuditLowSep();
	diags->AuditLow("Fast Unload");

	//Assorted initial diagnostics - e.g. basically echoing the call parameters
	std::string msg;
	if (opts & FUNLOAD_WHAT_F) msg.append(" FDEFS");
	if (opts & FUNLOAD_WHAT_D) msg.append((msg=="") ? " DATA" : ", DATA");
	if (opts & FUNLOAD_WHAT_I) msg.append((msg=="") ? " INDEXES" : ", INDEXES");
	diags->AuditLow(std::string("  Of file area(s):").append(msg));

	if (opts & FUNLOAD_WHAT_D) {
		if (!baseset)
			diags->AuditLow("  For all records");
		else
			diags->AuditLow(std::string("  For record subset - ")
				.append(util::IntToString(baseset->Count())).append(" records"));
	}

	if (!fnames)
		diags->AuditLow("  Including all fields");
	else {
		if (opts & FUNLOAD_EXCLUDE_FIELDS)
			diags->AuditLow("  Excluding these field(s):");
		else
			diags->AuditLow("  Only including these field(s):");

		for (size_t x = 0; x < fnames->size(); x++)
			diags->AuditLow(std::string("    ").append(fnames->at(x)));
	}

	diags->AuditLow(MakeOptsDiagSummary(opts, 2));

	//Open all files up front to check we can do so.  NB. Close them again for now in 
	//case file handles become an issue, and also to clarify locking since other worker
	//threads may populate them in the end.
	if (unloaddir.find(':') == std::string::npos) {
		unloaddir = std::string(util::StartupWorkingDirectory()).append("\\").append(unloaddir);
	}
	util::StdioEnsureDirectoryExists(unloaddir.c_str());

	std::string pref = std::string(unloaddir).append("\\").append(file->FileName(context));

	try {
		if (opts & FUNLOAD_WHAT_F) {
			std::string fname = std::string(pref).append("_TAPEF.DAT");
			tapef = new FastUnloadOutputFile(fname, opts);
			tapef->BaseIO()->Close();
		}

		if (opts & FUNLOAD_WHAT_D) {
			std::string fname = std::string(pref).append("_TAPED.DAT");
			taped = new FastUnloadOutputFile(fname, opts);
			taped->BaseIO()->Close();
		}
		
		if (opts & FUNLOAD_WHAT_I) {
			tapei_array.resize(fieldinfos.size(), NULL);

			for (size_t x = 0; x < fieldinfos.size(); x++) {
				PhysicalFieldInfo* pfi = fieldinfos[x];

				if (pfi && pfi->atts.IsOrdered() && pfi->extra) {
					std::string fname = std::string(pref).append("_TAPEI_");
					fname.append(pfi->name).append(".DAT");
					tapei_array[x] = new FastUnloadOutputFile(fname, opts);
					tapei_array[x]->BaseIO()->Close();
				}
			}
		}
	}
	//Common error, so special succinct message and hint
	catch (Exception& e) {
		if (e.What().find("already exists") != std::string::npos)
			ThrowBad("One or more of the unload files already exists. "
				"Use the (REPLACE) option or delete them first.");
		else
			throw;
	}

	//We can multithread all this if the machine has >1 core
	int loadthrd = dbapi->GetParmLOADTHRD();
	if (loadthrd == 1)
		diags->AuditLow("  Parallel processing is not active");
	else {
		diags->AuditLow(std::string("  Parallel processing is active (")
			.append(util::IntToString(loadthrd)).append(" threads)"));
	}

	diags->AuditLow(std::string("Start time ").append(win::GetCTime()));

	diags->AuditMed("Output file set created OK");
	diags->AuditMed(std::string("  Directory: ").append(unloaddir));

	double stime = win::GetFTimeWithMillisecs();
	diags->StartSLStats(statview, "UNLD");

	//We want a consistent set of extracts, so SHR the CFRS up front.  Actually I decided it
	//gives clearer conflict behaviour for users if we take a SHR record lock first, even though
	//for safety we also then take the CFRs.
	RecordSetHandle h(context->FindRecords(NULL, FD_LOCK_SHR, baseset));

	CFRSentry csd;
	CFRSentry csi;
	if (opts & FUNLOAD_WHAT_D)
		csd.Get(dbapi, file->cfr_direct, BOOL_SHR);
	if (opts & FUNLOAD_WHAT_I)
		csi.Get(dbapi, file->cfr_index, BOOL_SHR);

	//Prepare parallel subtasks - everything read only so all file parts eligible
	util::WorkerThreadTask* task = new util::WorkerThreadTask;
	
	try {

		//Field info
		if (opts & FUNLOAD_WHAT_F)
			task->AddSubTask(new FastUnloadFieldInfoParallelSubTask(this));

		//Data
		if (opts & FUNLOAD_WHAT_D)
			task->AddSubTask(new FastUnloadDataParallelSubTask(this));

		//Indexes
		if (opts & FUNLOAD_WHAT_I) {

			for (size_t i = 0; i < fieldinfos.size(); i++) {
				PhysicalFieldInfo* pfi = fieldinfos[i];

				//Deleted field IDs (consequence of using indexed array)
				if (!pfi)
					continue;

				//Only consider indexed fields
				if (!pfi->atts.IsOrdered())
					continue;

				//Skip any that the user wants to drop
				if (!fieldinfos[pfi->id]->extra)
					continue;

				PrepIndexBaseset();
				task->AddSubTask(new FastUnloadIndexParallelSubTask(this, pfi));
			}
		}

		//Spawn and monitor threads to perform the subtasks
		while (task->NumReadySubTasks() > 0) {

			//If errors occur, quiesce any other running threads and abort the whole thing
			if (task->ErrCode()) {
				task->RequestInterruptAllSubTasks();
				break;
			}

			//Spawn new thread if we have a CPU free
			if (task->NumRunningSubTasks() < loadthrd)
				task->SpawnThread();

			win::Cede(true);
		}

		//Wait for the last threads to finish
		while (task->NumRunningSubTasks() > 0)
			win::Cede(true);

		//Rethrow the exception caught before waiting for parallel quiesce
		if (task->ErrCode()) {
			throw Exception(task->ErrCode(), std::string(
				task->ErrSubTask()->Name()).append(": ").append(task->ErrMsg()));
		}

		//Final diagnostics
		double etime = win::GetFTimeWithMillisecs();
		double elapsed = etime - stime;

		diags->AuditLow(std::string("Finished on ").append(win::GetCTime())
			.append(" (total elapsed: ").append(RoundedDouble(elapsed)
			.ToStringWithFixedDP(1)).append("s)"));

		diags->AuditLowSep();
		router->Issue(DBA_UNLOAD_INFO, "Fast unload complete");
		statview->EndActivity();

		util::WorkerThreadTask::CleanUp(task);
	}
	catch (...) {
		util::WorkerThreadTask::CleanUp(task);
		core->GetRouter()->Issue(DBA_UNLOAD_ERROR, "Exception thrown during unload...");
		throw;
	}
}

//*************************************
//The data unload works off the supplied baseset and automatically takes account of EBP or
//not as per usual convention.  However, with the index unloads we generally want to avoid
//the EBP and just dump the inverted lists directly as that's much faster.  However, the
//user is given the option to apply EBP masking for cases where indexes may be "messy".
//*************************************
void FastUnloadRequest::PrepIndexBaseset()
{
	if (baseset_prepped)
		return;
	
	file_baseset = NULL;
	empty_file_baseset = false;

	//No baseset specified means output all records...
	if (baseset) {
		file_baseset = baseset->GetFileSubSet(0);

		//...but empty base set, or at least empty in the current file, means output none.
		if (file_baseset == NULL || file_baseset->IsEmpty())
			empty_file_baseset = true;
	}

	//Either way we may want to further mask against the EBP
	if (!empty_file_baseset && context->DBAPI()->GetParmLOADCTL() & LOADCTL_UNLOAD_DEORPHAN) {
		ebp = static_cast<DatabaseFileContext*>(context)->FindRecords();

		//Explicit mask, since referback in the above find would have bypassed the EBP.
		if (baseset)
			ebp->BitAnd(baseset);

		file_baseset = ebp->GetFileSubSet(0);
		if (file_baseset == NULL || file_baseset->IsEmpty())
			empty_file_baseset = true;
	}

	baseset_prepped = true;
}


//*************************************
std::string FastUnloadRequest::MakeOptsDiagSummary(const FastUnloadOptions& o, int nspaces)
{
	std::string msg(nspaces, ' ');

	if (!AnyDataReformatOptions(o) && !(o & FUNLOAD_CRLF))
		msg.append("All I/O format options default");

	else {
		msg.append("Non-default I/O format options:");

		if (o & FUNLOAD_FNAMES)  msg.append(" FNAMES");
		if (o & FUNLOAD_NOFLOAT) msg.append((msg=="") ? ", NOFLOAT" : " NOFLOAT");
		if (o & FUNLOAD_EBCDIC)  msg.append((msg=="") ? ", EBCDIC" : " EBCDIC");
		if ( (o & FUNLOAD_ENDIAN) == FUNLOAD_ENDIAN)  
			msg.append((msg=="") ? ", ENDIAN" : " ENDIAN");
		else {
			if (o & FUNLOAD_IENDIAN)  msg.append((msg=="") ? ", IENDIAN" : " IENDIAN");
			if (o & FUNLOAD_FENDIAN)  msg.append((msg=="") ? ", FENDIAN" : " FENDIAN");
		}
		if (o & FUNLOAD_CRLF)  msg.append((msg=="") ? ", CRLF" : " CRLF");
		if (o & FUNLOAD_PAI)   msg.append((msg=="") ? ", PAI" : " PAI");
	}

	return msg;
}










//********************************************************************************************
//********************************************************************************************
void FastUnloadOutputFile::Initialize
(SingleDatabaseFileContext* context, const std::string& what)
{
	//Remember we closed the file after creating it to avoid hitting handle limit
	file.Open(util::STDIO_OLD);

	file.WriteLine(std::string(70, '*'));
	file.WriteLine(std::string("* DPT fast unload file generated on ").append(win::GetCTime()));

	std::string whatline("* File ");
	whatline.append(context->GetDBFile()->FileName(context));
	whatline.append(", ").append(what);
	file.WriteLine(whatline);

	std::string optsline("* Format options");
	optsline.append((opts & FUNLOAD_FNAMES)  ? " +FNAMES"  : " -FNAMES" );
	optsline.append((opts & FUNLOAD_NOFLOAT) ? " +NOFLOAT" : " -NOFLOAT");
	optsline.append((opts & FUNLOAD_EBCDIC)  ? " +EBCDIC"  : " -EBCDIC" );
	
	unsigned int emask = (opts & FUNLOAD_ENDIAN);
	if (emask) {
		if (emask == FUNLOAD_ENDIAN) 
			optsline.append(" +ENDIAN");
		else if (emask == FUNLOAD_IENDIAN) 
			optsline.append(" +IENDIAN -FENDIAN");
		else
			optsline.append(" -IENDIAN +FENDIAN");
	}
	else {
		optsline.append(" -ENDIAN");
	}

	optsline.append((opts & FUNLOAD_CRLF)  ? " +CRLF"    : " -CRLF"   );
	optsline.append((opts & FUNLOAD_PAI)   ? " +PAI"     : " -PAI"    );
	file.WriteLine(optsline);

	file.WriteLine(std::string(70, '*'));

	//All subsequent stuff will be locally buffered.  This gives a good improvement over 
	//calling stdio write for each handful of bytes.
	buffcurr = 0; 
	buff = new char[BUFFMAX];
}


//********************************************************************************************
//Buffer access functions.
//********************************************************************************************
void FastUnloadOutputFile::FlushBuffer()
{
	if (buffcurr) {
		file.Write(buff, buffcurr);
		buffcurr = 0;
	}
}

//**************************
char* FastUnloadOutputFile::GetBuffer(int reqd)
{
	if (reqd > BUFFMAX)
		FastUnloadRequest::ThrowBad("Bug: attempt to unload enormous item");

	if (buffcurr + reqd > BUFFMAX)
		FlushBuffer();

	char* itembuff = buff + buffcurr;
	buffcurr += reqd;
	return itembuff;
}

//**************************
void FastUnloadOutputFile::RewindBuffer(int n) 
{
	assert(n <= buffcurr);

	//This is always possible so long as it's done before writing another item, which
	//might trigger the buffer flush of the previous one. (see textline comment below). 
	buffcurr -= n;
}

//**************************
void FastUnloadOutputFile::AppendString
(const char* s, int len, bool with_length)
{
	//Get one exta byte for the length prefix if required
	char* itembuff;
	if (with_length) {
		itembuff = GetBuffer(len+1);
		*itembuff = len;
		itembuff++;
	}
	else {
		itembuff = GetBuffer(len);
	}

	memcpy(itembuff, s, len);

	//The length byte is not converted, just the text
	if (opts & FUNLOAD_EBCDIC)
		util::AsciiToEbcdic(itembuff, len);
}

//**************************
void FastUnloadOutputFile::AppendTextLine(const char* s, unsigned _int8 len)
{
	//This is done atomically rather than in two chunks just to make the index
	//unload neater when we might want to call RewindBuffer() - see above.
	char* itembuff = GetBuffer(len+2);

	itembuff[len] = '\r';
	itembuff[len+1] = '\n';

	if (s) {
		memcpy(itembuff, s, len);
	
		//CRLF is not converted
		if (opts & FUNLOAD_EBCDIC)
			util::AsciiToEbcdic(itembuff, len);
	}
}

//**************************
void FastUnloadOutputFile::AppendBinaryInt16(short i)
{
	short* pibuff = reinterpret_cast<short*>(GetBuffer(2));
	*pibuff = i;

	if (opts & FUNLOAD_IENDIAN)
		util::ReverseInt16(pibuff);
}

//************************
void FastUnloadOutputFile::AppendBinaryUint16Array
(unsigned short* source, unsigned short nentries)
{
	int nbytes = 2 * nentries;

	//Allocate and write out buffer
	unsigned short* sbuff = reinterpret_cast<unsigned short*>(GetBuffer(nbytes));
	memcpy(sbuff, source, nbytes);

	//Endianize each 2 bytes if required
	if (! (opts & FUNLOAD_IENDIAN) )
		return;

	//There is a slightly (10-20% cheaper) algorithm for doing this but it's less intuitive
	//and longer code so I went with the obvious.  The other way is to output the buffer
	//offset by one, then skip up in twos moving every other byte back two.
	unsigned short* send = sbuff + nentries;
	while (sbuff != send) {
		util::ReverseInt16((short*)sbuff);
		sbuff++;
	}
}


//**************************
void FastUnloadOutputFile::AppendBinaryInt32(int i)
{
	int* pibuff = reinterpret_cast<int*>(GetBuffer(4));
	*pibuff = i;

	if (opts & FUNLOAD_IENDIAN)
		util::ReverseInt32(pibuff);
}

//**************************
void FastUnloadOutputFile::AppendBinaryDouble(const double& d)
{
	double* pdbuff = reinterpret_cast<double*>(GetBuffer(8));
	*pdbuff = d;

	if (opts & FUNLOAD_FENDIAN)
		util::ReverseDouble(pdbuff);
}

//**************************
void FastUnloadOutputFile::AppendRawData(const char* source, int len)
{
	//No conversion during this, so we can deal with the buffers neatly.

	//The alternatives here are used because with with a large write it's actually
	//faster (on this machine anyway, but it does make sense) to go straight to 
	//stdio and save a big memcpy.  With smaller writes though using our local
	//buffer gets faster quickly.  The actual figures are based on some rough tests
	//I did just now where the crossover point was around 4-8K.  This makes it
	//relevant since bitmap pages and large table B records are both common.
	bool do_local;

	if (len < 4000)
		do_local = true;         //"small" write
	else if (buffcurr == 0)
		do_local = false;        //local buffer empty so no flush complications
	else if (buffcurr > 4000)
		do_local = false;        //local buffer nice and large - flush is OK
	else if (len > 8000)
		do_local = false;        //huge write - take hit on poss small flush
	else
		do_local = true;         //middling either way

	if (do_local)
		memcpy(GetBuffer(len), source, len);
	else {
		FlushBuffer();
		file.Write(source, len);
	}
}

} //close namespace

