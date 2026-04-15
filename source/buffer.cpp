
#include "stdafx.h"

#include "buffer.h"

namespace dpt { namespace util {

//******************************
char* RechunkingBuffer::AppendChunk_S(char* source, int len, bool adopt)
{
	if (adopt)
		data.push_back(source);
	else {
		data.push_back(NULL);
		data.back() = new char[len];
		if (source)
			memcpy(data.back(), source, len);
	}

	datalens.push_back(len);
	total_len += len;

	return data.back();
}

//******************************
void RechunkingBuffer::Clear()
{
	for (size_t x = 0; x < data.size(); x++) {
		if (data[x])
			delete[] data[x];
	}

	data.clear();
	datalens.clear();

	total_len = 0;
	ResetExtraction();

	item_append_chunkid = -1;
	item_append_chunk_avail = -1;
}

//******************************
int RechunkingBuffer::Extract(void* vdest, int reqd_len)
{
	char* dest = (char*) vdest;

	int remaining_len = RemainingToExtract();
	if (remaining_len == 0)
		return 0;

	int this_extract_len = (remaining_len > reqd_len) ? reqd_len : remaining_len;

	//Now the tricky part - we may have to take pieces from more than one chunk
	int all_chunks_taken_len = 0;
	for (;;) {
		int chunk_remaining = datalens[extraction_currchunk] - extraction_chunkpos;
		if (chunk_remaining == 0) {
			AdvanceExtractChunk();
			continue;
		}

		int this_chunk_taken_len = (chunk_remaining > reqd_len) ? reqd_len : chunk_remaining;

		if (dest) {
			memcpy(dest, data[extraction_currchunk] + extraction_chunkpos, this_chunk_taken_len);
			dest += this_chunk_taken_len;
		}

		extraction_chunkpos += this_chunk_taken_len;

		//Taken enough?
		all_chunks_taken_len += this_chunk_taken_len;
		if (all_chunks_taken_len == this_extract_len)
			break;

		//No so start on the next chunk
		AdvanceExtractChunk();

		reqd_len -= this_chunk_taken_len;
	}

	extraction_pos += this_extract_len;
	return this_extract_len;
}










//**************************************************************************************************
char* RechunkingBuffer::AppendItem(const void* source, unsigned short len)
{
	//Got room in the last chunk?
	if (item_append_chunk_avail < len) {
		static int AUTOCHUNK_SIZE = USHRT_MAX;
	
		data.push_back(new char[AUTOCHUNK_SIZE]);
		datalens.push_back(0);

		item_append_chunkid = data.size() - 1;
		item_append_ptr = data.back();
		item_append_len = &(datalens.back());
		item_append_chunk_avail = AUTOCHUNK_SIZE;
	}

	char* itemptr = item_append_ptr;
	memcpy(itemptr, source, len);

	*item_append_len += len;	
	item_append_ptr += len;
	total_len += len;
	item_append_chunk_avail -= len;

	return itemptr;
}







//**************************************************************************************************
void RechunkingBuffer::Append(RechunkingBuffer& rhs) 
{
	//Ensure no memory probs halfway through
	data.reserve(data.size() + rhs.data.size());
	datalens.reserve(datalens.size() + rhs.datalens.size());

	data.insert(data.end(), rhs.data.begin(), rhs.data.end()); 
	datalens.insert(datalens.end(), rhs.datalens.begin(), rhs.datalens.end()); 
	
	total_len += rhs.total_len;

	//Now this stuff has a new owner
	rhs.data.clear();
	rhs.datalens.clear();
	rhs.Clear();
}

}} //close namespace
