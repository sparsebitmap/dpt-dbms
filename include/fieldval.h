/****************************************************************************************
A kind of rudimentary variant object, created when retrieving values from records
and also to help reduce the number of API functions for things like finding and changing 
records.  The user can specify either a number or a string.
See API docs for more discussion/background about this class.
****************************************************************************************/

#if !defined(BB_FIELDVAL)
#define BB_FIELDVAL

#include "bbfloat.h"
#include "assert.h"
#include <functional>
#include "apiconst.h"

namespace dpt {

//see 0xF0 test
#define FV_DATA_SIZE     16
#define FV_MAX_LOCAL_LEN FV_DATA_SIZE-1
#define FV_CTRLA         FV_DATA_SIZE-1
#define FV_CTRLB         FV_DATA_SIZE-2
//only used for numerics - see SpecialType
#define FV_CTRLC         FV_DATA_SIZE-3 
//BLOB descriptor flag
#define FV_CTRLD         FV_DATA_SIZE-4 

#define FV_TYPE_DBL      'D'
#define FV_TYPE_LOCAL    'L'
#define FV_TYPE_HEAP     'H'

//V3.0 BLOBs
//bbbbllll------*8
#define FV_TYPE_BLOB     'B'
#define FV_BLOBINFO_LEN  8
#define FV_BLOBLENPOS    0
#define FV_BLOBDATAPOS   4
#define FV_BLOBDESC_LEN  10

//V2.27 Allow null C string to mean "" as a convenience
#ifdef DPT_BUILDING_CAPI_DLL
#define NULLCHK if (!cs) return;
#else
#define NULLCHK ;		
#endif

//********************************************************************************
class FieldValue {
	unsigned char  data[FV_DATA_SIZE];

			RoundedDouble*	 PRoundedDouble()			{return (RoundedDouble*)(data);}
			char*			 PLocalChars()				{return (char*)(data);}
			char**			 PHeapChars()				{return (char**)(data);}
	const	RoundedDouble*	CPRoundedDouble()	const	{return (const RoundedDouble*)(data);}
	const	char*			CPLocalChars()		const	{return (const char*)(data);}
	const	char**			CPHeapChars()		const	{return (const char**)(data);}

	//V3.0. BLOBs.
	char**					 PBLOBChars()				{return (char**)(data + FV_BLOBDATAPOS);}
	int*					 PBLOBLen()					{return (int*)(data + FV_BLOBLENPOS);}
	const	char**			CPBLOBChars()		const	{return (const char**)(data + FV_BLOBDATAPOS);}
	const	int*			CPBLOBLen()			const	{return (const int*)(data + FV_BLOBLENPOS);}

	int*					 PBLOBDescPage()			{return (int*)(data);}
	short*					 PBLOBDescSlot()			{return (short*)(data+4);}
	int*					 PBLOBDescLen()				{return (int*)(data+6);}
	const	int*			CPBLOBDescPage()	const	{return (const int*)(data);}
	const	short*			CPBLOBDescSlot()	const	{return (const short*)(data+4);}
	const	int*			CPBLOBDescLen()		const	{return (const int*)(data+6);}

	        int*             PCtrlAtoD()        {return (int*)(data + FV_CTRLD);}
			unsigned char&		 CtrlA()		{return data[FV_CTRLA];}
			unsigned char&		 CtrlB()		{return data[FV_CTRLB];}
			unsigned char&		 CtrlD()		{return data[FV_CTRLD];}
	const	unsigned char&		CCtrlA() const	{return data[FV_CTRLA];}
	const	unsigned char&		CCtrlB() const	{return data[FV_CTRLB];}
	const	unsigned char&		CCtrlD() const	{return data[FV_CTRLD];}

protected:
			unsigned char&		 CtrlC()		{return data[FV_CTRLC];}
	const	unsigned char&		CCtrlC() const	{return data[FV_CTRLC];}

private:
	inline const char Type() const {
							unsigned char locallen = CCtrlA();
							if (locallen == FV_BLOBINFO_LEN && CCtrlB() == 0xFF) //V3.0
								return FV_TYPE_BLOB;
							if (locallen <= FV_MAX_LOCAL_LEN) 
								return FV_TYPE_LOCAL;
							if (CCtrlB())		//nonzero in last-but-one byte
								 return FV_TYPE_DBL;
							return FV_TYPE_HEAP;}


