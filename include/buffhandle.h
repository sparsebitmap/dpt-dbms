/****************************************************************************************
This helps ensure that buffer pages are always released correctly.
****************************************************************************************/

#ifndef BB_BUFFHANDLE
#define BB_BUFFHANDLE

namespace dpt {

class BufferPage;
class RawPageData;
class BufferedFileInterface;
class DatabaseServices;
class DatabaseFilePage;
class FCTPage;
class LabelDefinitionWithBufferPage;

//***************************************************************************************
class BufferPageHandle {

	friend class DatabaseFilePage;
	BufferPage* buff;
	DatabaseServices* dbapi;
	mutable bool enabled;  //ugly but saves recoding std::vector::insert

	//Assignment: the asignee becomes responsible for releasing the buffer page
	void Copy(const BufferPageHandle&);

	//Normally algorithms will require table D or B relative pages, so force them
	//through the FCT and LPM to avoid using absolute file page # accidentally.
	friend class FCTPage;
	friend class LabelDefinitionWithBufferPage; //special UL diagnostic class
	BufferPageHandle(DatabaseServices*, BufferedFileInterface*, int, bool = false);

public:
	BufferPageHandle() : enabled(false) {}
	BufferPageHandle(const BufferPageHandle& hfrom) : enabled(false) {Copy(hfrom);}
	void operator=(const BufferPageHandle& hfrom) {Copy(hfrom);}

	DatabaseServices* DBAPI() {return dbapi;}

	void Release();
	~BufferPageHandle() {Release();}

	bool IsEnabled() {return enabled;}

	//Buffer stats.  Only used for some types.
	void BuffAPINoteFreshFormattedPage(char);
};

} //close namespace

#endif
