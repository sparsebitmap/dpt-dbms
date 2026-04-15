
#if !defined(BB_API_FIELDATTS)
#define BB_API_FIELDATTS

#include "apiconst.h"

//V3.0.  BLOB flag: optional parameters and functions added below.

namespace dpt {

class FieldAttributes;

class APIFieldAttributes {
public:
	FieldAttributes* target;
	APIFieldAttributes(const FieldAttributes*);
	APIFieldAttributes(const APIFieldAttributes&);
	~APIFieldAttributes();
	//-----------------------------------------------------------------------------------

	//Alternate constructors - first one is more readable
	APIFieldAttributes(FieldStorageFormat = FDEF_STRING, FieldBtreeType = FDEF_NON_ORDERED, 
		FieldStorageUpdatePosition = FDEF_UPDATE_IN_PLACE, unsigned char splitpct = 50, bool nomerge = false);
	APIFieldAttributes(bool flt, bool inv, bool uae, 
		bool ord, bool onm, unsigned char s = 50, bool nomerge = false, bool blob = false);

	bool IsFloat()			const;
	bool IsString()			const;
	bool IsInvisible()		const;
	bool IsVisible()		const;
	bool IsBLOB()			const;
	bool IsUpdateAtEnd()	const;
	bool IsUpdateInPlace()	const;
	bool IsOrdered()		const;
	bool IsOrdNum()			const;
	bool IsOrdChar()		const;
	bool IsNoMerge()		const;
	unsigned char Splitpct() const;

	//-----------------------------------------------
	//These are really only intended for use during a REDEFINE sequence.  During
	//DEFINE of new fields use the multi-parameter constructor above.
	void SetFloatFlag();
	void ClearFloatFlag();
	void SetInvisibleFlag();
	void ClearInvisibleFlag();
	void SetBLOBFlag();
	void ClearBLOBFlag();
	void SetUpdateAtEndFlag();
	void ClearUpdateAtEndFlag();
	void SetOrderedFlag();
	void ClearOrderedFlag();
	void SetOrdNumFlag();
	void ClearOrdNumFlag();
	void SetNoMergeFlag();
	void ClearNoMergeFlag();
	void SetSplitPct(unsigned char);
};

} //close namespace

#endif
