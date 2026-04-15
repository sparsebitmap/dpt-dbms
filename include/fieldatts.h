
#if !defined(BB_FIELDATTS)
#define BB_FIELDATTS

#include "apiconst.h"
#include <string>

namespace dpt {

class FieldAttributes {
	static unsigned char FLT;
	static unsigned char INV;
	static unsigned char UAE;
	static unsigned char ORD;
	static unsigned char ONM;
	static unsigned char NM; //V2.14
	static unsigned char BLOB; //V3.0.  NB: One day we may run out of flag byte space!
	                           //       Then maybe the NM flag (not very useful) could be retired before 
	                           //       another byte was needed, which might be a rather fiddly development.

	friend class FieldAttributePage;
	unsigned char flags;
	unsigned char splitpct;

	bool FloatFlag()   const {return (flags & FLT)  ? true : false;}
	bool InvisFlag()   const {return (flags & INV)  ? true : false;}
	bool UAEFlag()     const {return (flags & UAE)  ? true : false;}
	bool OrdFlag()     const {return (flags & ORD)  ? true : false;}
	bool OrdNumFlag()  const {return (flags & ONM)  ? true : false;}
	bool NoMergeFlag() const {return (flags & NM)   ? true : false;}
	bool BLOBFlag()    const {return (flags & BLOB) ? true : false;}

	//Mar 07.  For use by the API.
	friend class APIFieldAttributes;
	int refcount;

public:
	//V2.06 Jun 07.  Added a constructor which uses symbolic constants
	FieldAttributes(FieldStorageFormat = FDEF_STRING, FieldBtreeType = FDEF_NON_ORDERED, 
		FieldStorageUpdatePosition = FDEF_UPDATE_IN_PLACE, unsigned char splitpct = 50, bool nomerge = false);

	FieldAttributes(const FieldAttributes& a) : flags(a.flags), splitpct(a.splitpct), refcount(0) {}
	
	FieldAttributes(bool flt, bool inv, bool uae, bool ord, bool onm, 
						unsigned char s = 50, bool nomerge = false, bool blob = false);

	void RedefineValidityCheck(const FieldAttributes&) const;
	bool GroupConsistencyCheck(const FieldAttributes&) const;
	void ValidityCheck(bool sysgen = false) const;

	bool IsFloat()			const {return (InvisFlag()) ? false : FloatFlag();}
	bool IsString()			const {return (InvisFlag()) ? false : !FloatFlag();}

	bool IsInvisible()		const {return InvisFlag();}
	bool IsVisible()		const {return !InvisFlag();}

	bool IsUpdateAtEnd()	const {return (InvisFlag()) ? false : UAEFlag();}
	bool IsUpdateInPlace()	const {return (InvisFlag()) ? false : !UAEFlag();}

	bool IsOrdered()		const {return OrdFlag();}

	bool IsOrdNum()			const {return (OrdFlag()) ? OrdNumFlag() : false;}
	bool IsOrdChar()		const {return (OrdFlag()) ? !OrdNumFlag() : false;}

	bool IsNoMerge()		const {return NoMergeFlag();}
	bool IsBLOB()			const {return BLOBFlag();}

	const unsigned char Flags() const {return flags;}
	const unsigned char Splitpct() const {return splitpct;}

	//-----------------------------------------------
	//These are really only intended for use during a REDEFINE sequence.  During
	//DEFINE of new fields use the multi-bool or symbolic constant constructors above.
	void SetFloatFlag() {flags |= FLT;}
	void ClearFloatFlag() {flags &= ~FLT;}
	void SetInvisibleFlag() {flags |= INV;}
	void ClearInvisibleFlag() {flags &= ~INV;}
	void SetUpdateAtEndFlag() {flags |= UAE;}
	void ClearUpdateAtEndFlag() {flags &= ~UAE;}
	void SetOrderedFlag() {flags |= ORD;}
	void ClearOrderedFlag() {flags &= ~ORD;}
	void SetOrdNumFlag() {flags |= ONM;}
	void ClearOrdNumFlag() {flags &= ~ONM;}
	void SetNoMergeFlag() {flags |= NM;}
	void ClearNoMergeFlag() {flags &= ~NM;}
	void SetBLOBFlag() {flags |= BLOB;}
	void ClearBLOBFlag() {flags &= ~BLOB;}
	void SetSplitPct(unsigned char s) {splitpct = s;}
};

//*********************************************************************
//Jul 09, in prep for V3.0.  Factored this out from the command.
class FieldAttsParser {
	static void BadParse(const std::string& msg);

protected:
	bool opt_float;
	bool opt_string;
	bool opt_visible;
	bool opt_invisible;
	bool opt_up;
	bool opt_ue;
	bool opt_ordered;
	bool opt_non_ordered;
	bool opt_ordnum;
	bool opt_ordchar;
	bool opt_no_merge; //V2.14 Jan 09.
	bool opt_du_merge;
	bool opt_blob;     //V3.0 Nov 10.
	bool opt_noblob;
	int opt_splitpct;

public:
	FieldAttsParser() 
		: opt_float(false), opt_string(false), opt_visible(false), opt_invisible(false), 
			opt_up(false), opt_ue(false), 
			opt_ordered(false), opt_non_ordered(false), 
			opt_ordnum(false), opt_ordchar(false),
			opt_no_merge(false), opt_du_merge(false),
			opt_blob(false), opt_noblob(false), opt_splitpct(-1)
	{}

	void ParseFieldAttributes(const std::string&, int, std::string* = NULL);

	FieldAttributes MakeAtts();
};

} //close namespace

#endif
