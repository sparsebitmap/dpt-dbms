
#include "stdafx.h"

#include "pagebase.h"

//Utils
#include "dataconv.h"
//API Tiers
#include "buffmgmt.h"
//Diagnostics
#include "except.h"
#include "msg_db.h"
#include "assert.h"

namespace dpt {

//**************************************************************************************
void DatabaseFilePage::Construct
(DatabaseServices* d, char pagetype, BufferPage* bp, bool fresh)
{	
	buff = bp;
	data = buff->PageData();
	dbapi = d;

	//Fresh pages are zeroized, mainly just to make pages easier to read in a dump 
	if (fresh) {
		SetChars(0, 0, DBPAGE_SIZE);
		SetChars(DBPAGE_PAGETYPE, pagetype, 1);
		
		MakeDirty();
	}

	else {
		char pt = PageType_S();
		if (pagetype != '*' && pagetype != pt) {

			std::string msg;

			//V2.27 When opening a non-DPT file make this message less alarming
			if (pagetype == 'F')
				 msg = "The file is not a DPT database file, or is corrupt";
			else
				 msg = std::string("Probable bug: unexpected page type in file (")
					 .append(1, pt).append(" should be ").append(1, pagetype)
					 .append(" on page ").append(util::IntToString(bp->filepage).append(")"));

			throw Exception(DB_UNEXPECTED_PAGE_TYPE, msg);
		}
	}

#ifdef _DEBUG
	void* debug_data_address = data;
#endif
}

//**************************************************************************************
//This constructor is only used in one or two places, just to map a raw piece of memory.
//**************************************************************************************
void DatabaseFilePage::Construct(char pagetype, RawPageData* rp, bool fresh)
{	
	buff = NULL;
	dbapi = NULL;
	data = rp;

	if (fresh)
		SetChars(DBPAGE_PAGETYPE, pagetype, 1);

	else {
		char pt = PageType_S();
		if (pagetype != '*' && pagetype != PageType_S())
			throw Exception(DB_UNEXPECTED_PAGE_TYPE, std::string
				("Probable bug: unexpected page type in file (").append(1, pt)
				.append(" should be ").append(1, pagetype).append(")"));
	}
}

//**************************************************************************************
void DatabaseFilePage::MakeDirty_S()
{
	assert(buff);
	buff->MakeDirty(dbapi);
}

} //close namespace
