
#include "stdafx.h"

#include "dbapi\fieldatts.h"
#include "fieldatts.h"

namespace dpt {

APIFieldAttributes::APIFieldAttributes(const FieldAttributes* t)
: target(new FieldAttributes)
{
	target->refcount = 1;
	if (t)
		*target = *t;
}

APIFieldAttributes::APIFieldAttributes(const APIFieldAttributes& from)
{
	//Assignment or or copy constructor
	target = from.target;
	if (target)
		target->refcount++;
}

APIFieldAttributes::APIFieldAttributes
(FieldStorageFormat storfmt, FieldBtreeType bttype, 
 FieldStorageUpdatePosition storupos, unsigned char s, bool nomerge)
: target(new FieldAttributes(storfmt, bttype, storupos, s, nomerge))
{
	target->refcount = 1;
}

APIFieldAttributes::APIFieldAttributes
(bool flt, bool inv, bool uae, bool ord, bool onm, unsigned char s, bool nomerge, bool blob)
: target(new FieldAttributes(flt, inv, uae, ord, onm, s, nomerge, blob))
{
	target->refcount = 1;
}

APIFieldAttributes::~APIFieldAttributes()
{
	if (target) {
		target->refcount--;
		if (target->refcount == 0)
			delete target;
	}
}

//**********************************************
bool APIFieldAttributes::IsFloat()			const {return target->IsFloat();}
bool APIFieldAttributes::IsString()			const {return target->IsString();}
bool APIFieldAttributes::IsInvisible()		const {return target->IsInvisible();}
bool APIFieldAttributes::IsVisible()		const {return target->IsVisible();}
bool APIFieldAttributes::IsBLOB()			const {return target->IsBLOB();}
bool APIFieldAttributes::IsUpdateAtEnd()	const {return target->IsUpdateAtEnd();}
bool APIFieldAttributes::IsUpdateInPlace()	const {return target->IsUpdateInPlace();}
bool APIFieldAttributes::IsOrdered()		const {return target->IsOrdered();}
bool APIFieldAttributes::IsOrdNum()			const {return target->IsOrdNum();}
bool APIFieldAttributes::IsOrdChar()		const {return target->IsOrdChar();}
bool APIFieldAttributes::IsNoMerge()		const {return target->IsNoMerge();}
unsigned char APIFieldAttributes::Splitpct() const {return target->Splitpct();}

//**********************************************
void APIFieldAttributes::SetFloatFlag() {target->SetFloatFlag();}
void APIFieldAttributes::ClearFloatFlag() {target->ClearFloatFlag();}

void APIFieldAttributes::SetInvisibleFlag() {target->SetInvisibleFlag();}
void APIFieldAttributes::ClearInvisibleFlag() {target->ClearInvisibleFlag();}

void APIFieldAttributes::SetBLOBFlag() {target->SetBLOBFlag();}
void APIFieldAttributes::ClearBLOBFlag() {target->ClearBLOBFlag();}

void APIFieldAttributes::SetUpdateAtEndFlag() {target->SetUpdateAtEndFlag();}
void APIFieldAttributes::ClearUpdateAtEndFlag() {target->ClearUpdateAtEndFlag();}

void APIFieldAttributes::SetOrderedFlag() {target->SetOrderedFlag();}
void APIFieldAttributes::ClearOrderedFlag() {target->ClearOrderedFlag();}

void APIFieldAttributes::SetOrdNumFlag() {target->SetOrdNumFlag();}
void APIFieldAttributes::ClearOrdNumFlag() {target->ClearOrdNumFlag();}

void APIFieldAttributes::SetNoMergeFlag() {target->SetNoMergeFlag();}
void APIFieldAttributes::ClearNoMergeFlag() {target->ClearNoMergeFlag();}

void APIFieldAttributes::SetSplitPct(unsigned char s) {target->SetSplitPct(s);}


} //close namespace