	const void SameNumericType(const char t1, const char t2) const {
#ifdef _DEBUG
								assert ((t1 == 'D') == (t2 == 'D'));
#endif
	}

	char* PChars(const unsigned char t) {
							if (t == FV_TYPE_LOCAL)
								return PLocalChars();
							if (t == FV_TYPE_BLOB)     //V3.0
								return *PBLOBChars();
							return *PHeapChars();}
	const char* CPChars(const unsigned char t) const {
							if (t == FV_TYPE_LOCAL)
								return CPLocalChars();
							if (t == FV_TYPE_BLOB)     //V3.0
								return *CPBLOBChars();
							return *CPHeapChars();}
	
	const char* LocalMaxLenCstrFudge(); //V3.0

	//--------------------------------------------
	void Destroy() {
						const char type = Type();
						if (type == FV_TYPE_HEAP) 
							delete[] *PHeapChars();
						else if (type == FV_TYPE_BLOB) //V3.0
							delete [] *PBLOBChars();
						else if (any_cstr_fudges && type == FV_TYPE_LOCAL && CCtrlA() == FV_MAX_LOCAL_LEN) //V3.0
							CstrFudgeCleanup();
						//CtrlA() = 0; CtrlB() = 0; } //V3.0. Be extra-tidy and save a couple of instructions.
						*PCtrlAtoD() = 0;}
	
	//See comments in cpp file.
	void CstrFudgeCleanup();
	static bool any_cstr_fudges;

	void ThrowTooLong(const char*, const char*) const;
	void TooLong(const char*, const char* = NULL);

	//V3.0. Allows separated and more efficient populate for BLOBs
	void AllocateAndMaybePopulate(unsigned int slen, const char* s) {
						if (slen > 255) {
							//TooLong(s); //V3.0: OK now
							*PBLOBChars() = new char[slen + 1]; //allow for later zt in Cstr()
							CtrlA() = FV_BLOBINFO_LEN; CtrlB() = 0xFF;
							*PBLOBLen() = slen;
							if (s) memcpy(*PBLOBChars(), s, slen);
						}
						else if (slen <= FV_MAX_LOCAL_LEN) {
							CtrlA() = slen;
							if (slen && s)
								memcpy(PLocalChars(), s, slen);
							if (slen != FV_MAX_LOCAL_LEN) //V3.0. to distinguish BLOBs
								CtrlB() = 0;}
						else {
							*PHeapChars() = new char[slen + 1];//allow for later zt in Cstr()
							CtrlA() = slen;
							CtrlB() = 0;
							if (s) memcpy(*PHeapChars(), s, slen);}}

	//Note that the data can contain zeros
	void CopyInChars(const char* s, unsigned int slen) {AllocateAndMaybePopulate(slen, s);}
	void Allocate(int len) {AllocateAndMaybePopulate(len, NULL);}

	//Used on btree nodes or anywhere prefix compression is used (currently nowhere else)
	void CopyInChars(const char* pref, unsigned int preflen,
							const char* cs, unsigned int cslen) {
						unsigned int totlen = preflen + cslen;
						if (totlen > 255)
							TooLong(pref, cs);
						if (totlen <= FV_MAX_LOCAL_LEN) {
							CtrlA() = totlen;
							if (preflen)
								memcpy(PLocalChars(), pref, preflen);
							if (cslen)
								memcpy(PLocalChars() + preflen, cs, cslen);
							if (totlen != FV_MAX_LOCAL_LEN) //V3.0. for BLOBs.
								CtrlB() = 0;}
						else {
							*PHeapChars() = new char[totlen + 1];//allow for later zt in Cstr()
							CtrlA() = totlen;
							CtrlB() = 0;
							if (preflen)
								memcpy(*PHeapChars(), pref, preflen);
							if (cslen)
								memcpy(*PHeapChars() + preflen, cs, cslen);}}

	void CopyInNumber(const RoundedDouble& d) {
						*PRoundedDouble() = d;
						CtrlA() = 0xFF; CtrlB() = 0xFF; CtrlC() = 0x00;}

	void CopyInFieldValue(const FieldValue& rhs) {
						const char rt = rhs.Type();
						if (rt == FV_TYPE_DBL) {
							CopyInNumber(*(rhs.CPRoundedDouble())); CtrlC() = rhs.CCtrlC();}
						else 
							CopyInChars(rhs.CPChars(rt), rhs.StrLen());}

public:
	//Use a default constructor that gives behaviour close to the common convention on 
	//M204 of missing fields showing "" as string, not "0.0".
	FieldValue()							{CtrlA() = 0; CtrlB() = 0;}

