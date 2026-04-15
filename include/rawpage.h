/****************************************************************************************
Some functionality common to both database file pages when they're in buffers and
other instances of 8K pages, such as found sets.
****************************************************************************************/

#ifndef BB_RAWPAGE
#define BB_RAWPAGE

#include <string>

namespace dpt {

const int DBPAGE_SIZE = 8192;

class RawPageData {
	char bytes[DBPAGE_SIZE];

	//* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
	//NB. No virtual functions or other data members must be put in this class,
	//as the size is relied upon for many addressing and file IO purposes.
	//* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *

	void CopyFrom(const RawPageData* p) {memcpy(bytes, p->bytes, DBPAGE_SIZE);}

public:
	RawPageData(const RawPageData* p) {CopyFrom(p);}
	RawPageData& operator=(const RawPageData* p) {CopyFrom(p); return *this;}
	RawPageData& operator=(const RawPageData& p) {CopyFrom(&p); return *this;}

	char* CharData() {return &bytes[0];}
};

//*****************************************************************************
struct AlignedPage {
	void* data;

	AlignedPage(const char* = NULL);
	~AlignedPage();

	RawPageData* GetRawPageData() {return reinterpret_cast<RawPageData*>(data);}
};


} //close namespace

#endif
