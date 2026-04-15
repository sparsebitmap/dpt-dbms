
#include "stdafx.h"

#include "fieldinfo.h"

//Utils
//API Tiers
#include "dbctxt.h"
#include "dbf_field.h"
#include "dbfile.h"

namespace dpt {

//**********************************************************************************
FieldAttributeCursor::FieldAttributeCursor(DatabaseFileContext* c) 
: DBCursor(c), Destroyable(IndirectDestroy), context(c), ix(-1)
{}

//**********************************************************************************
void FieldAttributeCursor_Single::CacheContextFieldAtts()
{
	DatabaseFileFieldManager* fm = context->GetDBFile()->GetFieldMgr();
	fm->TakeFieldAttsTableCopy(context, &cached_field_table);
	needs_reposition = false;
}

//**********************************************************************************
FieldAttributeCursor_Single::FieldAttributeCursor_Single
(SingleDatabaseFileContext* c, bool f)
: FieldAttributeCursor(c), context(c)
{
	CacheContextFieldAtts();
	if (f)
		GotoFirst();
}

//**********************************************************************************
void FieldAttributeCursor_Group::CacheContextFieldAtts()
{
	cached_field_table.clear();

	//Make a pretend PFI with dummy field ID and btree root
	std::map<std::string, FieldAttributes>::const_iterator i;
	for (i = context->group_field_table.begin(); i != context->group_field_table.end(); i++)
		cached_field_table.push_back(PhysicalFieldInfo(i->first, i->second, -1, -1));

	needs_reposition = false;
}

//**********************************************************************************
FieldAttributeCursor_Group::FieldAttributeCursor_Group
(GroupDatabaseFileContext* c, bool f) 
: FieldAttributeCursor(c), context(c)
{
	CacheContextFieldAtts();
	if (f)
		GotoFirst();
}

//**********************************************************************************
void FieldAttributeCursor::GotoFirst() 
{
	if (NumFields() == 0)
		ix = -1;
	else
		ix = 0;
}

//**********************************************************************************
void FieldAttributeCursor::GotoLast() 
{
	if (NumFields() == 0)
		ix = -1;
	else
		ix = NumFields() - 1;
}

//**********************************************************************************
bool FieldAttributeCursor::Accessible()
{
	//If a field is defined or changed while the cursor exists, just start again.
	//We could fairly easily repositon at the last-used value, but that's overcomplicated.
	if (needs_reposition) {
		CacheContextFieldAtts();
		GotoFirst();
		needs_reposition = false;
	}

	return ix != -1;
}

//**********************************************************************************
void FieldAttributeCursor::Advance(int n)
{
	ix += n;
	if (ix < 0 || ix >= NumFields())
		ix = -1;

	//V2.06 Jul 07.
	PostAdvance(n);
}

//*******************************
const std::string* FieldAttributeCursor::Name()
{
	if (Accessible())
		return &(cached_field_table[ix].name);
	else
		return NULL;
}

//*******************************
const FieldAttributes* FieldAttributeCursor::Atts()
{
	if (Accessible())
		return &(cached_field_table[ix].atts);
	else
		return NULL;
}

//*******************************
const FieldID* FieldAttributeCursor::FID()
{
	if (Accessible())
		return &(cached_field_table[ix].id);
	else
		return NULL;
}


//*****************************************************************************************
//Required for general garbage collection facility
//*****************************************************************************************
void FieldAttributeCursor::IndirectDestroy(Destroyable* pd)
{
	FieldAttributeCursor* obj = static_cast<FieldAttributeCursor*>(pd);
	obj->context->CloseFieldAttCursor(obj);
}

} //close namespace