	FieldValue(const char* cs)				{CtrlA() = 0; NULLCHK CopyInChars(cs, strlen(cs));}
	FieldValue(const char* cs, int len)		{CtrlA() = 0; NULLCHK CopyInChars(cs, len);}
	FieldValue(const std::string& s)		{CtrlA() = 0; CopyInChars(s.c_str(), s.length());}
	FieldValue(const int& i)				{CtrlA() = 0; CopyInNumber(i);}
	FieldValue(const double& d)				{CtrlA() = 0; CopyInNumber(d);}
	FieldValue(const RoundedDouble& rd)		{CtrlA() = 0; CopyInNumber(rd);}
	FieldValue(const FieldValue& fv)		{CtrlA() = 0; CopyInFieldValue(fv);}
	~FieldValue() {Destroy();}

	void operator=(const char* cs)			{Destroy(); NULLCHK CopyInChars(cs, strlen(cs));}
	void operator=(const std::string& s)	{Destroy(); CopyInChars(s.c_str(), s.length());}
	void operator=(const int& i)			{Destroy(); CopyInNumber(i);}
	void operator=(const double& d)			{Destroy(); CopyInNumber(d);}
	void operator=(const RoundedDouble& rd)	{Destroy(); CopyInNumber(rd);}
	void operator=(const FieldValue& fv)	{Destroy(); CopyInFieldValue(fv);}
	//V2.10 - Dec 07.  Alternative names for the above (easier SWIG/Python interfaces).
	void Assign(const char* cs)				{Destroy(); NULLCHK CopyInChars(cs, strlen(cs));}
	void Assign(const std::string& s)		{Destroy(); CopyInChars(s.c_str(), s.length());}
	void Assign(const int& i)				{Destroy(); CopyInNumber(i);}
	void Assign(const double& d)			{Destroy(); CopyInNumber(d);}
	void Assign(const RoundedDouble& rd)	{Destroy(); CopyInNumber(rd);}
	void Assign(const FieldValue& fv)		{Destroy(); CopyInFieldValue(fv);}

	//From raw memory (V2.10 name change for clarity - see above)
	void AssignData(const char* s, unsigned int i)	{Destroy(); CopyInChars(s, i);}
	void AssignData(const char* p, unsigned int pl, const char* cs, unsigned int csl)	{
		Destroy(); CopyInChars(p, pl, cs, csl);}
	FieldValue(const char* s, unsigned int i)	{CtrlA() = 0; CopyInChars(s, i);}

	//Used in sort sometimes
	void Swap(FieldValue& rhs) {
						char buff[FV_DATA_SIZE]; 
						memcpy(buff, data, FV_DATA_SIZE);
						memcpy(data, rhs.data, FV_DATA_SIZE);
						memcpy(rhs.data, buff, FV_DATA_SIZE);}

	//----------------------------------
	void ConvertToString() {
						if (CurrentlyString()) 
							return; 	
						char buff[128]; 
						int slen = PRoundedDouble()->ToStringBufferForDisplay(buff);
						Destroy(); 
						CopyInChars(buff, slen);}
	void ConvertToNumeric(bool throw_non_num = false) {
						const char t = Type();
						if (t == FV_TYPE_DBL) 
							return; 
						try {operator=(RoundedDouble(std::string(CPChars(t), StrLen())));}
						catch (...) {if (throw_non_num) throw; 
							operator=(0);}}

	//----------------------------------
	//Numeric extractors
	RoundedDouble ExtractRoundedDouble(bool throw_non_num = false) const {
						const char t = Type();
						if (t == FV_TYPE_DBL)
							return *CPRoundedDouble();
						try { 
							return std::string(CPChars(t), StrLen());}
						catch (...) {if (throw_non_num) throw; 
							return 0;}};

	bool CurrentlyNumeric() const {return Type() == FV_TYPE_DBL;}
	bool CurrentlyString() const {return !CurrentlyNumeric();}
	const RoundedDouble* RDData() const {return CPRoundedDouble();}

