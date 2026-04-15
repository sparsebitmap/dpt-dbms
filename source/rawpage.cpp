
#include "stdafx.h"


#include "windows.h"
#include "rawpage.h"
#include "except.h"
#include "msg_util.h"

namespace dpt {

//****************************************************************************************
AlignedPage::AlignedPage(const char* initial_value)
{
	data = VirtualAlloc(NULL, DBPAGE_SIZE, MEM_COMMIT, PAGE_READWRITE);

	if (data == NULL)
		throw Exception(UTIL_MALLOC_FAILURE, 
			"Error allocating memory for single page buffer");

	if (initial_value) {
		int len = strlen(initial_value);
		if (len > DBPAGE_SIZE)
			len = DBPAGE_SIZE;

		memcpy(data, initial_value, len);
	}
}

//****************************************************************************************
AlignedPage::~AlignedPage()
{
	if (data)
		VirtualFree(data, 0, MEM_RELEASE);
}

} //close namespace
