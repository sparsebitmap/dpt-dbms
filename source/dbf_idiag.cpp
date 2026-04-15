
#include "stdafx.h"

#include "dbf_index.h"

//Utils
//API tiers
#include "cfr.h" //#include "CFR.h" : V2.24 case is less interchangeable on *NIX - Roger M.
#include "dbctxt.h"
#include "dbf_tabled.h"
#include "dbf_field.h"
#include "dbfile.h"
#include "btree.h"
#include "inverted.h"
#include "page_v.h" //#include "page_V.h" : V2.24 case is less interchangeable on *NIX - Roger M.
#include "page_i.h" //#include "page_I.h"
//Diagnostics
#include "except.h"
#include "msg_db.h"

namespace dpt {

//****************************************************************************************
void DatabaseFileIndexManager::Analyze1
(BTreeAnalyzeInfo* btinfo, InvertedListAnalyze1Info* ilinfo, 
 SingleDatabaseFileContext* sfc, const std::string& fname, bool descending)
{
	//Decided to allow ANALYZE for diagnostic purposes even if file is broken
	file->CheckFileStatus(false, true,  /**/ false/**/ , false);

	PhysicalFieldInfo* pfi = file->GetFieldMgr()->GetPhysicalFieldInfo(sfc, fname);

	if (pfi->atts.IsOrdered()) {
		CFRSentry cs(sfc->DBAPI(), file->cfr_index, BOOL_SHR);
		BTreeAPI bt(sfc, pfi);
		bt.Analyze1(btinfo, ilinfo, sfc, pfi, descending);
	}
	else if (false) {
		; //other index types
	}
	else
		throw Exception(DML_INDEX_REQUIRED, 
			std::string("Field is not indexed: ").append(fname));
}

//****************************************************************************************
void DatabaseFileIndexManager::Analyze2
(InvertedListAnalyze2Info* ilinfo, SingleDatabaseFileContext* sfc)
{
	//Decided to allow ANALYZE for diagnostic purposes even if file is broken
	file->CheckFileStatus(false, true,  /**/ false/**/ , false);

	CFRSentry cs(sfc->DBAPI(), file->cfr_index, BOOL_SHR);

	double ilmr_totfree = 0;
	double il_totfree = 0;

	//Just process table D pages till there are no more
	for (int pagenum = 0; ;pagenum++) {
		try {
			BufferPageHandle bh = file->GetTableDMgr()->GetTableDPage(sfc->DBAPI(), pagenum);

			GenericPage pg(bh);
			char ptype = pg.PageType();

			//ILMRs
			if (ptype == 'I') {
				ilinfo->ilmr_totpages++;

				InvertedListMasterRecordPage pi(bh);
				ilinfo->ilmr_totrecs += pi.NumSlotsInUse();
				ilmr_totfree += pi.NumFreeBytes();
			}

			//Normal inverted lists
			else if (ptype == 'V') {
				ilinfo->il_totpages++;

				InvertedIndexListPage pv(bh);
				ilinfo->il_totrecs += pv.NumSlotsInUse();
				il_totfree += pv.NumFreeBytes();
			}
		}
		catch (Exception& e) {
			if (e.Code() == DB_BAD_PAGE_NUMBER)
				break;
			throw;
		}
	}

	//Calculate averages
	if (ilinfo->ilmr_totpages > 0) 
		ilinfo->ilmr_avefree_frac = 
			(ilmr_totfree / DBPAGE_SIZE) / ilinfo->ilmr_totpages;
	else
		ilinfo->ilmr_avefree_frac = 0; //avoid DBZ

	if (ilinfo->il_totpages > 0) 
		ilinfo->il_avefree_frac = 
			(il_totfree / DBPAGE_SIZE) / ilinfo->il_totpages;
	else
		ilinfo->il_avefree_frac = 0; //avoid DBZ


}

//****************************************************************************************
void DatabaseFileIndexManager::Analyze3
(SingleDatabaseFileContext* sfc, BB_OPDEVICE* op, 
 const std::string& fname, bool leaves_only, bool descending)
{
	//Decided to allow ANALYZE for diagnostic purposes even if file is broken
	file->CheckFileStatus(false, true,  /**/ false/**/ , false);

	PhysicalFieldInfo* pfi = file->GetFieldMgr()->GetPhysicalFieldInfo(sfc, fname);

	if (pfi->atts.IsOrdered()) {
		CFRSentry cs(sfc->DBAPI(), file->cfr_index, BOOL_SHR);
		BTreeAPI::Analyze3(sfc, op, pfi, leaves_only, descending);
	}
	else if (false) {
		; //other index types
	}
	else
		throw Exception(DML_INDEX_REQUIRED, 
			std::string("Field is not indexed: ").append(fname));
}

} //close namespace