	//----------------------------------
	//String/char extractors
	//const unsigned char StrLen() const {			//current length
	const int StrLen() const {						//V3.0. BLOB values can now be up to 2G
					assert(CurrentlyString());
					if (Type() == FV_TYPE_BLOB) return *CPBLOBLen(); 
					return CCtrlA();}

	const char* StrChars() const {					//NB no terminating zero
					assert(CurrentlyString());
					return CPChars(Type());}

	const char* CStr() {                            //Fudge in a hidden zero terminator
					assert(CurrentlyString());
					int curlen = StrLen();
					if (curlen == FV_MAX_LOCAL_LEN)
						return LocalMaxLenCstrFudge();                        //oops, no room
					const char type = Type();
					if (type == FV_TYPE_LOCAL) {
						PLocalChars()[curlen] = '\0'; return CPLocalChars();} //local buff OK
					PChars(type)[curlen] = '\0'; return CPChars(type);}       //heap or blob buff OK

	std::string ExtractString() const {				//Object can hold any data type
						const char t = Type();
						if (t == FV_TYPE_DBL)
							return CPRoundedDouble()->ToStringForDisplay();
						return std::string(CPChars(t), StrLen());}
	std::string ExtractAbbreviatedString(int max = 30) const {
						const char t = Type();
						if (t == FV_TYPE_DBL)
							return CPRoundedDouble()->ToStringForDisplay();
						int len = StrLen();
						if (len <= max) 
							return std::string(CPChars(t), len);
						return std::string(CPChars(t), max).append("...");}

	void CheckStrLen255(const char* r = "stored in non-BLOB fields") const {
							if (StrLen() > 255) ThrowTooLong(StrChars(), r);}

	//V3.0.  Allow direct memcpy into str FieldValue, perhaps saving intermediate buffer work.
	char* AllocateBuffer(int len) {Allocate(len); return PChars(Type());}

#ifdef _DEBUG
	const char* DebuggerExtractString() const; 
#endif

	//-----------------------------------
	//Comparisons - only to be used when the type is known to be the same.
	bool operator==(const FieldValue& rhs) const {
						const char t = Type();
						SameNumericType(t, rhs.Type());
						if (t == FV_TYPE_DBL)
							return *CPRoundedDouble() == *(rhs.CPRoundedDouble());
						int slen = StrLen();
						if (slen != rhs.StrLen())
							return false;
						return (memcmp(CPChars(t), rhs.CPChars(t), slen) == 0);}

	int Compare(const FieldValue& rhs) const {
						const char t = Type();
						SameNumericType(t, rhs.Type());
						if (t == FV_TYPE_DBL)
							return CompareNumeric(rhs);
						else
							return CompareString(rhs);}

	int CompareNumeric(const FieldValue& rhs) const {
						assert(Type() == FV_TYPE_DBL);
						if (*CPRoundedDouble() == *(rhs.CPRoundedDouble()))
							return 0;
						if (*CPRoundedDouble() < *(rhs.CPRoundedDouble()))
							return -1;
						return 1;}

	int CompareString(const FieldValue& rhs) const {
						const char t = Type(); const char rt = rhs.Type();
						assert(Type() != FV_TYPE_DBL);
//						unsigned char len = StrLen(); unsigned char rlen = rhs.StrLen(); //V3.0
//						unsigned char shortest = (len < rlen) ? len : rlen;
						int len = StrLen(); int rlen = rhs.StrLen();
						int shortest = (len < rlen) ? len : rlen;
						int cmp = memcmp(CPChars(t), rhs.CPChars(rt), shortest);
						if (cmp != 0)
							return cmp;
						if (len == rlen)
							return 0;
						if (len < rlen)
							return -1; //shortest sorts lower if they match
						return 1;} 

	int CompareNoCaseString(const FieldValue& rhs) const;
	
	int CompareRightJustifiedString(const FieldValue& rhs) const {
						const char t = Type(); const char rt = rhs.Type();
						assert(Type() != FV_TYPE_DBL);
//						unsigned char len = StrLen(); unsigned char rlen = rhs.StrLen(); //V3.0
						int len = StrLen(); int rlen = rhs.StrLen();
						if (len < rlen)
							return -1; //shorter sorts lower regardless of contents
						if (len > rlen)
							return 1;
						return memcmp(CPChars(t), rhs.CPChars(rt), len);}

	//Operator < for std::sort (now see predicate below)
//	bool operator<(const FieldValue& rhs) {return Compare(rhs) < 0;}

