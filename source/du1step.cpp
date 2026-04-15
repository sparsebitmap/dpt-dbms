
#include "stdafx.h"

#include "du1step.h"
#include "windows.h" //V2.14 for MEMORYSTATUS

//Utils
#include "dataconv.h"
#include "winutil.h"
#include "bbstdio.h"
#include "bbarray.h"
//API Tiers
#include "statview.h"
#include "msgroute.h"
#include "core.h"
#include "cfr.h"
#include "loaddiag.h"
#include "dbf_field.h"
#include "dbf_index.h"
#include "fastload.h"
#include "dbfile.h"
#include "dbctxt.h"
#include "seqserv.h"
#include "dbserv.h"
#include "frecset.h"
#include "bmset.h"
#include "btree.h"
//Diagnostics
#include "except.h"
#include "msg_db.h"

namespace dpt {

const short DU1SegInvList::NONE = 0;
const short DU1SegInvList::UNIQUE1 = 1;
const short DU1SegInvList::UNIQUE2 = 2;
const short DU1SegInvList::ARRAY = 3;
const short DU1SegInvList::BITMAP = 4;

//*************************************************************************************
//Memory availablility is checked every so often.  The cost of the Windows calls is 
//quite high (hundreds x e.g. strcmp or double*double, and even more if the 
//VirtualQuery loop is used - e.g. on Wine ATOW) so use a large interval, but not so 
//large that the FV pairs added in between checks would greatly impact memeory usage.
static const int MEMORY_CHECK_FVP_COUNT = 10000;

//*************************************************************************************
//V2.15 In early versions of this code the deletion of cached DU info after processing
//end-of-chunk did not return much, or any, memory to the OS, at least as reported by
//GlobalMemoryStatus().  I now believe that was due to heap fragmentation caused by all 
//the huge arrays and bitmaps being put on the default heap.  Windows heaps (pre-vista) 
//are known for not being good with a lot of large objects.  Hence use of private heaps 
//here. These mean we can force heap memory to be decommitted at chunk end, plus cut
//down fragmentation of the main heap used by the small objects in the STL - maps/strings
//etc.  The arrays and bitmaps are the large objects causing all the trouble. 
//V2.23.  Had not coded for sharing between multiple files in DU mode (duh).  Options:
//a. Share the local heaps and clear all files down whenever memory fills;
//b. Local heaps per file and accept that a file might sit hogging memory.
//Option b is easier to retrofit and I think more intuitive behaviour for users.  The 
//problem of one quiet file hogging mem is unlikely, and user-manageable anyway.
//*************************************************************************************
//HANDLE h_arrayheap = NULL;
//HANDLE h_bitmapheap = NULL;
//HANDLE* arrayheap = NULL;
//HANDLE* bitmapheap = NULL;

void DeferredUpdate1StepInfo::CreateLocalHeaps()
{
	DestroyLocalHeaps(AHeap(), BHeap());

	//V2.16 Start with 1Mb each should help reduce VM fragmentation.
	static const int initsz = 1 << 20; 

	//V3.0.  Parts might be multithreaded now, and these heaps are implicated.  In
	//addition the NO_SERIALIZE flag reportedly forces Vista+ to fall back to
	//pre-vista unoptimized code and hence is not ideal anyway.
//	h_arrayheap = HeapCreate(HEAP_NO_SERIALIZE | HEAP_GENERATE_EXCEPTIONS, initsz, 0);
//	h_bitmapheap = HeapCreate(HEAP_NO_SERIALIZE | HEAP_GENERATE_EXCEPTIONS, initsz, 0);
	h_arrayheap = HeapCreate(HEAP_GENERATE_EXCEPTIONS, initsz, 0);
	h_bitmapheap = HeapCreate(HEAP_GENERATE_EXCEPTIONS, initsz, 0);
}

void DeferredUpdate1StepInfo::DestroyLocalHeaps(HANDLE* aheap, HANDLE* bheap)
{
	if (*aheap != NULL) {
		HeapDestroy(*aheap);
		*aheap = NULL;
	}

	if (*bheap) {
		HeapDestroy(*bheap);
		*bheap = NULL;
	}
}

//*************************************************************************************
void DU1FlushStats::PrepMsg(std::string& msg, int numvals)
{
	msg.append(" Values:").append(util::IntToString(numvals));
	msg.append(", Uniq:").append(util::IntToString(unique_f));
	msg.append(", Seg lists:").append(util::IntToString(seglists));
	msg.append(", (unq1:").append(util::IntToString(unique_s1));
	msg.append(", unq2:").append(util::IntToString(unique_s2));
	msg.append(", list:").append(util::IntToString(arrays));
	msg.append(", bmap:").append(util::IntToString(bmaps));
	msg.append(")");
}

//*************************************************************************************
void DU1SegInvList::Clear(DeferredUpdate1StepInfo* du1)
{
	if (rectype == ARRAY) {
		ILArray ila(data.array, du1->AHeap());
		ila.DestroyData();
	}
	else if (rectype == BITMAP) {
		delete data.bmap;
	}
	
	rectype = NONE;
}

//************************************
void DU1SegInvList::AddRecord(unsigned short relrec, DeferredUpdate1StepInfo* du1) 
{
	//Special cases for unique and semi-unique record numbers
	if (rectype == UNIQUE1) {

		//Dupe F=V on the same rec is possible, if uncommon.  See also array comment below.
		if (relrec == data.recnums.rn1)
			return;

		data.recnums.rn2 = relrec;

		rectype = UNIQUE2;
		return;
	}

	//2 existing record numbers - expand out to array format
	if (rectype == UNIQUE2) {
		unsigned short rn1 = data.recnums.rn1;
		unsigned short rn2 = data.recnums.rn2;

		//ILArray ila(NULL, arrayheap); //V2.23
		ILArray ila(NULL, du1->AHeap()); 

		data.array = ila.InitializeAndReserve();

		data.array = ila.AppendEntry(rn1);
		data.array = ila.AppendEntry(rn2);
		data.array = ila.AppendEntry(relrec);

		rectype = ARRAY;
		return;
	}

	//Already array format.  See above comments re. the uncommon case of dupe F=V on the
	//same record.  To check the array here at insertion time would be slow, and a waste
	//of time in nearly all cases.  We do a check before writing to the file later on.
	if (rectype == ARRAY) {

		//ILArray ila(data.array, arrayheap); //V2.23
		ILArray ila(data.array, du1->AHeap());

		if (ila.NumEntries() < DLIST_MAXRECS) {
			data.array = ila.AppendEntry(relrec);
			return;
		}

		//Convert up to bitmap format at the appropriate point
		//util::BitMap* temp = new util::BitMap(DBPAGE_BITMAP_SIZE, bitmapheap); //V2.23
		util::BitMap* temp = new util::BitMap(DBPAGE_BITMAP_SIZE, du1->BHeap());

		for (int x = ila.NumEntries() - 1; x >= 0; x--)
			temp->Set(ila.GetEntry(x));

		ila.DestroyData();
		data.bmap = temp;

		rectype = BITMAP;
	}

	//Bitmap format.  No dupe issue here, it's an intrinsically deduped structure.
	data.bmap->Set(relrec);
}

//************************************
void DU1SegInvList::FlushData
(BitMappedFileRecordSet* fvalset, DU1SeqOutputFile* seq_file, 
 DU1FlushStats* stats, DeferredUpdate1StepInfo* du1) 
{
	//Single record in segment
	if (rectype == UNIQUE1) {
		if (fvalset) {
			fvalset->AppendSegmentSet(
				new SegmentRecordSet_SingleRecord(segnum, data.recnums.rn1));
		}
		else {
			seq_file->WriteSegInvList(segnum, 1, &(data.recnums.rn1));
		}

		if (stats) stats->unique_s1++;
		return;
	}

	//More than one record but not a bitmap
	if (rectype != BITMAP) {

		//----------------------------------
		//Semi-unique
		if (rectype == UNIQUE2) {

			//There is no semi-unique segset type - array segset covers it
			if (fvalset) {
				SegmentRecordSet_Array* a = new SegmentRecordSet_Array(segnum, 2);
				a->Array().AppendEntry(data.recnums.rn1);
				a->Array().AppendEntry(data.recnums.rn2);
				fvalset->AppendSegmentSet(a);
			}
			else {
				seq_file->WriteSegInvList(segnum, 2, &(data.recnums));
			}

			if (stats) stats->unique_s2++;
			return;
		}

		//----------------------------------
		//Array format
		//ILArray ila(data.array, arrayheap); //V2.23
		ILArray ila(data.array, du1->AHeap());

		//See Add() comments about checking for the unlikely dupe F=V on the same record.
		//Do it now on extracting the array.  Only dedupe in unlikely cases.  Most often
		//the data has been loaded in ascending record number order, and all is OK.
		unsigned short prevrec = USHRT_MAX;
		int numentries = ila.NumEntries();
		for (int x = 0; x < numentries; x++) {

			unsigned short rec = ila.GetEntry(x);
			if (rec > prevrec) {
				prevrec = rec;
				continue;
			}

			//Record #s not in strict order, so accept overhead of dedupe.  I'd guess
			//there's not much to choose between dedupe methods here. std::set is simple.
			std::set<unsigned short> deduped;
			for (int y = 0; y < numentries; y++)
				deduped.insert(ila.GetEntry(y));

			ila.SetCount(0);

			//(This also puts the array in rec# order, although that's not essential)
			std::set<unsigned short>::iterator i;
			for (i = deduped.begin(); i != deduped.end(); i++)
				ila.AppendEntry(*i);

			break;
		}

		//Alternate destinations: final flush or temp seq file
		if (fvalset) {
			//fvalset->AppendSegmentSet(new SegmentRecordSet_Array(segnum, data.array, arrayheap)); //V2.23
			fvalset->AppendSegmentSet(new SegmentRecordSet_Array(segnum, data.array, du1->AHeap()));

			//The record set object is the owner of the array now
			rectype = NONE;
		}
		else {
			seq_file->WriteSegInvList(segnum, ila.NumEntries(), ila.Data());
			Clear(du1);
		}

		if (stats) stats->arrays++;
		return;
	}

	//Finally bitmap format
	if (fvalset) {
		fvalset->AppendSegmentSet(new SegmentRecordSet_BitMap(segnum, data.bmap));

		//Again here the set becomes the owner of the data now
		rectype = NONE;
	}
	else {
		seq_file->WriteSegInvList(segnum, USHRT_MAX, data.bmap->Data());
		Clear(du1);
	}

	if (stats) stats->bmaps++;
}








//**************************************************************************************
void DU1InvList::AddRecord(int recnum, DeferredUpdate1StepInfo* du1) 
{
	//Special packing for unique values saves various space and path overheads
	if (allocsegs == 0) {

		//First record for this value
		if (info.unique_recnum == -1) {
			info.unique_recnum = recnum;
			return;
		}

		//Dupe F=V on the same record is possible, if uncommon
		if (recnum == info.unique_recnum)
			return;

		//It's no longer unique - perform the seg array init deferred the previous time
		int no_longer_unique_recnum = info.unique_recnum;

		info.seginfos = new DU1SegInvList[1];
		allocsegs = 1;

		AddRecord(no_longer_unique_recnum, du1);
	}

	//Now the regular processing for non-special case
	short segnum = SegNumFromAbsRecNum(recnum);
	unsigned short relrec = RelRecNumFromAbsRecNum(recnum, segnum);

	//Typically data is loaded in record number order, which means the last-added
	//segment will be the one of interest.  So scan down usually hits first time.
	for (int x = numsegs - 1; x >= 0; x--) {
		DU1SegInvList& seglist = SegList(x);
		if (seglist.SegNum() == segnum) {
			seglist.AddRecord(relrec, du1);
			return;
		}
	}

	//No entries for segment yet, so make new segment list and put record on it.
	//Extend array if needed by doubling its allocation, like std::<vector> does.
	if (numsegs == allocsegs) {

		int new_alloc = allocsegs * 2;
		DU1SegInvList* temp = new DU1SegInvList[new_alloc];

		//Copy in objects from the old array, and swap pointers
		for (int x = 0; x < allocsegs; x++)
			temp[x] = info.seginfos[x];

		DestroySegInfo(du1);
		info.seginfos = temp;

		allocsegs = new_alloc;
	}

	numsegs++;
	InfoBack().Initialize(segnum, relrec);
}

//************************************
void DU1InvList::FlushData
(BitMappedFileRecordSet* fvalset, DU1SeqOutputFile* seq_file, 
 DU1FlushStats* stats, DeferredUpdate1StepInfo* du1) 
{
	//Unique record case
	if (numsegs == 0) {

		if (fvalset) {
			short segnum = SegNumFromAbsRecNum(info.unique_recnum);
			unsigned short relrec = RelRecNumFromAbsRecNum(info.unique_recnum, segnum);
			fvalset->AppendSegmentSet(new SegmentRecordSet_SingleRecord(segnum, relrec));
		}
		else {
			seq_file->WriteURN(URN()); 
		}

		if (stats) stats->unique_f++;
		return;
	}

	if (seq_file)
		seq_file->WriteValueHeader(false);

	//ILMR case
	for (int x = 0; x < numsegs; x++)
		SegList(x).FlushData(fvalset, seq_file, stats, du1);

	if (stats) stats->seglists += numsegs;
}







//**************************************************************************************
//**************************************************************************************
//Since these objects live in a map a copy constructor is required for map expansions
DU1FieldIndex::DU1FieldIndex(const DU1FieldIndex& i) 
: pfi(NULL), fastload_tapei(NULL), numinfo(i.numinfo), strinfo(i.strinfo)
{
	Initialize(i.pfi, i.diags, i.du1);

	//The donor object no longer has responsibility for the data
	i.numinfo = NULL;
	i.strinfo = NULL;
}

//************************************
void DU1FieldIndex::Initialize
(PhysicalFieldInfo* p, LoadDiagnostics* d, DeferredUpdate1StepInfo* dd)
{
	if (!p)
		return;

	//Telling these two objects about each other saves various lookups
	pfi = p;
	void** pcache = &(pfi->extra);
	DU1FieldIndex** pfix = reinterpret_cast<DU1FieldIndex**>(pcache);
	*pfix = this;

	//Create empty map of the appropriate type
	if (!strinfo && !numinfo) {
		if (pfi->atts.IsOrdNum())
			numinfo = new std::map<RoundedDouble, DU1InvList*>;
		else
			strinfo = new std::map<std::string, DU1InvList*>;
	}

	diags = d;
	du1 = dd;
}

//************************************
void DU1FieldIndex::Clear()
{
	//Clear down maps
	if (numinfo) {
		std::map<RoundedDouble, DU1InvList*>::iterator i;
		for (i = numinfo->begin(); i != numinfo->end(); i++) {
			if (i->second)
				DU1InvList::DestroyObject(i->second, du1);
		}

		//At one point I suspected the STL map allocator of not giving all its memory
		//back on clear(), so force delete/recreate here.  In the end I think it was down
		//to heap fragmentation (see comments at top) but still delete here just to give
		//<map> as little chance as possible to hoard anything.
		delete numinfo;
		numinfo = new std::map<RoundedDouble, DU1InvList*>;
		//numinfo->clear();
	}
	if (strinfo) {
		std::map<std::string, DU1InvList*>::iterator i;
		for (i = strinfo->begin(); i != strinfo->end(); i++) {
			if (i->second)
				DU1InvList::DestroyObject(i->second, du1);
		}

		//As above
		delete strinfo;
		strinfo = new std::map<std::string, DU1InvList*>;
		//strinfo->clear();
	}

	current_seq_file.Close();
}

//************************************
void DU1FieldIndex::CloseMergeInputStreams()
{
	//V2.17 Moved this here from Destroy() (file handles were left open too long)
	for (size_t x = 0; x < merge_streams.size(); x++) {
		if (merge_streams[x])
			delete merge_streams[x];
	}
	merge_streams.clear();
}

//************************************
void DU1FieldIndex::DeleteMergeSequentialFiles(int howmany)
{
	//V2.17 After a pre-merge we only want to delete the first few
	if (howmany == -1)
		howmany = seq_file_names.size();

	for (int y = 0; y < howmany; y++) {
		try {
			util::StdioDeleteFile(seq_file_names[y].c_str());
		}
		catch (...) {}
	}
//	seq_file_names.erase(seq_file_names.begin(), &(seq_file_names[howmany])); V2.17.1 (gcc)
	seq_file_names.erase(seq_file_names.begin(), seq_file_names.begin()+howmany);
}

//************************************
void DU1FieldIndex::Destroy()
{
	Clear(); 
	CloseMergeInputStreams();
	DeleteMergeSequentialFiles();

	if (numinfo) 
		delete numinfo; 
	if (strinfo) 
		delete strinfo;

	//V2.15.  In case of multiple loads without first freeing file
	if (pfi)
		pfi->extra = NULL;
}

//************************************
DU1InvList* DU1FieldIndex::FindOrInsertInvList(const FieldValue& val) 
{
	//Rather inelegant but avoids the FieldValue copy and compare functions which 
	//would be required to do the search.  They're not bad but much worse than intrinsic
	//double, and also than std::string in many cases.  So hold 2 different maps.
	//NB.  RoundedDouble has no extra overhead compared to intrinsic double for the
	//operations we're using, since all incoming values are already rounded.  And it's
	//less work in the end to avoid dropping down to double and back later.
	if (numinfo) {

		//V3.0.  During normal use, the value is already known to be in the right format,
		//but during fast load when building indexes from TAPED data, it *MUST* convert.
//		RoundedDouble rd = val.ExtractRoundedDouble();
		RoundedDouble rd = val.ExtractRoundedDouble(true);

		//Assuming random value order coming in, so no find cache here.  Leave it to <map>.
		std::pair<std::map<RoundedDouble, DU1InvList*>::iterator, bool> ins;
		ins = numinfo->insert(std::make_pair<RoundedDouble, DU1InvList*>(rd, NULL));

		//Trial pair was inserted - make a new inverted list
		if (ins.second)
			ins.first->second = new DU1InvList();

		return ins.first->second;
	}
	else {

//		std::string s = val.ExtractString();

		std::pair<std::map<std::string, DU1InvList*>::iterator, bool> ins;
		ins = strinfo->insert(std::make_pair<std::string, DU1InvList*>(val.ExtractString(), NULL));

		if (ins.second)
			ins.first->second = new DU1InvList();

		return ins.first->second;
	}
}

//************************************
//Code shared by the flush to the database file or to a temporary sequential file
std::string DU1FieldIndex::FlushData
(SingleDatabaseFileContext* context, DatabaseFileIndexManager* indexmgr, bool finalizing)
{
	std::string msg = std::string("Field '").append(pfi->name).append("'");

	//NM fields never get temp files written
	if (IsNoMerge())
		msg.append(" [To table D]: ");

	//Regular fields do...
	else {

		//So open a fresh file (no need if it's the last chunk though)
		if (!finalizing) {
			msg.append(" [To #SEQTEMP]: ");
			OpenNewCurrentSeqFile();
		}

		//If it *is* the last chunk, quit and get on with the merge if there is one to do
		else if (seq_file_names.size() != 0) {
			msg.append(" [Last chunk input to merge]");
			return msg;
		}

		//No merge required - just write complete table D straight from memory
		else {
			msg.append(" [No merge reqd]: ");
		}
	}

	DU1FlushStats stats;

	//Cache b-tree position outside append loop to save continual repositioning
	BTreeAPI_Load bt(&stats);
	bt.Initialize(context, pfi);

	for (BeginIterator(); !IteratorEnded(); AdvanceIterator()) {
		DU1InvList* invlist = ListAtIterator();

#ifdef _DEBUG
		std::string sfld = pfi->name;
		std::string sval = ValueAtIterator().ExtractString();
#endif

		//To sequential file for later merge.
		if (current_seq_file.IsOpen()) {

			if (numinfo)
				current_seq_file.SetFieldValue(ValAtNumIter());
			else
				current_seq_file.SetFieldValue(ValAtStrIter());

			current_seq_file.SetNumSegs(invlist->NumSegs());

			invlist->FlushData(NULL, &current_seq_file, &stats, du1);
		}

		//Build and write value set to table D
		else {
			BitMappedFileRecordSet fvalset(context);

			//V2.19.  Get the correct array/bitmap breakdown here rather than provisional
//			invlist->FlushData(&fvalset, NULL, stats);
//			invlist->FlushData(&fvalset, NULL, NULL);
			invlist->FlushData(&fvalset, NULL, NULL, du1); //V2.23. See comment at top.

			indexmgr->Atom_AugmentValRecSet(pfi, ValueAtIterator(), context, &fvalset, &bt);
		}
	}

	stats.PrepMsg(msg, NumValues());

	Clear();
	return msg;
}

//************************************
void DU1FieldIndex::OpenNewCurrentSeqFile(int suffix)
{
	if (suffix == -1)
		suffix = seq_file_names.size();

	//The SEQTEMP directory is a convenient place to store these files, but
	//let's not go via the SequentialFileView class.  We don't need all its
	//frills and we can read/write faster by opening stdio files directly.
	std::string filename = SequentialFileServices::SeqTempDir();
	filename.append("\\DU_");
	filename.append(du1->DD()).append("_"); //V2.23 cosmetics.  Nicer if 2 files have same fld.
	std::string fld = pfi->name;
	util::UnBlank(fld, true);
	filename.append(fld).append("_");

	//V2.17 We know what the name will probably be.  Saves directory scanning.
	filename.append(util::IntToString(suffix)).append(".TMP");

	try {
		current_seq_file.Open(filename.c_str(), util::STDIO_NEW);
	}
	//Exists, so now scan directory for the next similar usable name
	catch (...) {
		filename = win::GetUnusedFileName(filename);
		current_seq_file.Open(filename.c_str(), util::STDIO_NEW);
	}

	seq_file_names.push_back(filename);
}

//************************************
std::string DU1FieldIndex::Merge
(SingleDatabaseFileContext* context, DatabaseFileIndexManager* indexmgr)
{
	std::string msg = std::string("Field '").append(pfi->name).append("': ");
	int numchunks = NumSeqFiles() + ((NumValues() != 0) ? 1 : 0);
	if (numchunks != 0) //V3.0
		msg.append("Chunks: ").append(util::IntToString(numchunks)).append(", ");

	//Initial merge passes if required (very large numbers of files)
	PreMerge(context);

	//Final pass. Prepare sequential files...
	int numfiles = NumSeqFiles(); 
	merge_streams.reserve(numfiles+1);
	bool onm = pfi->atts.IsOrdNum();

	for (int x = 0; x < numfiles; x++) {
		merge_streams.push_back(new DU1MergeStream_File(seq_file_names[x].c_str(), onm));
	}

	//...plus the last chunk still held in memory if there is one
	DU1MergeStream_Mem* final_mem_chunk = NULL;
	if (NumValues() != 0) {
		final_mem_chunk = new DU1MergeStream_Mem(this, onm);
		merge_streams.push_back(final_mem_chunk);
	}

	//V3.0.  Or the fastload tape
	if (fastload_tapei) {
		assert (merge_streams.size() == 0);
		merge_streams.push_back(new DU1MergeStream_FastLoadTape(fastload_tapei, onm));
	}

	util::Merger merger(merge_streams);

	//The output sets from the merge will be built into this set
	BitMappedFileRecordSet fvalset(context);

	int distinctvals = 0;
	DU1FlushStats stats;

	//Cache b-tree position outside append loop to save continual repositioning
	BTreeAPI_Load bt(&stats);
	bt.Initialize(context, pfi);

	//OK, let's go - read successively higher values from the merging streams
	//(there will always be a first value)
	FieldValue currval = 
		static_cast<DU1MergeStream*>(merger.CurrentLoStream())->MakeKeyFieldValue();

	for (;;) {

		//All streams exhausted
		DU1MergeStream* lostream = static_cast<DU1MergeStream*>(merger.CurrentLoStream());
		if (!lostream)
			break;

#ifdef _DEBUG
		std::string sval = lostream->MakeKeyFieldValue().ExtractString();
#endif

		//Got a new value - commit the amalgamated set for the previous value
		if (lostream->DifferentKeyTo(currval)) {
			distinctvals++;

			indexmgr->Atom_AugmentValRecSet(pfi, currval, context, &fvalset, &bt);
			fvalset.ClearButNoDelete();

			currval = lostream->MakeKeyFieldValue();
		}

		//Amalgamate sets where the same value appears in different streams
		lostream->MergeChunkSet(&fvalset);

		merger.ReadNextLoKey();
	}

	//Last remaining value set after the loop
	distinctvals++;
	indexmgr->Atom_AugmentValRecSet(pfi, currval, context, &fvalset, &bt);

	msg.append("Distinct");
	stats.PrepMsg(msg, distinctvals);

	//For interest, the distribution stats for the last chunk in memory
	if (final_mem_chunk)
		merge_final_chunk_stats_msg = final_mem_chunk->FinalMsg();

	CloseMergeInputStreams();
	DeleteMergeSequentialFiles();

	return msg;
}


//************************************
//V2.17.  This function ensures that the number of files opened concurrently by the merge 
//does not become excessive and cause OS file handle issues.
void DU1FieldIndex::PreMerge(SingleDatabaseFileContext* context)
{
	int mmfh = DatabaseServices::GetParmLOADMMFH();

	//Perform merges on groups of files till there are few enough to do the final pass.
	int new_file_suffix = NumSeqFiles();
	while (NumSeqFiles() > mmfh) {

		//Open the first batch of files 
		merge_streams.reserve(mmfh);
		for (int x = 0; x < mmfh; x++) {
			merge_streams.push_back(
				new DU1MergeStream_File(seq_file_names[x].c_str(), pfi->atts.IsOrdNum()));
		}
		util::Merger merger(merge_streams);

		//This code is similar to the final pass (see above func), except we collect
		//no stats info, and the results go not to table D but another seq file.
		OpenNewCurrentSeqFile(new_file_suffix++);

		//Initialize loop
		FieldValue currval = 
			static_cast<DU1MergeStream*>(merger.CurrentLoStream())->MakeKeyFieldValue();
		BitMappedFileRecordSet fvalset(context);

		//Read successively higher values from the merging streams
		for (;;) {

			//All streams exhausted
			DU1MergeStream* lostream = static_cast<DU1MergeStream*>(merger.CurrentLoStream());
			if (!lostream)
				break;

#ifdef _DEBUG
			std::string sval = lostream->MakeKeyFieldValue().ExtractString();
#endif

			//Got a new value - write out the amalgamated set for the previous value
			if (lostream->DifferentKeyTo(currval)) {
				current_seq_file.SetFieldValue(&currval);
				fvalset.Unload(&current_seq_file);
				
				fvalset.ClearButNoDelete();
				currval = lostream->MakeKeyFieldValue();
			}

			//Amalgamate sets for the same value
			lostream->MergeChunkSet(&fvalset);

			merger.ReadNextLoKey();
		}

		//Last remaining value set after the loop
		current_seq_file.SetFieldValue(&currval);
		fvalset.Unload(&current_seq_file);

		current_seq_file.Close();

		//Delete the batch of files we just merged into one
		CloseMergeInputStreams();
		DeleteMergeSequentialFiles(mmfh);

		std::string msg("Pre-merge: ");
		msg.append(util::IntToString(mmfh)).append(" files merged into #SEQTEMP\\..._");
		msg.append(util::IntToString(new_file_suffix-1)).append(", ");
		msg.append(util::IntToString(seq_file_names.size())).append(" remain");

		diags->AuditVerbose(context->DBAPI()->Core(), msg);
	}
}





//**************************************************************************************
//**************************************************************************************
DeferredUpdate1StepInfo::DeferredUpdate1StepInfo
(DatabaseFile* f, const std::string& dd, CriticalFileResource* cfri, int pct, int d) 
: file(f), cfr_index(cfri), memory_hwm(0), partial_flushes(0), chunk_fvpairs(0),
  allchunks_fvpairs(0), callcount(0), total_time_taken(0.0), merge_required(false),
  ddname(dd), h_arrayheap(NULL), h_bitmapheap(NULL)
{
	MEMORYSTATUS ms;
	GlobalMemoryStatus(&ms);
	total_physical_memory = ms.dwTotalPhys;

	//The above function is available on all Windows, but may return unsigned value 
	//mod 2G, which results in a negative number.  Assume 2G if that happens.
	if (total_physical_memory < 0)
		total_physical_memory = INT_MAX;

	max_memory_pct = pct;
	max_memory_size = (total_physical_memory / 100) * max_memory_pct;

	//V2.16 handle incompletely supported GlobalMemoryStatus() on Wine.
//	memory_hwm = ms.dwTotalVirtual - ms.dwAvailVirtual;
	//V2.23 Let's try that again
//	memory_hwm = ms.dwTotalVirtual - win::VirtualMemoryCalcFree();
	loadctl_flags = DatabaseServices::GetParmLOADCTL();
	bool use_gms_function = (loadctl_flags & LOADCTL_MEMQUERY_LONGWAY) ? false : true;
	memory_hwm = ms.dwTotalVirtual - win::VirtualMemoryCalcFree(use_gms_function);

	CreateLocalHeaps();
	diags = new LoadDiagnostics(d);

	fvps_per_chunk = DatabaseServices::GetParmLOADFVPC();
}

//************************************
//This is a separate function because the FCT is locked when the object is created.
void DeferredUpdate1StepInfo::Initialize(SingleDatabaseFileContext* c)
{
	//Assume all fields will be used. Saves a decent amount of lookup work below.
	info.resize(file->GetFieldMgr()->HighestFieldID(c) + 1);
}

//************************************
//V2.19 June 2009.  Used when redefining a field
void DeferredUpdate1StepInfo::InitializeForSingleField(PhysicalFieldInfo* p)
{
	info.resize(1);
	info[0].Initialize(p, diags, this);
}

//************************************
DeferredUpdate1StepInfo::~DeferredUpdate1StepInfo()
{
	delete diags;
}

//************************************
//V2.23. The member local heaps must remain while objects on them are deleted, so they
//can't be dstroyed in the destructor.
void DeferredUpdate1StepInfo::DestroyObject(DeferredUpdate1StepInfo* o)
{
	HANDLE h_aheap = o->h_arrayheap;
	HANDLE h_bheap = o->h_bitmapheap;

	delete o;

	DestroyLocalHeaps(&h_aheap, &h_bheap);
}

//************************************
bool DeferredUpdate1StepInfo::AddEntry
(SingleDatabaseFileContext* context, PhysicalFieldInfo* pfi, const FieldValue& val, int recnum) 
{
	//See Flush() below for the other side of this and more comments.  We don't want the 
	//overhead of CFR_INDEX here in what should be a really fast function. 
	LockingSentry ls(&data_lock);

	//Since the calling code has already located the field info we can avoid a lookup here.
	void** pcache = &(pfi->extra);
	DU1FieldIndex** pfix = reinterpret_cast<DU1FieldIndex**>(pcache);

	//First entry for field, so load the cache used above with ptr to an empty index.
	if (*pfix == NULL)
		info[pfi->id].Initialize(pfi, diags, this);

	//Add the entry
	(*pfix)->FindOrInsertInvList(val)->AddRecord(recnum, this);

	//V2.17.  We can now force end-of-chunk with a parameter.  Very useful for
	//diagnosing problems on different machines, OSes etc.
//	if (++callcount < MEMORY_CHECK_FVP_COUNT)
//		return false;
	callcount++;
	if (callcount == fvps_per_chunk || callcount >= MEMORY_CHECK_FVP_COUNT) {

		chunk_fvpairs += callcount;
		callcount = 0;

		//Case A: Forced minichunk
		if (chunk_fvpairs == fvps_per_chunk)
			return true;

		//Case B: Contingent on memory usage
		MEMORYSTATUS ms;
		GlobalMemoryStatus(&ms);
		//Note that we may be having page faults, but no attempt to second-guess those here.
		//All storage used should be real RAM on a nice quiet system.
		//V2.16 work around incompletely supported GlobalMemoryStatus() on Wine.
//		int storage_used = ms.dwTotalVirtual - ms.dwAvailVirtual;
		//V2.23 Let's try that again
		//int storage_used = ms.dwTotalVirtual - win::VirtualMemoryCalcFree();
		bool use_gms_function = (loadctl_flags & LOADCTL_MEMQUERY_LONGWAY) ? false : true;
		int storage_used = ms.dwTotalVirtual - win::VirtualMemoryCalcFree(use_gms_function);

		if (storage_used > memory_hwm)
			memory_hwm = storage_used;

		if (storage_used > max_memory_size)
			return true;
	}

	return false;
}

//************************************
void DeferredUpdate1StepInfo::AttachFastLoadTapeI
(SingleDatabaseFileContext* context, PhysicalFieldInfo* pfi, FastLoadInputFile* tapei)
{
	if (!IsInitialized())
		Initialize(context);

	void** pcache = &(pfi->extra);
	DU1FieldIndex** pfix = reinterpret_cast<DU1FieldIndex**>(pcache);
	info[pfi->id].Initialize(pfi, diags, this);

	(*pfix)->AttachFastLoadTapeI(tapei);

	//Close till needed, to minimize file handles open at the same time.
	tapei->CloseFile();
}


//************************************
int DeferredUpdate1StepInfo::Flush(SingleDatabaseFileContext* context, FlushMode flush_mode) 
{
	int rcode = 0;
	DatabaseServices* dbapi = context->DBAPI();
	CoreServices* core = dbapi->Core();
	MsgRouter* router = core->GetRouter();
	StatViewer* stats = core->GetStatViewer();
	DatabaseFileIndexManager* indexmgr = file->GetIndexMgr();

	bool partial = (flush_mode == MEMFULL || flush_mode == USER);
	bool finalizing = (flush_mode == CLOSING || flush_mode == REDEFINE || flush_mode == REORG);
	assert(partial != finalizing);

	//Structure locking:  The temporary structures are not considered part of 'INDEX' and
	//are not protected by CFR_INDEX which just covers the disk-based structures.  Going
	//under the CFR would have made sense but I decided to keep these structures logically
	//separate, and only lock the actual index when making the actual disk updates.  The
	//big advantage of that is that there is no need to take the (slow) CFR lock in 
	//AddEntry() above and we can use this fast low level lock.  The downside is that 
	//the MONITOR CFR display will not show conflicts (although of course most loads
	//are single-user jobs).  Let's take CFR_INDEX here just for display purposes.
	LockingSentry ls(&data_lock);
	CFRSentry cs(context->DBAPI(), cfr_index, BOOL_EXCL);

	std::string prev_sl_activity = stats->CurrentActivity();
	if (flush_mode != REORG)
		diags->StartSLStats(stats);

	//Explicit elapsed time is handy as SL stats are awkward to pick through
	double start_time = win::GetFTimeWithMillisecs();

	std::string msg;
	DU1MergeTask* mergetask = NULL;

	try {
		chunk_fvpairs += callcount;
		allchunks_fvpairs += chunk_fvpairs;
		callcount = 0;

		//Anything chunked up in memory for any fields?
		if (chunk_fvpairs > 0) {
			msg = "Index build: flushing stored information ";
			if (flush_mode == CLOSING)
				msg.append("because last user closed file ");
			else if (flush_mode == MEMFULL)
				msg.append("because memory use exceeded trigger level");
			else if (flush_mode == REDEFINE || flush_mode == REORG)
				msg.append("at end of operation");
			else
				msg.append("at request of user");
			diags->AuditVerbose(core, msg);
			diags->AuditVerboseSep(core);

			partial_flushes++;

			msg = std::string(ddname);
			msg.append(" chunk #").append(util::IntToString(partial_flushes)).append(": ");
			msg.append(util::IntToString(chunk_fvpairs)).append(" F=V pairs, as follows");
			diags->AuditVerbose(core, msg);

			//If we're going to finalize further down don't flush unnecessarily to temp file 
			if (!finalizing) {
				diags->AuditChunk(core, std::string("Index build: flushing ")
					.append(ddname).append(" chunk #")
					.append(util::IntToString(partial_flushes)).append(" ..."));
			}

			//Sanity check for cases where the heap cleanup does not reclaim any memory
			//in which case it will be impossible to track incremental memory usage. 
			if (chunk_fvpairs == MEMORY_CHECK_FVP_COUNT) {
				if (DatabaseServices::GetParmLOADFVPC() != MEMORY_CHECK_FVP_COUNT) {
					diags->AuditForce(core, "Index updates required immediate flush - try more memory (LOADMEMP parameter)");
					throw Exception(DBA_DEFERRED_UPDATE_ERROR, "Cancelling operation");
				}
			}
		}

		//Anything chunked up in memory for each individual field?
		for (size_t fid = 0; fid < info.size(); fid++) {
			DU1FieldIndex& fix = info[fid];

			if (fix.NumValues() != 0) {
				double stime = win::GetFTimeWithMillisecs();

				//Flush it to table D or temp sequential file
				msg = fix.FlushData(context, indexmgr, finalizing);

				double etime = win::GetFTimeWithMillisecs();
				double fld_time = etime - stime;
				
				if (fld_time > 0) {
					msg.append(" in ");
					msg.append(RoundedDouble(fld_time).ToStringWithFixedDP(3)).append("s");
				}
			
				diags->AuditVerbose(core, msg);

				//Make a note to do final merges later if we wrote temp sequential data
				if (!fix.IsNoMerge() && partial)
					merge_required = true;
			}
		}

		//Post-chunk messages
		if (chunk_fvpairs > 0) {
			double end_time = win::GetFTimeWithMillisecs();
			double chunktime = end_time - start_time;
			total_time_taken += chunktime;

			if (chunktime > 0) {
				msg = "Chunk processing time: ";
				msg.append(RoundedDouble(chunktime).ToStringWithFixedDP(3)).append("s");
				diags->AuditVerbose(core, msg);
			}
			diags->AuditVerboseSep(core);

			if (!finalizing)
				diags->AuditChunk(core, std::string("... processed in ")
					.append(RoundedDouble(chunktime).ToStringWithFixedDP(3)).append("s"));
			
			rcode = chunk_fvpairs;
			chunk_fvpairs = 0;
		}

		//----------------------------------------------------------------------------
		//After the last chunk (closing file etc.)
		//----------------------------------------------------------------------------
		if (finalizing) {

			//There will only be anything to do here if there was one or more fields
			//where table D wasn't being built at the end of each chunk.
			//-----
			//V3.0.  
			//-----
			//Also, there will always be a from-file phase when TAPEIs are supplied 
			//externally, which is handled as a "merge" here even though only 1 of them.
			//Note that in that scenario some fields' indexes might have been built via
			//the regular DU1 mechanism too, and we do them all together now here.
			//if (merge_required) {
			if (merge_required || flush_mode == REORG) {

				//Make a fresh SL block if something got done above
				if (rcode != 0 && flush_mode != REORG)
					diags->StartSLStats(stats);
				
				if (flush_mode == REORG)
					diags->AuditChunk(core, "Index build: loading input file(s)");
				if (merge_required) {
					diags->AuditFinal(core, 
						std::string("Index build: performing final chunk merge(s) for file ")
								.append(ddname));
					diags->AuditFinalSep(core);
				}

				//V3.0. Prepare for possible parallel processing
				mergetask = new DU1MergeTask;

				int loadthrd = dbapi->GetParmLOADTHRD();
				if (loadthrd == 1)
					diags->AuditChunk(core, "  Parallel processing is not active");
				else {
					diags->AuditChunk(core, std::string("  Parallel processing is active (")
						.append(util::IntToString(loadthrd)).append(" threads)"));
				}

				//Look through fields for the ones with updates to apply
				for (size_t fid = 0; fid < info.size(); fid++) {
					DU1FieldIndex& fix = info[fid];

					//In deferred update processing if there's only one memory chunk it 
					//would have been written direct to table D in the Flush() phase above.
					if (fix.NumSeqFiles() > 0 || fix.TapeI()) {
						mergetask->AddSubTask(new DU1MergeParallelSubTask
							(context, &fix, flush_mode == REORG));
					}
				}

				//Execute parallel processing.  Even in the common case of just one
				//thread, it still comes through here in the same way.
				while (mergetask->NumReadySubTasks() > 0) {

					//If errors occur, quiesce any other running threads and abort
					if (mergetask->ErrCode()) {
						mergetask->RequestInterruptAllSubTasks();
						break;
					}

					//Spawn new thread if we have a CPU free
					if (mergetask->NumRunningSubTasks() < loadthrd)
						mergetask->SpawnThread();

					win::Cede(true);
				}

				//Wait for the last threads to finish
				while (mergetask->NumRunningSubTasks() > 0)
					win::Cede(true);

				//Rethrow the exception caught before waiting for parallel quiesce
				if (mergetask->ErrCode()) {
					util::WorkerThreadSubTask* errsubtask = mergetask->ErrSubTask();

					static_cast<DU1MergeParallelSubTask*>(errsubtask)->ClaimError();

					throw Exception(mergetask->ErrCode(), std::string(
						mergetask->ErrSubTask()->Name()).append(": ").append(mergetask->ErrMsg()));
				}

				total_time_taken += mergetask->GetTime();
				util::WorkerThreadTask::CleanUp(mergetask);

				diags->AuditFinalSep(core);
			}

			//-------------------------------------
			//In a load/reorg we're usually not interested in prior memory usage
			if (partial_flushes > 0 || merge_required) {
				msg = "Index build final summary";
				if (flush_mode == CLOSING)
					msg.append(": closing file ").append(ddname);

				diags->AuditFinal(core, msg);
				diags->AuditFinalSep(core);

				msg = std::string("Processed in ");
				msg.append(util::IntToString(partial_flushes)).append(" chunk(s)");
				if (merge_required)
					msg.append(" plus merge");
				diags->AuditFinal(core, msg);

				msg = std::string("All chunks F=V pairs: ");
				msg.append(util::IntToString(allchunks_fvpairs));
				diags->AuditFinal(core, msg);

				msg = "All chunks processing time: ";
				msg.append(RoundedDouble(total_time_taken).ToStringWithFixedDP(3)).append("s");
				diags->AuditFinal(core, msg);

				msg = "Memory usage HWM: ";
				msg.append(util::IntToString(memory_hwm/1000000));
				msg.append("Mb (");
				double hwm = memory_hwm; hwm /= total_physical_memory; hwm *= 100;
				msg.append(RoundedDouble(hwm).ToStringWithFixedDP(1)).append("%)");
				diags->AuditFinal(core, msg);

				diags->AuditVerbose(core, "Since-last stat group(s) contain more details");
				diags->AuditFinalSep(core);
			}

			if (flush_mode == REORG)
				diags->AuditChunk(core, "Index build: processing complete for all fields");
			else {
				router->Issue(DBA_DEFERRED_UPDATE_INFO, 
					"Index build: deferred updates finalized and applied");
			}
		}

		//----------------------------------------------------------------------------
		//After each chunk, and the last chunk too.
		//----------------------------------------------------------------------------

		//See comments at top.  Force return of a lot of memory so VM usage figs 
		//during the next chunk are at least usable.
		CreateLocalHeaps();
		//V2.16  Default heap can clean up too.  No guarantees with this but no harm.
		HeapCompact(GetProcessHeap(), 0); 

		//Revert SL stats attribution to whatever code it was before we started.
		if (flush_mode != REORG)
			diags->StartSLStats(stats, prev_sl_activity);
	}
	catch (...) {
		if (mergetask)
			util::WorkerThreadTask::CleanUp(mergetask);

		//This will be a lot of memory, so important to release it if we want to carry on
		info.clear();
		throw;
	}

	return rcode;
}


//**************************************************************************************
DU1MergeParallelSubTask::DU1MergeParallelSubTask
(SingleDatabaseFileContext* c, DU1FieldIndex* i, bool r)
: WorkerThreadSubTask(std::string("Index for ").append(i->FieldName())),
	context(c), fix(i), isreorg(r)
{}

//***********************************************
void DU1MergeParallelSubTask::ClaimError()
{
	FastLoadInputFile* tapei = fix->TapeI();

	//Extra diagnostics when tape supplied by user (i.e. not generated internally)
	if (tapei)
		tapei->Request()->SetErrTape(tapei);
}

//***********************************************
void DU1MergeParallelSubTask::Perform()
{
	DatabaseServices* dbapi = context->DBAPI();
	CoreServices* core = dbapi->Core();
	LoadDiagnostics* diags = fix->DU1()->Diags();
	DatabaseFileIndexManager* indexmgr = context->GetDBFile()->GetIndexMgr();

	double stime = win::GetFTimeWithMillisecs();
	int nfiles = fix->NumSeqFiles();

	std::string msg = fix->Merge(context, indexmgr);

	double etime = win::GetFTimeWithMillisecs();
	double mrg_time = etime - stime;

	//Slightly awkward message leveling difference between fastload and regular DU1
	if (isreorg && diags->GetLevel() < LOADDIAG_VERBOSE)
		;
	else {
		diags->AuditFinal(core, msg);
		if (fix->MergeFinalChunkMsg() != "")
			diags->AuditFinal(core, fix->MergeFinalChunkMsg());

		msg = (nfiles > 1) ? "  Merged and applied in " : "  Applied in ";
		msg.append(RoundedDouble(mrg_time).ToStringWithFixedDP(3)).append("s");
		diags->AuditFinal(core, msg);
	}

	static_cast<DU1MergeTask*>(parent_task)->AddTime(mrg_time);
}







//**************************************************************************************
//**************************************************************************************
//**************************************************************************************
void DU1SeqOutputFile::SetFieldValue(const std::string& s)
{
	vallen = s.length();
	value_buffer[0] = vallen;
	memcpy(value_buffer+1, s.c_str(), vallen);

	vallen++;
}

//***********************************************
void DU1SeqOutputFile::SetFieldValue(const FieldValue* v)
{
	if (v->CurrentlyNumeric())
		SetFieldValue(*(v->RDData()));
	else {
		vallen = v->StrLen();
		value_buffer[0] = vallen;
		memcpy(value_buffer+1, v->StrChars(), vallen);
		vallen++;
	}
}

//***********************************************
void DU1SeqOutputFile::WriteSegInvList
(short segnum, unsigned short segnrecs, void* source)
{
	SegNum() = segnum;
	SegNRecs() = segnrecs;
	Write(seg_header_buffer, 4);

	if (segnrecs <= DLIST_MAXRECS)
		Write(source, segnrecs*2);
	else
		Write(source, DBPAGE_BITMAP_BYTES);
}








//**************************************************************************************
DU1MergeStream_Mem::DU1MergeStream_Mem(DU1FieldIndex* i, bool n)
: DU1MergeStream(n), index(i)
{
}

//***********************************************
void DU1MergeStream_Mem::ReadFirstKey()
{
	index->BeginIterator(); 
	CacheValues();
}

//***********************************************
void DU1MergeStream_Mem::CacheValues()
{
	if (FieldIsNum()) {
		const RoundedDouble& dv = index->ValAtNumIter();
		SetCacheNum(&dv);
	}
	else {
		const std::string& sv = index->ValAtStrIter();
		SetCacheStr(sv.c_str(), sv.length());
	}
}

//***********************************************
util::MergeRecordStream* DU1MergeStream_Mem::ReadNextKey()
{
	index->AdvanceIterator();

	if (index->IteratorEnded()) {

		final_msg = "Last chunk stats:";
		stats.PrepMsg(final_msg, index->NumValues());

		return NULL;
	}

	CacheValues();
	return this;
}

//***********************************************
void DU1MergeStream_Mem::MergeChunkSet(BitMappedFileRecordSet* fvalset)
{
	DU1InvList* invlist = index->ListAtIterator();

	//The usual special case for unique values
	if (invlist->NumSegs() == 0) {
		stats.unique_f++;
		fvalset->BitOr(invlist->URN());
	}
	else {
		//Make value set from the last chunk info still in memory
		BitMappedFileRecordSet chunkvalset(fvalset->HomeFileContext());
		invlist->FlushData(&chunkvalset, NULL, &stats, index->DU1());

		//Merge it with any other sets from files for the value
		fvalset->BitOr(&chunkvalset, true);
	}
}






//**************************************************************************************
DU1MergeStream_File::DU1MergeStream_File(const char* f, bool n)
: DU1MergeStream(n), fname(f)
{
	fhandle = util::StdioSopen(fname, util::STDIO_OLD);
}


//***********************************************
DU1MergeStream_File::~DU1MergeStream_File()
{
	util::StdioClose(fhandle);
}

//***********************************************
void DU1MergeStream_File::ReadFirstKey()
{
	util::StdioLseek(fhandle, fname, 0, SEEK_SET);
	if (ReadNextKey() == NULL)
		throw Exception(DBA_DEFERRED_UPDATE_ERROR, 
			std::string("Bug - Empty DU merge file: ").append(fname));
}

//***********************************************
bool DU1MergeStream_File::ReadFile(void* dest, int trybytes, bool eof_allowed)
{
	int gotbytes = util::StdioRead(fhandle, fname, dest, trybytes);

	if (gotbytes != trybytes) {
		if (gotbytes == 0 && eof_allowed)
			return true;

		//Insufficient data left in file - this should not happen.  If the counts etc. in 
		//the data are right we should come neatly to the end after all segs for a value.
		//V2.16.  Make a copy of the suspect file.
		std::string msg("Bug - Suspected corrupt DU merge file data, reading ");
		msg.append(fname).append(", wanted ").append(util::IntToString(trybytes));
		msg.append(" bytes, got ").append(util::IntToString(gotbytes));

		try {
			msg.append(". Seq file pointer is now at ");
			msg.append(util::Int64ToString(util::StdioTellI64(fhandle, fname)));

			std::string dumpfname = std::string(util::StartupWorkingDirectory()).append("\\")
				.append("#DUMPS\\DU_BAD_FILE.TMP");
			util::StdioEnsureDirectoryExists(dumpfname.c_str());
			dumpfname = win::GetUnusedFileName(dumpfname);
			int dumphandle = util::StdioSopen(dumpfname.c_str(), util::STDIO_NEW);
			util::StdioCopyFile(dumphandle, dumpfname.c_str(), fhandle, fname);
			util::StdioClose(dumphandle);

			dumpfname = std::string(util::StartupWorkingDirectory()).append("\\")
				.append("#DUMPS\\DU_BAD_OBJ.TMP");
			dumpfname = win::GetUnusedFileName(dumpfname);
			dumphandle = util::StdioSopen(dumpfname.c_str(), util::STDIO_NEW);
			util::StdioWrite(dumphandle, dumpfname.c_str(), this, sizeof(*this));
			util::StdioClose(dumphandle);

			msg.append(". Seq file copied to #DUMPS directory");
		}
		catch (...) {
			msg.append(". Attempted to dump file but encountered unknown error");
		}

		throw Exception(DBA_DEFERRED_UPDATE_ERROR, msg);
	}

	//Successful read
	return false;
}

//***********************************************
util::MergeRecordStream* DU1MergeStream_File::ReadNextKey()
{
	bool eof = false;

	//For string values we need to read the value length first
	if (FieldIsNum())
		valuelen = 8;
	else
		eof = ReadFile(&valuelen, 1, true);

	//There will always be at least 6 bytes after the value (#segs and either URN or
	//the first seg# and seg #recs).
	if (!eof)
		eof = ReadFile(valheader_buffer, valuelen + 6, FieldIsNum());

	if (eof)
		return NULL;

	if (FieldIsNum())
		SetCacheNum(reinterpret_cast<RoundedDouble*>(valheader_buffer));
	else
		SetCacheStr(valheader_buffer, valuelen);

	num_segs_read = 0;
	return this;
}

//***********************************************
void DU1MergeStream_File::MergeChunkSet(BitMappedFileRecordSet* fvalset)
{
	short numsegs = NumSegs();

	//Unique record in the whole chunk
	if (numsegs == 0) {
		fvalset->BitOr(URN());
		return;
	}

	//ILMR case - make a file-wide set from the segment set data
	BitMappedFileRecordSet chunkvalset(fvalset->HomeFileContext());
	while (numsegs-- > 0) {

		//The first segment header comes in with the value in case it was a URN
		if (num_segs_read == 0)
			SegHeaderAsInt() = URN();

		//For segments after the first, get seg# and seg #recs
		else
			ReadFile(seg_header_buffer, 4, false);

		num_segs_read++;


		short segnum = SegNum();
		unsigned short segnrecs = SegNRecs();

		//Single record in segment
		if (segnrecs == 1) {
			unsigned short relrec;
			ReadFile(&relrec, 2, false);
			chunkvalset.AppendSegmentSet(new SegmentRecordSet_SingleRecord(segnum, relrec));
		}

		//Multiple records but not enough for a bitmap
		else if (segnrecs <= DLIST_MAXRECS) {

			SegmentRecordSet_Array* arrayset = 
				new SegmentRecordSet_Array(segnum, segnrecs);

			//Read file directly into the arrayset memory
			ReadFile(arrayset->Array().Data(), segnrecs*2, false);
			arrayset->Array().SetCount(segnrecs);

			chunkvalset.AppendSegmentSet(arrayset);
		}

		//Bitmap
		else {
			SegmentRecordSet_BitMap* bmapset = new SegmentRecordSet_BitMap(segnum);

			//As above, read file directly into bitmap set memory
			ReadFile(bmapset->Data()->Data(), DBPAGE_BITMAP_BYTES, false);

			//Otherwise later count would return zero
			bmapset->Data()->InvalidateCachedCount(); 

			chunkvalset.AppendSegmentSet(bmapset);
		}
	}

	//Merge it with any other sets for the value
	fvalset->BitOr(&chunkvalset, true);
}








//**************************************************************************************
DU1MergeStream_FastLoadTape::DU1MergeStream_FastLoadTape
(FastLoadInputFile* t, bool n)
: DU1MergeStream(n), tapei(t)
{
	//Since we closed for tidiness earlier
	tapei->OpenFile();

	nofloat_option = tapei->NofloatOption();
	crlf_option = tapei->CrlfOption();
	pai_option = tapei->PaiOption();

	FastLoadFieldInfo* info = tapei->Request()->GetLoadFieldInfoByName(tapei->TapeIFieldName());
	float_values_in_tape = info->TAPEDI_Atts().IsOrdNum() && !nofloat_option && !pai_option;
	
	throw_bad_numbers = (tapei->Request()->Context()->DBAPI()->GetParmFMODLDPT() & 1) ? true : false;
}

//***********************************************
DU1MergeStream_FastLoadTape::~DU1MergeStream_FastLoadTape()
{
	//Close to free up file handles
	tapei->CloseFile();
}

//***********************************************
util::MergeRecordStream* DU1MergeStream_FastLoadTape::ReadNextKey()
{
	bool eof = false;

	//Read the value
	if (pai_option)
		pai_line = tapei->ReadTextLine(&eof);

	else if (float_values_in_tape)
		num_key_value = tapei->ReadBinaryDouble(&eof);

	else {
		string_key_len = tapei->ReadBinaryUint8(&eof);
		if (!eof)
			tapei->ReadChars(string_key_value, string_key_len);
		string_key_value[string_key_len] = 0;
	}

	if (eof)
		return NULL;

	//Convert as appropriate to store in the database.  Various combinations, although
	//in the initial release some can not happen.  For example we're not allowing
	//redefine from ORD CHAR to ORD NUM as part of a reorg.
	if (FieldIsNum()) {

		//TAPEI supplied in actual numeric format is assumed to have come out of
		//DPT in the first place, and already have valid RoundedDouble values.
		if (float_values_in_tape)
			;
		else {
			try {
				if (pai_option)
					num_key_value = RoundedDouble(pai_line).Data();
				else if (nofloat_option)
					num_key_value = RoundedDouble(std::string(string_key_value)).Data();
				else
					//See comment above.  This case should have been screened out before now.
					throw Exception(BUG_MISC, "Bug: ORD CHAR tape made it through to ORD NUM load?");
			}
			catch (...) {
				if (throw_bad_numbers)
					throw;
				num_key_value = 0;
			}
		}

		SetCacheNum(reinterpret_cast<RoundedDouble*>(&num_key_value));
	}

	//Ordered character fields
	else {
		if (pai_option) {
			string_key_len = pai_line.length();
			strcpy(string_key_value, pai_line.c_str());
		}
		else if (float_values_in_tape) {
			//See comment above.  This case should have been screened out before now.
			throw Exception(BUG_MISC, "Bug: ORD NUM tape made it through to ORD CHAR load?");
			//string_key_len = sprintf(string_key_value, "%.20G", num_key_value);
		}

		SetCacheStr(string_key_value, string_key_len);
	}

	if (pai_option)
		numsegs = 0xffff;
	else
		numsegs = tapei->ReadBinaryUint16();

	return this;
}

//***********************************************
void DU1MergeStream_FastLoadTape::MergeChunkSet(BitMappedFileRecordSet* fvalset)
{
	FastLoadRequest* request = tapei->Request();

	//PAI-style inverted list
	if (pai_option) {

		//The blank line is compulsory, so no eof tests
		std::string line = tapei->ReadTextLine();
		while (line.length()) {
			int taperec = util::StringToInt(line);
			int filerec = request->XrefRecNum(taperec);

			fvalset->FastAppend(filerec);
			line = tapei->ReadTextLine();  
		}
		return;
	}

	//Unique record in the tape (undocumented for user input but DPT uses by default)
	if (numsegs == 0) {
		int taperec = tapei->ReadBinaryInt32();
		int filerec = request->XrefRecNum(taperec);

		fvalset->FastAppend(filerec);
		return;
	}

	//Init these here.  They're reread at bottom of each loop to test for terminator
	short segnum = tapei->ReadBinaryInt16();
	unsigned short nrecs = tapei->ReadBinaryUint16();

	while (numsegs--) {

		//-----------------------------------------------------------------------------
		//Note: The segment numbers coming in can't be relied on to remain valid
		//since every record number may change for a variety of reasons (all of which
		//may happen in typical cases so we can't really special-case a fast path). So 
		//each inverted section below goes via FileSet:BitOr(absrecnum) x N.
		//-----------------------------------------------------------------------------

		//Bitmap form
		if (nrecs == 0xFFFF) {

			//Read in bitmap
			util::BitMap bmtemp(DBPAGE_BITMAP_SIZE);
			tapei->ReadRawData(bmtemp.Data(), DBPAGE_BITMAP_BYTES);

			//Run each rec# through the Xref table
			for (unsigned int bit = 0; bit < (unsigned int)DBPAGE_BITMAP_SIZE; bit++) {
				if (!bmtemp.FindNext(bit, bit))
					break;

				int taperec = AbsRecNumFromRelRecNum(bit, segnum);
				int filerec = request->XrefRecNum(taperec);

				//Add to set for value
				fvalset->FastAppend(filerec);
			}
		}

		//Array form
		else if (nrecs > 0) {

			util::Array<unsigned short> arrtemp(nrecs);

			//Read in array
			tapei->ReadBinaryUint16Array(arrtemp.Data(), nrecs);

			//Run each rec# through the Xref table
			for (int entry = 0; entry < nrecs; entry++) {
				unsigned short relrec = arrtemp[entry];

				int taperec = AbsRecNumFromRelRecNum(relrec, segnum);
				int filerec = request->XrefRecNum(taperec);

				//Add to set for value
				fvalset->FastAppend(filerec);
			}
		}

		//Optional seglist separator
		if (crlf_option)
			tapei->ReadCRLF();

		//Optional FFFFFFFF value terminator might occur after any seg list.
		//Read it in 2 chunks to get correct endianizing of 2 vars for the next loop.
		bool eof = false;
		segnum = tapei->ReadBinaryInt16(&eof);
		if (eof)
			break;
		nrecs = tapei->ReadBinaryUint16();
		if (segnum == -1 && nrecs == 0xFFFF)
			break;
	}
}


} //close namespace


