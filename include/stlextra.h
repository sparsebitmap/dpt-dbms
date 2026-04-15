
#if !defined(BB_STLEXTRA)
#define BB_STLEXTRA

#include <functional>
#include <string>

namespace dpt {

//NB zeros not handled, but OK for most things like file paths
struct NoCaseSortPredicate : public std::binary_function<std::string, std::string, bool> 
{
	bool operator()(const std::string& lhs, const std::string& rhs) 
	{
		return (stricmp(lhs.c_str(), rhs.c_str()) < 0);
	}
};

//****************************
//String helper function
size_t FindSubstringInRange
	(const std::string& s, const std::string& sub, size_t startcol, size_t numcols);

//****************************
//Wasn't sure where to put this - here's OK for the time being.
//Compare buffers of differing lengths.  Saves strlen() calls, or allows zeros in the data.
//Seem to find myself writing this code quite a lot so hopefully this will be the last time.
class VarLenMemCmp {
	static bool CmpEQ(int c) {return (c == 0);}
	static bool CmpNE(int c) {return (c != 0);}
	static bool CmpLT(int c) {return (c < 0);}
	static bool CmpGT(int c) {return (c > 0);}

	static bool DiffLen(int l1, int l2) {return (l1 != l2);}
	static bool SameLen(int l1, int l2) {return (l1 == l2);}
	static bool Shorter(int l1, int l2) {return (l1  < l2);}
	static bool Longer(int l1, int l2)  {return (l1  > l2);}

	static int Cmp(const char* s1, const char* s2, int l) {
		return memcmp(s1, s2, l);}

	//Don't run past the end of either buffer
	static int PartCmp(const char* s1, int l1, const char* s2, int l2) {
		int shortest = (l1 < l2) ? l1 : l2;
		return Cmp(s1, s2, shortest);}

public:

	static bool LT(const char* s1, int l1, const char* s2, int l2) {
		int c = PartCmp(s1,l1,s2,l2); 
		return( CmpLT(c) || ( CmpEQ(c) && Shorter(l1,l2) ));}

	static bool GT(const char* s1, int l1, const char* s2, int l2) {
		int c = PartCmp(s1,l1,s2,l2); 
		return( CmpGT(c) || ( CmpEQ(c) && Longer(l1,l2) ));}
	
	static bool EQ(const char* s1, int l1, const char* s2, int l2) {
		return( SameLen(l1, l2) && CmpEQ(Cmp(s1,s2,l2)) );}
	
	static bool NE(const char* s1, int l1, const char* s2, int l2) {
		return( DiffLen(l1, l2) || CmpNE(Cmp(s1,s2,l2)) );}
};

}

#endif
