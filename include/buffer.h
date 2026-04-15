
//****************************************************************************************
//Specialized types of character buffer
//****************************************************************************************

#ifndef BB_BUFFER
#define BB_BUFFER

#include <vector>

namespace dpt {
namespace util {

//For when a probably-large buffer is added to in unpredictably-sized, but probably large 
//chunks, and then accessed in different-sized pieces.  This cuts out all the moving around 
//of memory during enlargement.  The cost is that the buffer can't be used in any way that
//would assume the data was stored contiguously.  Using this for fast load of table B.
class RechunkingBuffer {

	//General control
	std::vector<char*> data;
	std::vector<int> datalens;
	int total_len;

	//Special control of extension in small pieces (auto-extends in large chunks)
	int item_append_chunkid;
	int item_append_chunk_avail;
	char* item_append_ptr;
	int* item_append_len;

	//Extraction control
	int extraction_pos;
	int extraction_currchunk;
	int extraction_chunkpos;
	void AdvanceExtractChunk() {extraction_currchunk++; extraction_chunkpos = 0;}

	char* AppendChunk_S(char*, int, bool adopt = false);

public:
	RechunkingBuffer() {Clear();}
	virtual ~RechunkingBuffer() {Clear();}

	void Clear();
	int TotalLen() {return total_len;}

	//Build in large chunks
	char* AllocateChunk(int len) {return AppendChunk_S(NULL, len);}
	char* AppendChunk(char* source, int len) {return AppendChunk_S(source, len, false);}
	char* AdoptChunk(char* adoptee, int len) {return AppendChunk_S(adoptee, len, true);}

	//Build in small chunks (local array is much faster but this is OK where we want to 
	//share code with large-chunk cases, and where the latter are the most important).
	char* AppendItem(const void*, unsigned short);

	void Append(RechunkingBuffer&);

	int Extract(void*, int);
	void Advance(int n) {Extract(NULL, n);}
	void ResetExtraction() {extraction_pos = 0; extraction_currchunk = 0; extraction_chunkpos = 0;}
	int BytesExtracted() {return extraction_pos;}
	int RemainingToExtract() {return total_len - extraction_pos;}
};
	
}} //close namespace

#endif
