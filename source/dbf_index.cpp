
#include "stdafx.h"

#include "dbf_index.h"

//Utils
#include "winutil.h"
#include "dataconv.h"
//API tiers
#include "cfr.h" //#include "CFR.h" : V2.24 case is less interchangeable on *NIX - Roger M.
#include "dbctxt.h"
#include "record.h"
#include "bmset.h"
#include "handles.h"
#include "foundset.h"
#include "dbfile.h"
#include "du1step.h"
#include "fastunload.h"
#include "fastload.h"
#include "loaddiag.h"
#include "dbf_find.h"
#include "dbf_field.h"
#include "btree.h"
#include "fieldinfo.h"
#include "dbserv.h"
#include "seqserv.h"
#include "core.h"
#include "msgroute.h"
//Diagnostics
#include "except.h"
#include "msg_db.h"

//For eyeball mode
#ifdef _BBHOST
#include "iodev.h"
#endif

namespace dpt {

//****************************************************************************************
bool DatabaseFileIndexManager::Atom_AddValRec
(PhysicalFieldInfo* pfi, const FieldValue& value, 
 SingleDatabaseFileContext* sfc, int recnum, BTreeAPI_Load* bt_load)
{
	CFRSentry cs(sfc->DBAPI(), file->cfr_index, BOOL_EXCL);

	//Find the value in the index (insert if not present).
	//V2.14 Jan 09.  See comments in Augment below - avoid scan from root during load.
	BTreeAPI dummy;
	BTreeAPI* bt = &dummy;
	if (bt_load)
		bt = bt_load;
	else
		bt->Initialize(sfc, pfi);

	if (!bt->LocateValueEntry(value))
		bt->InsertValueEntry(value);

	//Add the new record to the inverted list
	InvertedListAPI il = bt->InvertedList();
	return il.AddRecord(recnum);
}

//****************************************************************************************
bool DatabaseFileIndexManager::Atom_RemoveValRec
(PhysicalFieldInfo* pfi, const FieldValue& value, SingleDatabaseFileContext* sfc, int recnum)
{
	CFRSentry cs(sfc->DBAPI(), file->cfr_index, BOOL_EXCL);

	//Find the value in the index (DON'T insert if not present)
	BTreeAPI bt(sfc, pfi);
	if (!bt.LocateValueEntry(value))
		return false;

	//Remove the record from the inverted list
	InvertedListAPI il = bt.InvertedList();
	bool record_was_removed = il.RemoveRecord(recnum);

	//We may have left the entry with no more records - in which case remove from index
	if (record_was_removed && !il.IndexValueValid())
		bt.RemoveValueEntry();

	return record_was_removed;
}

//****************************************************************************************
bool DatabaseFileIndexManager::Atom_ReplaceValRecSet
(PhysicalFieldInfo* pfi, const FieldValue& value, SingleDatabaseFileContext* sfc,
 BitMappedFileRecordSet* set, BitMappedFileRecordSet** pp_tbo_set)
{
	DatabaseServices* dbapi = sfc->DBAPI();

	CFRSentry cs(dbapi, file->cfr_index, BOOL_EXCL);

	//Find the value in the index (insert if not present)
	BTreeAPI bt(sfc, pfi);
	if (!bt.LocateValueEntry(value)) {

		//Actually we may as well skip the insert if the new set is empty, since we
		//would just end up removing the entry again below.
		if (!set)
			return false;
		if (set->IsEmpty())
			return false;

		bt.InsertValueEntry(value);
	}

	//Replace the old inverted list with the new
	InvertedListAPI il = bt.InvertedList();
	bool tbo_set_created = il.ReplaceRecordSet(set, pp_tbo_set);

	//Index entry will now be redundant if replacement new set was empty (see also above)
	if (!il.IndexValueValid()) {
		try {
			bt.RemoveValueEntry();
		}
		catch (...) {
			if (pp_tbo_set)
				if (*pp_tbo_set && tbo_set_created)
					delete *pp_tbo_set;
			throw;
		}
	}

	return true;
}

//****************************************************************************************
//V2.14 Jan 09.  This processing is sufficiently different now from FILE RECORDS to 
//make it clearer as a separate function.
//****************************************************************************************
void DatabaseFileIndexManager::Atom_AugmentValRecSet
(PhysicalFieldInfo* pfi, const FieldValue& value, SingleDatabaseFileContext* sfc,
 BitMappedFileRecordSet* set, BTreeAPI_Load* bt_load)
{
	//NB. No need for INDEX CFR here as we have the entire file in EXCL

	//Nothing to do if the incoming set is empty
	if (!set || set->IsEmpty())
		return;

	//By holding onto the BTreeAPI object across calls we can often avoid the need 
	//to scan from the root for each successive value, and also scan each page for
	//branches/value entries as we do so.  A major saving during a big btree build.
	BTreeAPI dummy;
	BTreeAPI* bt = &dummy;
	DU1FlushStats* stats = NULL;
	if (bt_load) {
		bt = bt_load;
		stats = bt_load->DUFlushStats();
	}
	else {
		bt->Initialize(sfc, pfi);
	}

	//...so this call should actually require no DKPR and no leaf scanning at all, 
	//because we already have things positioned from the last entry appended.
	if (!bt->LocateValueEntry(value))
		bt->InsertValueEntry(value);


	//----------------------------------------------------------------------
	//V3.0: Note re. locking and parallelism during loads:
	//----------------------------------------------------
	//We have locked CFR_INDEX exclusively at load start so by spawning 
	//parallel threads beneath that we're taking back responsibility for 
	//protecting everything that would normally be protected by that file-
	//wide CFR, which means several diverse structures.  The btrees are of
	//course maintained separately per field, but other things are shared.
	//  1. Table D page allocation: (already protected by 'heap_lock')
	//  2. ILMRs, allocation and usage: (problem)
	//  3. Inverted lists in array form, allocation and usage: (problem)
	//     {Inverted lists in bitmap form are not shared}
	//In an ideal world the file design would have 2 and 3 as structures not
	//shared between fields, but that's too big a change to make now.  So 
	//the name of the game is to enable as neatly as possible the parallel 
	//access to the btrees and bitmaps while serializing access to 2 and 3.
	//Unfortunately inverted list processing is complicated, involving 
	//multi-segment lists containing a mixture of arrays (shared) and bitmaps
	//(non-shared), possible promotion/demotion between those forms, various
	//locks already used at some levels, recursive calls in places.  All of 
	//which makes it virtually impossible to introduce a new factor of
	//e.g. page-level (would be ideal) or other highly targeted locks, 
	//without risking deadly embraces.  Therefore I've gone with this 
	//compromise, basically ruling out any potential parallel processing
	//that would have been possible copying data into bitmap pages, but at
	//least delaying serialization till this point where we've got the input 
	//file processing and btree value inserts done in parallel.  Sadly
	//this phase (ILMRs and IL arrays) is quite a CPU-intensive one.  The
	//result is that I think 2 threads might be quite profitable (one doing
	//btree work and one processing inverted lists) but not so much 3+.
	//----------------------------------------------------------------------
	LockingSentry s(&parallel_load_lock);

	bt->InvertedList().AugmentRecordSet(set, stats);
}










//*************************************************************************************
//V2.19 June 09.  For both delete field and redefine field. (V3.0: also reorgs)
void DatabaseFileIndexManager::DeleteFieldIndex
(SingleDatabaseFileContext* sfc, PhysicalFieldInfo* pfi)
{
	BTreeAPI bt(sfc, pfi);

	//First pass to delete all inverted lists.  (See tech notes re algorithm choice)
	for (bt.LocateLowestValueEntryPreBrowse();
			bt.LastLocateSuccessful();
			bt.WalkToNextValueEntry()) 
	{
		bt.InvertedList().ReplaceRecordSet(NULL, NULL);
	}

	//Second pass to delete the btree itself
	bt.DeleteAllNodes();
}

//*************************************************************************************
//V2.19 June 09.  For redefine from non-ordered to ordered
void DatabaseFileIndexManager::CreateIndexFromData
(SingleDatabaseFileContext* sfc, PhysicalFieldInfo* pfi, bool create_num_index)
{
	DatabaseServices* dbapi = sfc->DBAPI();

	//Prepare data structures for building the new index
	pfi->atts.SetOrderedFlag();
	if (create_num_index)
		pfi->atts.SetOrdNumFlag();
	else
		pfi->atts.ClearOrdNumFlag();	

	file->EnterOneStepDUMode(dbapi, LOADDIAG_NONE, DatabaseFile::DU1_REDEFINE);
	file->du_1step_info->InitializeForSingleField(pfi);

	try {

		//For each record loop
		RecordSetHandle h(((DatabaseFileContext*)sfc)->FindRecords());
		for (RecordSetCursorHandle cursor(h.GetSet()); cursor.Accessible(); cursor.Advance(1)) {
			Record* record = cursor.GetLiveCursor()->AccessCurrentRealRecord_B(true);

			//For each ccurrence loop
			FieldValue val;
			for (int n = 1; ; n++) {
				if (!record->data_access.GetFieldValue(pfi, n, val))
					break;

				//Convert format if required
				if (create_num_index)
					//V2.23. Bad numeric values are now optionally allowed
					val.ConvertToNumeric(sfc->DBAPI()->GetParmFMODLDPT() & 1);
					//val.ConvertToNumeric(true);
				else
					val.ConvertToString();

				//Add {value/recnum} pair to the deferred index updates
				if (file->du_1step_info->AddEntry(sfc, pfi, val, record->RecNum()))
					file->du_1step_info->Flush(sfc, DeferredUpdate1StepInfo::MEMFULL);
			}
		}

		//At the end build the index from the deferred update structures collected above
		file->du_1step_info->Flush(sfc, DeferredUpdate1StepInfo::REDEFINE);
		file->ExitOneStepDUMode(dbapi);
	}
	catch (...) {
		file->ExitOneStepDUMode(dbapi);
		throw;
	}
}

//*************************************************************************************
//V2.19 June 09.  For redefine between index collating orders
//*NOTE* See tech docs re. this algorithm choice, particularly use of std::sort.
//*************************************************************************************
void DatabaseFileIndexManager::ChangeIndexType
(SingleDatabaseFileContext* sfc, PhysicalFieldInfo* pfi)
{
	//Extact values and inverted list pointers from the existing index
	BTreeExtract treedata(sfc, pfi);

	//Delete old btree
	BTreeAPI(sfc, pfi).DeleteAllNodes();

	//Change format flag
	if (pfi->atts.IsOrdNum())
		pfi->atts.ClearOrdNumFlag();
	else
		pfi->atts.SetOrdNumFlag();

	//Get the values ready
	//V2.23. Bad numeric values can now optionally be handled
	treedata.ConvertAndSort(sfc->DBAPI()->GetParmFMODLDPT() & 1);
	//treedata.ConvertAndSort();

	//Reload the values in their new order, linking them back up with the inverted lists, 
	//which haven't moved anywhere.
	BTreeAPI_Load btnew;
	btnew.Initialize(sfc, pfi);
	btnew.BuildFromExtract(treedata);
}








//*************************************************************************************
FastUnloadIndexParallelSubTask::FastUnloadIndexParallelSubTask
(FastUnloadRequest* r, PhysicalFieldInfo* p)
: WorkerThreadSubTask(std::string("Index for ").append(p->name)), 
	request(r), pfi(p) 
{}

//********************************************
void FastUnloadIndexParallelSubTask::Perform()
{
	request->Context()->GetDBFile()->GetIndexMgr()->Unload(request, pfi);
}

//*************************************************************************************
void DatabaseFileIndexManager::Unload(FastUnloadRequest* request, PhysicalFieldInfo* pfi)
{
	SingleDatabaseFileContext* context = request->Context();
	LoadDiagnostics* diags = request->Diags();

	const std::vector<FastUnloadOutputFile*>& tapei_array = request->TapeIArray();
	bool nofloat_option = request->NofloatOption();
	bool is_ordnum = pfi->atts.IsOrdNum();
	bool pai_option = request->PaiMode();

	double fstime = win::GetFTimeWithMillisecs();

	FastUnloadOutputFile* tapei = tapei_array[pfi->id];
	tapei->Initialize(context, std::string("index information for field ").append(pfi->name));

	diags->AuditMed(std::string("Starting index unload for ").append(pfi->name));

	int numvals = 0;
	int numsegs = 0;

	//Process the inverted list for each value.
	if (!request->EmptyFileBaseSet()) {

		BTreeAPI bt(context, pfi);

		for (bt.LocateLowestValueEntryPreBrowse();
				bt.LastLocateSuccessful();
				bt.WalkToNextValueEntry()) 
		{
			//Output value in various formats
			FieldValue* fv = bt.GetLastValueLocatedPtr();
			
			int valoutsize;

			if (!is_ordnum) {
				if (pai_option) {
					tapei->AppendTextLine(fv->StrChars(), fv->StrLen());
					valoutsize = fv->StrLen() + 2;
				}
				else {
					tapei->AppendString(fv->StrChars(), fv->StrLen(), true);
					valoutsize = fv->StrLen() + 1;
				}
			}
			else {
				if (!nofloat_option && !pai_option) {
					valoutsize = 8;
					tapei->AppendBinaryDouble(fv->ExtractRoundedDouble().Data());
				}
				else {
					std::string sval = fv->ExtractString();		
					if (pai_option) {
						tapei->AppendTextLine(sval);
						valoutsize = sval.length() + 2;
					}
					else {
						tapei->AppendString(sval, true);
						valoutsize = sval.length() + 1;
					}
				}
			}

			//Output inverted lists (maybe none after base set masking)
			int valsegs = bt.InvertedList().Unload(tapei, 
				request->FileBaseSet(), request->CrlfOption(), pai_option);

			//If all recs masked off, unwrite the value itself (see validity comment in this func)
			if (valsegs == 0)
				tapei->RewindBuffer(valoutsize);
			else {
				numvals++;
				numsegs += valsegs;
			}
		}
	}

	tapei->Finalize();

	double fetime = win::GetFTimeWithMillisecs();
	double felapsed = fetime - fstime;

	diags->AuditMed(std::string("  Index unload complete for ").append(pfi->name)
		.append(" (FID=").append(util::IntToString(pfi->id)).append(")"));
	diags->AuditHigh(std::string("  FID=").append(util::IntToString(pfi->id))
		.append(": ").append(util::IntToString(numvals)).append(" values, ")
		.append(util::IntToString(numsegs)).append(" segment entries, in ")
		.append(RoundedDouble(felapsed).ToStringWithFixedDP(1)).append("s"));
}

//*************************************************************************************
int DatabaseFileIndexManager::Load(FastLoadRequest* request, BB_OPDEVICE* eyedest)
{
	SingleDatabaseFileContext* context = request->Context();
	DatabaseServices* dbapi = context->DBAPI();
	CoreServices* core = dbapi->Core();
	MsgRouter* router = core->GetRouter();
	LoadDiagnostics* diags = request->Diags();

	int eyeball = request->EyeballRecs();

	try {

		try {
			if (eyeball && request->AnyTapeI()) {
				LoadEyeball(request, eyedest);
				return 0;
			}

			diags->AuditMed("Starting index load/build");

			//Attach the TAPEIs to the DU1 structure
			if (request->AnyTapeI()) {

				if (!file->IsInDeferredUpdateMode())
					file->EnterOneStepDUMode(dbapi, diags->GetLevel(), DatabaseFile::DU1_TAPEI);

				std::map<std::string, FastLoadInputFile*>* tapeis = request->TapeIs();
				std::map<std::string, FastLoadInputFile*>::const_iterator i;

				for (i = tapeis->begin(); i != tapeis->end(); i++) {
					const std::string& fieldname = i->first;
					FastLoadInputFile* tapei = i->second;

					std::string msg = std::string("  Attaching input file for ")
						.append(fieldname).append(": ");

					if (!tapei->Initialize())
						msg.append("(nothing to load)");

					else {
						msg.append(tapei->MakeOptsDiagSummary(0));

						PhysicalFieldInfo* pfi = file->GetFieldMgr()->GetPhysicalFieldInfo(context, fieldname);
						file->du_1step_info->AttachFastLoadTapeI(context, pfi, tapei);
					}

					diags->AuditHigh(msg);
				}
			}

			//DU1 - du your thing
			DeferredUpdate1StepInfo* du1 = file->du_1step_info;
			
			du1->Diags()->DisableSeparators();
			du1->Flush(context, DeferredUpdate1StepInfo::REORG);
			du1->Diags()->EnableSeparators();

			if (file->IsInDeferredUpdateMode())
				file->ExitOneStepDUMode(dbapi);

			return 0;
		}
		catch (Exception& e) {
			if (eyeball)
				eyedest->WriteLine("");

			std::string msg("Error processing TAPEI");

			FastLoadInputFile* errtape = request->GetErrTape();
			if (errtape && errtape->FileIsOpen())
				msg.append(" before offset ").append(errtape->FilePosDetailed());
				
			msg.append(": ").append(e.What());
			router->Issue(e.Code(), msg);

			//Most of the time these will be parsing issues and we won't have even touched
			//the file yet, but there is no easy way to differentiate these from problems
			//making a file update during a field define, so sadly always phys broken now.
			throw Exception(DBA_LOAD_ERROR, "Serious load error");
		}
		catch (...) {
			if (eyeball)
				eyedest->WriteLine("");

			std::string msg("Unknown error processing TAPEI");

			FastLoadInputFile* errtape = request->GetErrTape();
			if (errtape && errtape->FileIsOpen())
				msg.append(" before offset ").append(errtape->FilePosDetailed());
			
			router->Issue(DBA_LOAD_ERROR, msg);
			throw;
		}
	}
	catch (...) {
		if (file->IsInDeferredUpdateMode())
			file->ExitOneStepDUMode(dbapi);

		//In eyeball mode there's no need to leave the file physically inconsistent
		if (!eyeball)
			throw;
		return -1;
	}
}

//*************************************************************************************
bool PrintEyeballInvListRec
(int listrec, int& prtrecs, int eyeball, BB_OPDEVICE* eyedest)
{
	if (prtrecs >= eyeball)
		return false;

	prtrecs++;

	if (prtrecs > 1) {
		eyedest->Write(",");

		if ((prtrecs % 10) == 1) {
			eyedest->WriteLine("");
			eyedest->Write("          ");
		}
	}

	//Note that since this is eyeball mode, the data phase will not have loaded 
	//any fresh records, so the rec# Xref is not required.  Just show incoming rec#.
	eyedest->Write(util::IntToString(listrec));

	return true;
}

//*************************************************************************************
//No need to complicate the DU1 module with eyeball processing.  Plus we want all 
//the eyeball output on this thread and not worker threads.  Do it all right here.
//*************************************************************************************
void DatabaseFileIndexManager::LoadEyeball(FastLoadRequest* request, BB_OPDEVICE* eyedest)
{
	int eyeball = request->EyeballRecs();

	eyedest->WriteLine(request->Sep());
	eyedest->WriteLine("Indexes");

	std::map<std::string, FastLoadInputFile*>* tapeis = request->TapeIs();
	std::map<std::string, FastLoadInputFile*>::const_iterator i;

	for (i = tapeis->begin(); i != tapeis->end(); i++) {
		const std::string& fieldname = i->first;
		FastLoadInputFile* tapei = i->second;
		request->SetErrTape(tapei);

		if (tapei->Initialize()) {
			eyedest->WriteLine(request->Sep());
			eyedest->Write(fieldname);
			eyedest->Write(" (");
			eyedest->Write(tapei->MakeOptsDiagSummary(0));
			eyedest->WriteLine(")");
			eyedest->WriteLine(request->Sep());
		}
		else {
			eyedest->WriteLine(request->Sep());
			eyedest->WriteLine(fieldname);
			eyedest->WriteLine(std::string(fieldname.length(), '-'));
			eyedest->WriteLine("  Nothing to load");
			continue;
		}

		bool crlf_option = tapei->CrlfOption();
		bool pai_option = tapei->PaiOption();

		FastLoadFieldInfo* info = request->GetLoadFieldInfoByName(fieldname);
		bool float_values_in_tape = info->TAPEDI_Atts().IsOrdNum() && !tapei->NofloatOption() && !pai_option;

		//Process as many values as requested
		bool eof = false;
		int nvals = 0;
		while (nvals < eyeball) {

			//The value in various formats
			FieldValue value;

			if (pai_option)
				value = tapei->ReadTextLine(&eof);
			else if (float_values_in_tape)
				value = tapei->ReadBinaryDouble(&eof);
			else {
				unsigned _int8 valuelen = tapei->ReadBinaryUint8(&eof);
				if (!eof) {
					char vbuff[256];
					tapei->ReadChars(vbuff, valuelen);
					vbuff[valuelen] = 0;
					value = vbuff;
				}
			}
			if (eof)
				break;

			eyedest->Write("  ");
			eyedest->WriteLine(value.ExtractString());
			eyedest->Write("    Recs: ");

			int prtrecs = 0;
			int totrecs = 0;

			//PAI mode, just a list of printed absolute record numbers
			if (pai_option) {
				std::string line = tapei->ReadTextLine();
				while (line.length()) {
					totrecs++;
					PrintEyeballInvListRec(util::StringToInt(line), prtrecs, eyeball, eyedest);
					line = tapei->ReadTextLine();
				}
			}
			
			//Compressed mode: First thing is the number of segment inverted lists to come
			else {
				unsigned short nsegs = tapei->ReadBinaryUint16();

				//URN
				if (nsegs == 0) {
					totrecs++;
					PrintEyeballInvListRec(tapei->ReadBinaryInt32(), prtrecs, eyeball, eyedest);
				}

				//Inverted lists
				else {
					
					//Init these here.  They're reread at bottom of each loop to test for terminator
					short segnum = tapei->ReadBinaryInt16();
					unsigned short nrecs = tapei->ReadBinaryUint16();

					//Process each seg list
					while (nsegs--) {

						//Bitmap form
						if (nrecs == 0xFFFF) {
							util::BitMap bitmap(DBPAGE_BITMAP_SIZE);
							tapei->ReadRawData(bitmap.Data(), DBPAGE_BITMAP_BYTES);

							bitmap.InvalidateCachedCount();
							totrecs += bitmap.Count();

							for (unsigned int bit = 0; bit < (unsigned int)DBPAGE_BITMAP_SIZE; bit++) {
								if (!bitmap.FindNext(bit, bit))
									break;

								int listrec = AbsRecNumFromRelRecNum(bit, segnum);
								if (!PrintEyeballInvListRec(listrec, prtrecs, eyeball, eyedest))
									break;
							}
						}
						//Array form
						else {
							unsigned short* arraybuff = new unsigned short[nrecs];

							try {
								tapei->ReadBinaryUint16Array(arraybuff, nrecs);
								totrecs += nrecs;

								for (int entry = 0; entry < nrecs; entry++) {
									unsigned short relrec = arraybuff[entry];
								
									int listrec = AbsRecNumFromRelRecNum(relrec, segnum);
									if (!PrintEyeballInvListRec(listrec, prtrecs, eyeball, eyedest))
										break;
								}
							}
							catch (...) {}
							delete[] arraybuff;
						}

						//Optional seglist separator
						if (crlf_option)
							tapei->ReadCRLF();

						//Optional FFFFFFFF value terminator might occur after any seg list.
						//Read it in 2 chunks to get correct endianizing of 2 vars for the next loop.
						segnum = tapei->ReadBinaryInt16(&eof);
						if (eof)
							break;
						nrecs = tapei->ReadBinaryUint16();
						if (segnum == -1 && nrecs == 0xFFFF)
							break;

					} //each seglist
				} // URN or seglist
			} //Not PAI input

			if (prtrecs != totrecs) {
				eyedest->Write(" ... (");
				eyedest->Write(util::IntToString(totrecs));
				eyedest->Write(" total)");
			}

			eyedest->WriteLine("");
			nvals++;

		} //each value

		if (!eof) {
			eyedest->Write("  ... (");
			RoundedDouble pct = tapei->FilePosPercent();
			eyedest->Write(pct.ToStringWithFixedDP(2));
			eyedest->WriteLine("% of file processed)");
		}

		tapei->CloseFile();

	} //each tape
}








//*************************************************************************************
//Atom manager objects.  These basically just call the above, usually for the forward
//udpates, but if TBO is invoked they call the appropriate atomic functions to do 
//the reverse updates too.
//*************************************************************************************

//*************************************************************************************
//Add record to value entry
//*************************************************************************************
AtomicUpdate_AddIndexValRec::AtomicUpdate_AddIndexValRec
(Record* r, PhysicalFieldInfo* fi, const FieldValue& v, bool fr) 
: AtomicUpdate(r->HomeFileContext()), record(r), fieldinfo(fi), 
	fieldval(v), file_registered(fr)
{}

//*************************************************************************************
AtomicBackout* AtomicUpdate_AddIndexValRec::CreateCompensatingUpdate()
{
	backout = new AtomicBackout_DeAddIndexValRec(record, fieldinfo, fieldval);
	return backout;
}

//*************************************************************************************
bool AtomicUpdate_AddIndexValRec::Perform()
{
	RegisterAtomicUpdate(file_registered);

	bool valrec_added = context->GetDBFile()->GetIndexMgr()->Atom_AddValRec
		(fieldinfo, fieldval, record->HomeFileContext(), record->RecNum());

	if (backout && valrec_added)
		backout->Activate();

	return valrec_added;
}

//*************************************************************************************
AtomicBackout_DeAddIndexValRec::AtomicBackout_DeAddIndexValRec
(Record* r, PhysicalFieldInfo* fi, const FieldValue& v)
: AtomicBackout(r->HomeFileContext()), recnum(r->RecNum()), fieldinfo(fi), fieldval(v)
{}

//*************************************************************************************
void AtomicBackout_DeAddIndexValRec::Perform()
{
	//Unlike the table B functions there is no need to create a record MRO here
	//since the unlocked record scanner issue is not relevant to the index.
	bool removed_again = context->GetDBFile()->GetIndexMgr()->Atom_RemoveValRec
		(fieldinfo, fieldval, context, recnum);

	//This would not necessarily be an error, but definitely a cause for concern
	if (!removed_again) {
		context->DBAPI()->Core()->GetRouter()->Issue(TXN_BACKOUT_INFO, std::string
			("Backout warning: index value was removed since addition in forward update: ")
			.append(fieldval.ExtractString()));
	}
}




//*************************************************************************************
//Remove record from value entry
//*************************************************************************************
AtomicUpdate_RemoveIndexValRec::AtomicUpdate_RemoveIndexValRec
(Record* r, PhysicalFieldInfo* fi, const FieldValue& v) 
: AtomicUpdate(r->HomeFileContext()), record(r), fieldinfo(fi), fieldval(v)
{}

//*************************************************************************************
AtomicBackout* AtomicUpdate_RemoveIndexValRec::CreateCompensatingUpdate()
{
	backout = new AtomicBackout_DeRemoveIndexValRec(record, fieldinfo, fieldval);
	return backout;
}

//*************************************************************************************
bool AtomicUpdate_RemoveIndexValRec::Perform()
{
	RegisterAtomicUpdate();

	bool valrec_removed = context->GetDBFile()->GetIndexMgr()->Atom_RemoveValRec
		(fieldinfo, fieldval, record->HomeFileContext(), record->RecNum());

	if (backout && valrec_removed)
		backout->Activate();

	return valrec_removed;
}

//*************************************************************************************
AtomicBackout_DeRemoveIndexValRec::AtomicBackout_DeRemoveIndexValRec
(Record* r, PhysicalFieldInfo* fi, const FieldValue& v)
: AtomicBackout(r->HomeFileContext()), recnum(r->RecNum()), fieldinfo(fi), fieldval(v)
{}

//*************************************************************************************
void AtomicBackout_DeRemoveIndexValRec::Perform()
{
	//See comment in DeAdd
	bool added_back = context->GetDBFile()->GetIndexMgr()->Atom_AddValRec
		(fieldinfo, fieldval, context, recnum);

	//This would not necessarily be an error, but definitely a cause for concern
	//(perhaps not as much as when backing out an add however).
	if (!added_back) {
		context->DBAPI()->Core()->GetRouter()->Issue(TXN_BACKOUT_INFO, std::string
			("Backout warning: index value was reinserted since removal in forward update: ")
			.append(fieldval.ExtractString()));
	}
}


//*************************************************************************************
//File records
//*************************************************************************************
AtomicUpdate_FileRecords::AtomicUpdate_FileRecords
(SingleDatabaseFileContext* home, BitMappedFileRecordSet* s, 
 PhysicalFieldInfo* fi, const FieldValue& v) 
: AtomicUpdate(home), newset(s), fieldinfo(fi), fieldval(v)
{}

//*************************************************************************************
AtomicBackout* AtomicUpdate_FileRecords::CreateCompensatingUpdate()
{
	backout = new AtomicBackout_UnFileRecords(context, fieldinfo, fieldval);
	return backout;
}

//*************************************************************************************
void AtomicUpdate_FileRecords::Perform()
{
	RegisterAtomicUpdate();

	//Avoid building old set if not required
	BitMappedFileRecordSet* oldset = NULL;
	BitMappedFileRecordSet** ppoldset = (tbo_is_off) ? NULL : &oldset;

	bool something_done = context->GetDBFile()->GetIndexMgr()->
		Atom_ReplaceValRecSet(fieldinfo, fieldval, context, newset, ppoldset);

	//Nothing to back out if input set was empty and index doesn't have the value
	if (backout && something_done) {
		//If it was a new value the old set may be empty.  This is a rare case within
		//the system where a BMFRS object exists but is empty.

		static_cast<AtomicBackout_UnFileRecords*>(backout)->NoteOldSet(oldset);
		backout->Activate();
	}
}


//*************************************************************************************
AtomicBackout_UnFileRecords::AtomicBackout_UnFileRecords
(SingleDatabaseFileContext* home, PhysicalFieldInfo* pf, const FieldValue& v)
: AtomicBackout(home), oldset(NULL), fieldinfo(pf), fieldval(v)
{}

//*************************************************************************************
AtomicBackout_UnFileRecords::~AtomicBackout_UnFileRecords()
{
	if (oldset)
		delete oldset;
}

//*************************************************************************************
void AtomicBackout_UnFileRecords::Perform()
{
	context->GetDBFile()->GetIndexMgr()->
		Atom_ReplaceValRecSet(fieldinfo, fieldval, context, oldset, NULL);
}













//*************************************************************************************
//See dbf_find for the main bulk of this.
//*************************************************************************************
void DatabaseFileIndexManager::FindRecords
(int groupix, SingleDatabaseFileContext* sfc, FoundSet* set,  const FindSpecification& spec, 
 const FindEnqueueType& locktype, const BitMappedFileRecordSet* baseset)
{
	file->CheckFileStatus(false, true, true, false);

	//Create a context-level object to perform the find.
	FindOperation fo(groupix, sfc, set, spec, locktype, baseset, file);
	fo.Perform();
}





} //close namespace


