
#include "stdafx.h"

#include "page_l.h"
#include "fieldval.h"

namespace dpt {

//**************************************************************************************
short BLOBPage::AllocateSlotAndStoreBLOB(const char** ppblobdata, int& blob_remaining)
{
	//Each extent has an extension pointer like table B, although not just 4 bytes here
	int blob_maxrequired = blob_remaining + PAGE_L_EXTPTR_LEN;

	//If not enough room for the whole object we'll take any sizeable chunk
	short minimum_required_pagespace;
	if (blob_maxrequired < BLOB_SMALLEST_EXTENT)
		minimum_required_pagespace = blob_maxrequired;
	else
		minimum_required_pagespace = BLOB_SMALLEST_EXTENT;

	short avail_pagespace = NumFreeBytes();

	//No luck
	if (avail_pagespace < minimum_required_pagespace)
		return -1;

	//OK we're happy with this page, so ideally take as much space as there is
	short use_pagespace = (avail_pagespace >= blob_maxrequired) ? blob_maxrequired : avail_pagespace;

	//This may still fail because of insufficient slots
	short slot = AllocateSlot(0, use_pagespace);
	if (slot == -1)
		return -1;

	//Looking good - now populate the page
	MakeDirty();

	short pageoffset = MapRecordOffset(slot);

	//Make empty space
	Splice(slot, pageoffset, NULL, use_pagespace);

	//Initialize extension pointer to "unextended"
	SlotSetExtensionPointers(slot, -1, 0);

	//Copy in the data after that
	short blob_extent_data_bytes = use_pagespace - PAGE_L_EXTPTR_LEN;

	char* datadest = MapPChar(pageoffset) + PAGE_L_EXTPTR_LEN;
	memcpy(datadest, *ppblobdata, blob_extent_data_bytes);

	//Update these for the caller's loop control		
	blob_remaining -= blob_extent_data_bytes;
	*ppblobdata += blob_extent_data_bytes;

	return slot;
}

//**************************************************************************************
void BLOBPage::GetBLOBExtentData(int& page, short& slot, std::string& blobdata)
{
	const char* extent_data;
	short extent_len;
	GetBLOBExtentData(page, slot, &extent_data, &extent_len);

	blobdata.append(extent_data, extent_len);
}

//**************************************************************************************
void BLOBPage::GetBLOBExtentData(int& page, short& slot, const char** extent_data, short* extent_len)
{
	SlotGetPageDataPointer(slot, extent_data, extent_len);

	*extent_data += PAGE_L_EXTPTR_LEN;
	*extent_len -= PAGE_L_EXTPTR_LEN;

	//Set for the next iteration
	SlotGetExtensionPointers(slot, page, slot);
}

} //close namespace
