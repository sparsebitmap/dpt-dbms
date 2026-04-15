
//*****************************************************************************************
//The "Find Engine"
//*****************************************************************************************

#include "stdafx.h"

#include "dbf_find.h"

//Utils
#include "dataconv.h"
#include "cfr.h" //#include "CFR.h" : V2.24 case is less interchangeable on *NIX - Roger M.
#include "handles.h"
#include "lineio.h"
//API tiers
#include "dbctxt.h"
#include "recread.h"
#include "findspec.h"
#include "foundset.h"
#include "recset.h"
#include "findwork.h"
#include "dbf_field.h"
#include "dbf_ebm.h"
#include "btree.h"
#include "inverted.h"
#include "valdirect.h"
#include "dbfile.h"
#include "dbserv.h"
#include "core.h"
//Diagnostics
#include "except.h"
#include "msg_db.h"

namespace dpt {

//*****************************************************************************************
FindOperation::FindOperation
(int g, SingleDatabaseFileContext* c, FoundSet* r, const FindSpecification& s, 
 const FindEnqueueType& lt, const BitMappedFileRecordSet* b, DatabaseFile* f) 
: groupix(g), sfc(c), spec(s), locktype(lt), resultset(r), baseset(b), dbapi(c->DBAPI()),
	op(c->DBAPI()->Core()->Output()), dvc_prelock(NULL), fwi(c, s), file(f)
{}

//*****************************************************************************************
void FindOperation::Perform()
{
	bool is_referback_find = (baseset != NULL);

	if (spec.Diag_Spec()) {
		op->WriteLine("*****************************************");
		op->Write("Runtime search diagnostics, file=");
		op->WriteLine(sfc->GetShortName());
		op->WriteLine("*****************************************");

		std::vector<std::string> vs;
		spec.Dump(vs);
		for (size_t i = 0; i < vs.size(); i++)
			op->WriteLine(vs[i]);
		op->WriteLine("**");
	}

	if (spec.Diag_Workinit()) {
		std::vector<std::string> vs;
		fwi.Dump(vs, !is_referback_find);
		for (size_t i = 0; i < vs.size(); i++)
			op->WriteLine(vs[i]);
		op->WriteLine("**");
	}

	if (spec.Diag_Norun()) {
		op->Write("NORUN flag (");
		op->Write(util::IntToString(FD_RTD_NORUN));
		op->WriteLine(") was set");
		return;
	}

	//-----------------------------------------------------------------
	//Appropriate structure locks all have to be taken together up front, to ensure
	//all records in the final found set are in there for consistent reasons.
	CFRSentry se;
	if (is_referback_find)
		fwi.SetRootSet(baseset->MakeCopy(), false, NULL);
	else
		se.Get(dbapi, file->cfr_exists, BOOL_SHR);

	//It's debatable if maintaining an INDEX resource for each field would be beneficial 
	//- it's how I originally intended it, but this is I think as per M204, and simple.
	CFRSentry si;
	if (fwi.IndexSearchFlag()) {
		si.Get(dbapi, file->cfr_index, BOOL_SHR);
		dvc_prelock = &si;
	}

	//If we're doing a table B search, record locks are used, as per M204
	BitMappedFileRecordSet tbs_set(sfc);
	if (fwi.TableBSearchFlag()) {

		//Don't install these as the work tree root because of the complications it
		//would add in terms of maintaining the rec locks if/when the root gets refined.
		if (is_referback_find)
			baseset->MakeCopy(&tbs_set);
		else {
			if (spec.Diag_Critlog())
				op->WriteLine("Table B search is possible: retrieving entire EBP up front");

			file->GetEBMMgr()->CreateWholeFileSet(dbapi, sfc, &tbs_set);
			if (tbs_set.IsEmpty()) {
				FinalDiagnostics();				
				return;
			}
		}

		tbs_set.LockShr(dbapi);
	}

	//-----------------------------------------------------------------
	//See tech docs for detailed comments on all of this, particuarly the order of criteria.
	std::vector<FindWorkNode_Leaf*> leaves;
	size_t x;

	//--------------
	//FILE$ - the most powerful criterion when it's a different file
	fwi.GetUnprocessedLeavesByType(&leaves, FD_FILE$);
	for (x = 0; x < leaves.size(); x++)
		if (!leaves[x]->IsComplete())
			Perform_File$(leaves[x], false);

	//--------------
	//Single record - the hidden FRN find or general API use
	fwi.GetUnprocessedLeavesByType(&leaves, FD_SINGLEREC);
	for (x = 0; x < leaves.size(); x++)
		if (!leaves[x]->IsComplete())
			Perform_Singlerec(leaves[x]);

	//--------------
	//LIST$ and FIND$
	fwi.GetUnprocessedLeavesByType(&leaves, FD_SET$);
	for (x = 0; x < leaves.size(); x++)
		if (!leaves[x]->IsComplete())
			Perform_Set$(leaves[x]);

	//--------------
	//Indexed equality
	fwi.GetUnprocessedLeavesByType(&leaves, FD_EQ);
	for (x = 0; x < leaves.size(); x++)
		if (!leaves[x]->IsComplete())
			Perform_IxEQ(leaves[x]);

	//--------------
	//All indexed ranges and patterns
	fwi.GetUnprocessedLeavesByType(&leaves, FD_RANGE);
	for (x = 0; x < leaves.size(); x++)
		if (!leaves[x]->IsComplete())
			Perform_IxRange(leaves[x]);

	//-----------------------------------------------------------------
	//Whatever EBP is still required by now
	if (Perform_EBP_Phase(is_referback_find, &tbs_set))
		return;

	//--------------
	//Point$
	fwi.GetUnprocessedLeavesByType(&leaves, FD_POINT$);
	for (x = 0; x < leaves.size(); x++)
		if (!leaves[x]->IsComplete())
			Perform_Point$(leaves[x]);

	//--------------
	//Same file FILE$ - unlike the negative version (see top) this is usually weak
	fwi.GetUnprocessedLeavesByType(&leaves, FD_FILE$);
	for (x = 0; x < leaves.size(); x++)
		if (!leaves[x]->IsComplete())
			Perform_File$(leaves[x], true);

	//--------------
	//All remaining criteria - table B search.
	if (!fwi.IsComplete())
		Perform_TableBSearch(&tbs_set);

	//-----------------------------------------------------------------
	//That's the result set
	FinalDiagnostics();				
	if (fwi.RootSet() == NULL)
		return;

	//Try and place a record lock on them.  This has the automatic ENQRETRY waiting
	//mechanism, during which we have resource locks, but if fail/throw, all released.
	if (locktype == FD_LOCK_SHR)
		fwi.RootSet()->LockShr(dbapi);

	//V2.25.  Yikes - FDWOL placed EXCL!  Better than the other way round I suppose.
//	else
	else if (locktype == FD_LOCK_EXCL)
		fwi.RootSet()->LockExcl(dbapi);

	//Finally adopt the set out of the work tree and call it a day
	resultset->AppendFileSet(groupix, fwi.RootSet());
	fwi.SetRootSet(NULL, false, NULL);
}


//****************************************************************************************
//Criterion type handlers in the order they are called above
//****************************************************************************************
void FindOperation::Perform_File$(FindWorkNode_Leaf* crit, bool match_reqd)
{
	bool matchfile = false;

	//This covers two criteria: the internally-used ALLRECS
	if (crit->BasicOp() == FD_ALLRECS)
		matchfile = true;

	//and FILE$
	else 
		matchfile = (file->GetDDName() == crit->Operand1().ExtractString());

	if (!crit->Positive())
		matchfile = !matchfile;

	//Wrong file - very powerful
	if (!matchfile && !match_reqd) {
		CriterionDiagnostics1(crit);
		crit->RegisterLeafSet(NULL, DiagOP(op, spec.Diag_Critcount()));
		CriterionDiagnostics3(crit);
	}

	//Right file - less powerful and requires EBP, so done later
	else if (matchfile && match_reqd) {
		CriterionDiagnostics1(crit);
		crit->RegisterWholeFileLeafSet(DiagOP(op, spec.Diag_Critcount()));
		CriterionDiagnostics3(crit);
	}
}

//****************************************************************************************
void FindOperation::Perform_Singlerec(FindWorkNode_Leaf* crit)
{
	CriterionDiagnostics1(crit);

	int recnum = (int) crit->Operand1().ExtractRoundedDouble().Data();

	BitMappedFileRecordSet* baseset = crit->RestrictingSet();
	BitMappedFileRecordSet* critset;

	//Negative record number on FRN is compiler-legal in UL: no loop happens
	if (recnum < 0)
		critset = NULL;
	else if (baseset && !baseset->ContainsAbsRecNum(recnum))
		critset = NULL;

	//If we create a segment set in an arbitrarily high segment here it would mean all 
	//EBP processing would have to look arbitrarily high too.  It's much nicer if we
	//can assume the EBP covers every possible set that gets created.  So force that here.
	else if (!file->GetEBMMgr()->DoesPrimaryRecordExist(dbapi, recnum))
		critset = NULL;

	else {
		critset = new BitMappedFileRecordSet(sfc);
		try {
			SegmentRecordSet_SingleRecord* ss = new SegmentRecordSet_SingleRecord(recnum);
			critset->AppendSegmentSet(ss);
		}
		catch (...) {
			delete critset;
			throw;
		}
	}

	//No negation issues - negative singlerec is disallowed earlier
	CriterionDiagnostics2(critset);
	crit->RegisterLeafSet(critset, DiagOP(op, spec.Diag_Critcount()));
	CriterionDiagnostics3(crit);
}

//****************************************************************************************
void FindOperation::Perform_Set$(FindWorkNode_Leaf* crit)
{
	CriterionDiagnostics1(crit);

	BitMappedFileRecordSet* restricting_set = crit->RestrictingSet();
	PrepareForPossibleNegation(restricting_set, crit);

	int i = (int) crit->Operand1().ExtractRoundedDouble().Data();

	//Obviously this is going to crash if it's not actually a list or foundset object
	BitMappedRecordSet* crit_totalset = reinterpret_cast<BitMappedRecordSet*>(i);

	SingleDatabaseFileContext* subcontext = crit_totalset->Context()->CastToSingle();
	if (!subcontext)
		subcontext = crit_totalset->Context()->CastToGroup()->GetMemberContextByGroupOrder(groupix);

	if (subcontext != sfc)
		throw Exception(DML_RUNTIME_BADCONTEXT, 
			"Bug: run-time context mismatch in FIND$/LIST$");
	
	BitMappedFileRecordSet* crit_fileset = crit_totalset->GetFileSubSet(groupix);
	if (crit_fileset)
		crit_fileset = crit_fileset->MakeCopy();

	if (restricting_set)
		//No sense ANDing if we're just going to AND NOT on a fresh copy afterwards
		if (crit->Positive())
			crit_fileset = BitMappedFileRecordSet::BitAnd(crit_fileset, restricting_set);

	NegateSetIfRequired(crit, restricting_set, &crit_fileset);

	CriterionDiagnostics2(crit_fileset);
	crit->RegisterLeafSet(crit_fileset, DiagOP(op, spec.Diag_Critcount()));
	CriterionDiagnostics3(crit);
}

//****************************************************************************************
void FindOperation::Perform_IxEQ(FindWorkNode_Leaf* crit)
{
	CriterionDiagnostics1(crit);

	BitMappedFileRecordSet* restricting_set = crit->RestrictingSet();
	PrepareForPossibleNegation(restricting_set, crit);

	//Going to the trouble to create a DVC here doesn't really add any 
	//processing overhead and it keeps things nice and similar to range/pattern 
	//criteria below where using the DVC is more obviously necessary. 
	//NB.  I kept a separate function even though this is almost identical to the 
	//one below because of the remote possibility of one day introducing new 
	//index types specifically for EQ or range searches (e.g. the M204 KEY and NR).
	DirectValueCursor dvc(sfc, crit->PFI(), dvc_prelock);

	dvc.SetRestriction_Generic(FD_RANGE_GE_LE, crit->Operand1(), crit->Operand1());
	BitMappedFileRecordSet* critset = DVCFind(dvc, restricting_set, true);

	NegateSetIfRequired(crit, restricting_set, &critset);

	CriterionDiagnostics2(critset);
	crit->RegisterLeafSet(critset, DiagOP(op, spec.Diag_Critcount()));
	CriterionDiagnostics3(crit);
}

//****************************************************************************************
void FindOperation::Perform_IxRange(FindWorkNode_Leaf* crit)
{
	CriterionDiagnostics1(crit);

	BitMappedFileRecordSet* restricting_set = crit->RestrictingSet();
	PrepareForPossibleNegation(restricting_set, crit);

	DirectValueCursor dvc(sfc, crit->PFI(), dvc_prelock);

	dvc.SetRestriction_Generic(crit->BasicOp(), crit->Operand1(), crit->Operand2());
	BitMappedFileRecordSet* critset = DVCFind(dvc, restricting_set);

	NegateSetIfRequired(crit, restricting_set, &critset);

	CriterionDiagnostics2(critset);
	crit->RegisterLeafSet(critset, DiagOP(op, spec.Diag_Critcount()));
	CriterionDiagnostics3(crit);
}

//****************************************************************************************
BitMappedFileRecordSet* FindOperation::DVCFind(DirectValueCursor& dvc, 
 BitMappedFileRecordSet* restricting_set, bool eq)
{
	BitMappedFileRecordSet* result = NULL;

	try {
		for (dvc.GotoFirst(); dvc.CanEnterLoop(); dvc.Advance()) {

			InvertedListAPI il = dvc.btree->InvertedList();
			BitMappedFileRecordSet* onevalset = il.AssembleRecordSet(restricting_set);

			//OR the results for each matched value
			if (onevalset) {
				if (!result)
					result = onevalset;
				else {
					result->BitOr(onevalset);
					delete onevalset;
				}
			}

			//Slight kluge for equality find (see above) - GE but only take the first value
			if (eq)
				break;
		}
	}
	catch (...) {
		if (result)
			delete result;
		throw;
	}

	return result;
}

//****************************************************************************************
bool FindOperation::Perform_EBP_Phase(bool is_referback_find, BitMappedFileRecordSet* ptbs_set)
{
	if (spec.Diag_Critlog())
		op->WriteLine("EBP phase");

	BitMappedFileRecordSet* post_ebp_root;

	if (is_referback_find) {
		if (spec.Diag_Critlog())
			op->WriteLine("  (n/a - this is a referback find)");

		post_ebp_root = fwi.RootSet();
	}

	else if (fwi.GotEarlyEBPForNegation()) {
		if (spec.Diag_Critlog())
			op->WriteLine("  (n/a - EBPs were already retrieved to perform negation)");

		post_ebp_root = fwi.RootSet();
	}

	else if (fwi.RootSet()) {
		if (spec.Diag_Critlog()) {
			op->Write("  Masking current root working set");
			if (spec.Diag_Critcount()) {
				op->Write(" (");
				op->Write(util::IntToString(fwi.RootSet()->Count()));
				op->WriteLine(" records)");
			}
		}

		post_ebp_root = file->GetEBMMgr()->MaskOffNonexistentInSet(dbapi, fwi.RootSet());
	}

	//No set at the root level because of completion with no records found
	else if (fwi.IsComplete()) {
		if (spec.Diag_Critlog())
			op->WriteLine("  (n/a - set is complete with no records found)");
		post_ebp_root = NULL;
	}

	//Nothing to go on at the root level yet, but there are still criteria to be evaluated,
	//so tough luck - we need the entire file EBP.
	else if (fwi.TableBSearchFlag()) {
		if (spec.Diag_Critlog())
			op->WriteLine("  Applying EBP copy previously retrieved for TBS");

		post_ebp_root = ptbs_set->MakeCopy();
	}
	else {
		if (spec.Diag_Critlog())
			op->WriteLine("  Root node not yet begun - retrieving entire EBP");

		post_ebp_root = file->GetEBMMgr()->CreateWholeFileSet(dbapi, sfc);
	}

	//This gets propagated down to any as-yet incomplete nodes
	fwi.SetRootSet(post_ebp_root, true, DiagOP(op, spec.Diag_Critcount()));

	if (spec.Diag_Critlog()) {
		op->WriteLine("End of EBP phase");

		if (spec.Diag_Critcount()) {
			op->Write("  Root set count now : ");
			if (post_ebp_root)
				op->WriteLine(util::IntToString(post_ebp_root->Count()));
			else
				op->WriteLine("0");
		}
	}

	if (!post_ebp_root) {
		FinalDiagnostics();				
		return true;
	}

	return false;
}


//****************************************************************************************
void FindOperation::Perform_Point$(FindWorkNode_Leaf* crit)
{
	CriterionDiagnostics1(crit);

	int recnum = (int) crit->Operand1().ExtractRoundedDouble().Data();
	if (recnum < 0)
		recnum = 0;
	short segnum = SegNumFromAbsRecNum(recnum);

	short loseg = (crit->Positive()) ? segnum : 0;
	short hiseg = (crit->Positive()) ? SHRT_MAX : segnum;

	//There must always be a base set by the time we do POINT$
	BitMappedFileRecordSet* restricting_set = crit->RestrictingSet();
	if (!restricting_set)
		throw Exception(DB_ALGORITHM_BUG, "Bug: POINT$: no base set");

	BitMappedFileRecordSet* critset = restricting_set->MakeCopy(NULL, loseg, hiseg);
	critset = BitMappedFileRecordSet::Point$MaskOff(critset, recnum, crit->Positive());

	CriterionDiagnostics2(critset);
	crit->RegisterLeafSet(critset, DiagOP(op, spec.Diag_Critcount()));
	CriterionDiagnostics3(crit);
}

//****************************************************************************************
void FindOperation::Perform_TableBSearch(BitMappedFileRecordSet* wholefile_set)
{
	std::vector<FindWorkNode_Leaf*> leaves;
	fwi.GetUnprocessedLeavesByType(&leaves, FD_NULLOP);

	//Create a dummy found set so we can use a record cursor later
	BitMappedFileRecordSet* scanset = new BitMappedFileRecordSet(sfc);
	FoundSet fs(sfc);
	fs.AppendFileSet(0, scanset);

	//Prepare work areas
	std::vector<TBSLeafInfo> leafinfo;
	leafinfo.resize(leaves.size());
	std::vector<std::string> diags;

	//Prep for each criterion
	size_t x;
	for (x = 0; x < leaves.size(); x++) {
		if (spec.Diag_Critlog())
			leaves[x]->Dump(diags, 2);

		leafinfo[x].built_set = new BitMappedFileRecordSet(sfc);
		leafinfo[x].base_set = leaves[x]->RestrictingSet();

		//The set of records we will have to scan is the union of all TBS criteria sets
		scanset->BitOr(leafinfo[x].base_set);
	}

	if (spec.Diag_Critlog()) {
		op->WriteLine("Table B search phase criteria remaining:");
		for (x = 0; x < diags.size(); x++)
			op->WriteLine(diags[x]);
	}

	//The EBP should have marked all the nodes complete if they were
	int numrecs = scanset->Count();
	if (numrecs == 0)
		throw Exception(DB_ALGORITHM_BUG, "Bug: TBS scanset is empty");

	//First MBSCAN check: zero disallows TBS entirely
	int mbscan = dbapi->GetParmMBSCAN();
	if (mbscan == 0) {
		std::string msg("Table B search was required but MBSCAN was zero.  Field(s): ");
		for (x = 0; x < leaves.size(); x++) {
			if (x > 0)
				msg.append(", ");
			msg.append(leaves[x]->PFI()->name);
		}
		throw Exception(DML_TABLEB_MBSCAN, msg);
	}

	//Lock the reduced set instead of the original whole-file set
	wholefile_set->TBSHandOverLock(scanset);
	wholefile_set->ClearButNoDelete();

	//Second MBSCAN check - ask the user to confirm if there are lots of records.
	//Hopefully they will reply quickly - we have INDEX+EXISTS+reclocks!
	if (mbscan > 0 && numrecs > mbscan) {
		if (!dbapi->Core()->InteractiveYesNo(std::string("About to scan ")
				.append(util::IntToString(numrecs))
				.append(" table B records - continue?").c_str(), false))
			throw Exception(DML_TABLEB_MBSCAN, 
				"Find operation cancelled (table B search request was denied)");
	}

	if (spec.Diag_Critcount()) {
		op->Write("  Scanning ");
		op->Write(util::IntToString(numrecs));
		op->WriteLine(" records...");
	}

	//Scan the records
	short prevseg = -1;
	RecordSetCursorHandle h(&fs);

	for (h.GotoFirst(); h.CanEnterLoop(); h.Advance(1)) {
		ReadableRecord* r = h.AccessCurrentRecordForRead();

		//Release record locks after each segment
		int absrec = r->RecNum();
		short recseg = SegNumFromAbsRecNum(absrec);
		if (recseg != prevseg) {
			if (prevseg != -1) {
				fs.TBSDropSegmentLock(prevseg, h.GetLiveCursor());
				//h.GotoFirst(); done more efficiently in the above func now
			}
			prevseg = recseg;
		}

		//Check the record against each criterion that needs it
		for (x = 0; x < leaves.size(); x++) {
			if (!leafinfo[x].base_set->ContainsAbsRecNum(absrec))
				continue;

			std::string& fname = leaves[x]->PFI()->name;
			FieldValue fval;
			bool positive_match = false;

			//Just a little FEO loop on the field
			for (int n = 1; ; n++) {
				if (!r->GetFieldValue(fname, fval, n))
					break;

				if (leaves[x]->TableBSearchPositiveTestField(fval)) {
					positive_match = true;
					break;
				}
			}

			//Place successful records on the appropriate list
			if (positive_match == leaves[x]->Positive())
				leafinfo[x].built_set->BitOr(r);
		}
	}

	//V2.25 Feb 2010.  I can hardly believe it but this stat was never updated!
	file->AddToStatDIRRCD(dbapi, numrecs);

	if (spec.Diag_Critcount())
		op->WriteLine("  Scan complete - amalgamating result sets:");

	//Finally copy the sets of successful records into the tree
	for (x = 0; x < leaves.size(); x++) {
		BitMappedFileRecordSet* temp = leafinfo[x].built_set;
		leafinfo[x].built_set = NULL;

		if (spec.Diag_Critcount()) {
			op->Write("    ");
			op->WriteLine(diags[x]);
			op->Write("      ");
		}

		//It's possible that as we do this some will become irrelevant
		if (leaves[x]->IsComplete()) {
			if (spec.Diag_Critcount())
				op->WriteLine("    Now irrelevant because of other criteria");
			delete temp;
		}
		else {
			if (spec.Diag_Critcount())
				CriterionDiagnostics2(temp);
			leaves[x]->RegisterLeafSet(temp, DiagOP(op, spec.Diag_Critcount()));
		}
	}
}

//**********************
FindOperation::TBSLeafInfo::~TBSLeafInfo()
{
	if (built_set) 
		delete built_set;
}

//*****************************************************************************************
void FindOperation::PrepareForPossibleNegation
(const BitMappedFileRecordSet* baseset, FindWorkNode_Leaf* crit)
{
	if (baseset || crit->Positive())
		return;

	//I decided to go for it and make a complete EBP - see tech docs for why
	BitMappedFileRecordSet* ebp = file->GetEBMMgr()->CreateWholeFileSet(dbapi, sfc);
	fwi.SetRootSet(ebp, false, NULL);
	fwi.SetEarlyEBPFlag();

	if (spec.Diag_Critlog())
		op->WriteLine("    Retrieved EBP early because negation required");
}

//*****************************************************************************************
void FindOperation::NegateSetIfRequired
(FindWorkNode_Leaf* crit, const BitMappedFileRecordSet* baseset, BitMappedFileRecordSet** set)
{
	if (crit->Positive())
		return;

	if (spec.Diag_Critcount()) {
		op->Write("    Positive set    : ");
		if (*set)
			op->Write(util::IntToString((*set)->Count()));
		else 
			op->Write("0");
		op->WriteLine(" records.");
	}

	//Use the EBP if required (was readied beforehand by the above func)
	if (!baseset)
		baseset = fwi.RootSet();

	//There would be some benefit to writing a special "MakeMaskedCopy" function as it
	//could save some memcpy work in cases where many records then get removed.  However 
	//that can be considered as a future enhancement.  It would mean more scanning 
	//and would sometimes be slower anyway.
	BitMappedFileRecordSet* basecopy = baseset->MakeCopy();
	try {
		BitMappedFileRecordSet* neg_set = BitMappedFileRecordSet::BitAndNot(basecopy, *set);
		if (*set) 
			delete *set;
		*set = neg_set;
	}
	catch (...) {
		if (basecopy) delete basecopy;
		if (*set) delete *set;
		throw;
	}
}

//*****************************************************************************************
void FindOperation::CriterionDiagnostics1(FindWorkNode_Leaf* crit)
{
	if (spec.Diag_Critlog()) {
		op->WriteLine("Evaluating criterion");

		std::vector<std::string> vs;
		crit->Dump(vs, 2);
		op->WriteLine(vs[0]);

		if (spec.Diag_Critcount()) {
			op->Write("    Restricting set : ");

			BitMappedFileRecordSet* baseset = crit->RestrictingSet();
			if (baseset) {
				op->Write(util::IntToString(baseset->NumSegs()));
				op->WriteLine(" segments");
			}
			else
				op->WriteLine("none");
		}
	}
}

//*****************************************************************************************
void FindOperation::CriterionDiagnostics2(BitMappedFileRecordSet* critset)
{
	if (spec.Diag_Critcount()) {
		op->Write("    Criterion set   : ");
		if (critset)
			op->Write(util::IntToString(critset->Count()));
		else
			op->Write("0");
		op->WriteLine(" records.");
	}
}

//*****************************************************************************************
void FindOperation::CriterionDiagnostics3(FindWorkNode_Leaf* crit)
{
	if (spec.Diag_Critcount()) {
		if (crit->Parent()->IsComplete())
			//By now the working set has been copied up, so can't count it
			op->WriteLine("    All complete node effects applied");
		else {
			BitMappedFileRecordSet* nodeset = crit->Parent()->WorkingSet();
			op->Write("    Node working set: ");
			if (nodeset)
				op->Write(util::IntToString(nodeset->Count()));
			else 
				op->Write("0");
			op->WriteLine(" records.");
		}
	}

//	if (pfs->RTD() & FD_RTD_WORK_COUNTS) {
//		op->Write("Runtime working counts flag (");
//		op->Write(util::IntToString(FD_RTD_WORK_COUNTS));
//		op->WriteLine(") currently has no effect");
//	}
}

//*****************************************************************************************
void FindOperation::FinalDiagnostics()
{
	if (spec.Diag_Critlog()) {
		op->WriteLine("Search complete");
		if (spec.Diag_Critcount()) {
			op->Write("  Final file set: ");
			if (fwi.RootSet())
				op->Write(util::IntToString(fwi.RootSet()->Count()));
			else
				op->Write("0");
			op->WriteLine(" records.");
		}
	}
}

} //close namespace