	//-----------------------------------
	//String modifiers
	void StrTruncate(unsigned int reqdlen) {
						assert(CurrentlyString());
						unsigned int curlen = StrLen();
						if (reqdlen >= curlen) //don't extend
							return;
						const char type = Type(); //truncate within current storge format
						if (type == FV_TYPE_LOCAL) {
							CtrlA() = reqdlen; return;}
						if (type == FV_TYPE_HEAP && reqdlen > FV_MAX_LOCAL_LEN) {
							CtrlA() = reqdlen; return;}
						if (type == FV_TYPE_BLOB && reqdlen > 255) {
							*PBLOBLen() = reqdlen; return;}
						//char curbuff[255]; //changing storage format
						std::string s = std::string(StrChars(), reqdlen); //V3.0
						Destroy();
						Assign(s);}

	//-----------------------------------
	char SpecialType() {return (CurrentlyNumeric()) ? CtrlC() : 0;}
	
	//V3.0. BLOB descriptors.  Mainly for internal use.
	void MakeBLOBDescriptor(int epage = -1, short eslot = -1, int len = 0) {
						Destroy(); CtrlA() = FV_BLOBDESC_LEN; SetBLOBDescriptorFlag();
						*PBLOBDescPage() = epage; *PBLOBDescSlot() = eslot; *PBLOBDescLen() = len;}
	void MakeBLOBDescriptor(const char* tablebptr) {
						Destroy(); CtrlA() = FV_BLOBDESC_LEN; SetBLOBDescriptorFlag();
						memcpy(data, tablebptr, FV_BLOBDESC_LEN);}
	
	void SetBLOBDescriptorFlag() {CtrlD() = 'B';}
	bool IsBLOBDescriptor() const {return (Type() == FV_TYPE_LOCAL &&
											StrLen() == FV_BLOBDESC_LEN && CCtrlD() == 'B');}
	
	int BLOBDescTableEPage() const {return *CPBLOBDescPage();}
	short BLOBDescTableESlot() const {return *CPBLOBDescSlot();}
	int BLOBDescLen() const {return *CPBLOBDescLen();}

	//-----------------------------------
	//V2.26
#ifdef _BBHOST
	FieldValue(const std::string2& s) {CtrlA() = 0; CopyInChars(s.c_str(), s.length());}
	void operator=(const std::string2& s) {Destroy(); CopyInChars(s.c_str(), s.length());}
	std::string2 ExtractString2() const {
						const char t = Type();
						if (t == FV_TYPE_DBL)
							return CPRoundedDouble()->ToString2ForDisplay();
						return std::string2(CPChars(t), StrLen());}

#endif

};

//*************************************************************************************
//V2.20.  Special values for miscellaneous use.  
//This makes use of one of the spare 6 bytes in numeric form (see the new CtrlC())
typedef char FVSpecialType;
const FVSpecialType UL_MAGIC_S = 'A';

class SpecialFieldValue : public FieldValue {
public:
	SpecialFieldValue(char t, int i = 0) : FieldValue(i) {CtrlC() = t;}
	SpecialFieldValue(char t, double d) : FieldValue(d) {CtrlC() = t;}
};


//*************************************************************************************
//For std::sort on value sets
//*************************************************************************************
struct FieldValueLessThanPredicate
: public std::binary_function<FieldValue, FieldValue, bool> 
{
	SortType sorttype;
	SortDirection sortdir;

	FieldValueLessThanPredicate(SortType t, SortDirection d) : sorttype(t), sortdir(d) {}
	FieldValueLessThanPredicate(const FieldValueLessThanPredicate& p) 
		: sorttype(p.sorttype), sortdir(p.sortdir) {}

	bool operator()(const FieldValue& lhs, const FieldValue& rhs) {
		int cmp;
		if (sorttype == SORT_NUMERIC)
			cmp = lhs.CompareNumeric(rhs);
		else if (sorttype == SORT_CHARACTER)
			cmp = lhs.CompareString(rhs);
		else if (sorttype == SORT_NOCASE)
			cmp = lhs.CompareNoCaseString(rhs);
		else
			cmp = lhs.CompareRightJustifiedString(rhs);

		if (sortdir == SORT_ASCENDING)
			return (cmp < 0);
		else
			return (cmp > 0);}
};

} //close namespace

#endif
