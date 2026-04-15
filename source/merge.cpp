
#include "stdafx.h"

#include "merge.h"
#include "except.h"
#include "msg_util.h"

namespace dpt { namespace util {

//***********************************************
Merger::TreeLeaf::TreeLeaf(MergeRecordStream* s) : stream(s)
{
	stream->ReadFirstKey();
	current_lo_stream = stream;
}

//***********************************************
MergeRecordStream* Merger::TreeBranch::ReadNextLoKey()
{
	//Read in the next record from whatever stream had the previous lowest
	current_lo_stream = lo_node->ReadNextLoKey();

	//And note the value on the other child
	MergeRecordStream* hiside_lo_stream = hi_node->CurrentLoStream();

	//EOF on a stream means the other child must be lower now.
	if (current_lo_stream == NULL ||

		//Otherwise compare the two values.  So in the end we'll do one compare 
		//per level of the tree, so log2(n) where there are n streams.
		(hiside_lo_stream != NULL && hiside_lo_stream->LowerKeyThan(*current_lo_stream))) 
	{
		current_lo_stream = hiside_lo_stream;
		SwapHiLo();
	}

	return current_lo_stream;
}

//***********************************************
Merger::TreeBranch::TreeBranch(TreeNode* l, TreeNode* r) 
: left_node(l), right_node(r)
{
	//Initialize the info about which side is lower
	MergeRecordStream* left_lo_stream = left_node->CurrentLoStream();
	MergeRecordStream* right_lo_stream = right_node->CurrentLoStream();

	if (left_lo_stream->LowerKeyThan(*right_lo_stream)) {
		current_lo_stream = left_lo_stream;
		lo_node = left_node;
		hi_node = right_node;
	}
	else {
		current_lo_stream = right_lo_stream;
		lo_node = right_node;
		hi_node = left_node;
	}
}

//***********************************************
Merger::Merger(std::vector<MergeRecordStream*>& streams)
{
	allnodes.reserve(streams.size() * 2);

	try {

		//First make the leaf level of the tree
		for (size_t x = 0; x < streams.size(); x++)
			allnodes.push_back(new TreeLeaf(streams[x]));

		//Then pair off nodes to make the branches 
		unsigned int left = 0;
		unsigned int right = 1;
		while (right < allnodes.size()) {
			allnodes.push_back(new TreeBranch(allnodes[left], allnodes[right]));
			left += 2;
			right += 2;
		}

		//Till there's just one root node
		root = allnodes.back();
	}
	catch (...) {
		Destroy();
		throw;
	}
}

//***********************************************
void Merger::Destroy()
{
	for (size_t x = 0; x < allnodes.size(); x++)
		delete allnodes[x];
}

}} //close namespace
