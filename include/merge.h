/********************************************************************************************
Some simple classes implementing a simple tree-based merge of sorted sets of records. A tree 
is more efficient than repeated sequential scans to find the lowest if there are many sets.
********************************************************************************************/

#if !defined(BB_MERGE)
#define BB_MERGE

#include <string>
#include <vector>
#include <algorithm>

namespace dpt { namespace util {

/* * * Note * * *
This class is currently virtual as I'm too lazy to templatize the whole merge thing 
today, even if that would be a good thing anyway.
* * * * * * * * */
class MergeRecordStream {
public:
	virtual ~MergeRecordStream() {}

	virtual void ReadFirstKey() = 0;
	virtual MergeRecordStream* ReadNextKey() = 0;

	virtual bool LowerKeyThan(const MergeRecordStream& ) const = 0;
};


//*******************************************************************************************
//The main interface class
//*******************************************************************************************
class Merger {

	//Utility classes
	class TreeNode {
	protected:
		MergeRecordStream* current_lo_stream;

	public:
		MergeRecordStream* CurrentLoStream() {return current_lo_stream;}
		virtual MergeRecordStream* ReadNextLoKey() = 0;
	};

	//------------
	class TreeLeaf : public TreeNode {
		MergeRecordStream* stream;

	public:
		TreeLeaf(MergeRecordStream*);
		MergeRecordStream* ReadNextLoKey() {return current_lo_stream = stream->ReadNextKey();}
	};

	//------------
	class TreeBranch : public TreeNode {
		TreeNode* left_node;
		TreeNode* right_node;
		TreeNode* lo_node;
		TreeNode* hi_node;
		void SwapHiLo() {TreeNode* temp = hi_node; hi_node = lo_node; lo_node = temp;}

	public:
		TreeBranch(TreeNode*, TreeNode*);
		MergeRecordStream* ReadNextLoKey();
	};

	//------------
	void Destroy();

	std::vector<TreeNode*> allnodes;
	TreeNode* root;

public:

	//The streams passed in do not become owned by the merge.  Caller deletes them.
	Merger(std::vector<MergeRecordStream*>&);
	~Merger() {Destroy();}

	MergeRecordStream* ReadNextLoKey() {return root->ReadNextLoKey();}
	MergeRecordStream* CurrentLoStream() {return root->CurrentLoStream();}
};


}} //close namespace

#endif

/*

*******************************************************************************************
Testing version of the stream class

class MergeRecordStream {
	std::vector<std::string> data;
	int pos;

public:
	void ReadFirstKey() {pos = 0; if (data.size() == 0) throw "Empty file!";}
	MergeRecordStream* ReadNextKey() {if (++pos >= data.size()) return NULL; return this;}

	//This would not always be a string
	const std::string& CurrentKey() const {return data[pos];}

	//This should be made efficient by caching the value to be compared as fast as poss.
	//But for testing here it's OK to do a lookup on the vector each time.
	bool LowerKeyThan(const MergeRecordStream& r) const {return CurrentKey() < r.CurrentKey();} 

	///Testing
	MergeRecordStream& Add(const std::string& s) {data.push_back(s); return *this;}
	void Sort() {std::sort(data.begin(), data.end());}
};



Some calling code
MergeRecordStream ms1, ms2, ms3, ms4, ms5;
ms1.Add("Coke").Add("Wine").Add("Whisky").Add("Beer").Sort();
ms2.Add("Milk").Add("Water").Add("Squash").Sort();
ms3.Add("Tomato").Add("Orange").Add("Cranberry").Add("Grape").Sort();
ms4.Add("Becks").Add("Courage").Add("Youngs").Add("Heineken").Add("Leffe").Sort();
ms5.Add("Draino").Add("Acid").Add("Bleach").Add("Varnish").Sort();

std::vector<MergeRecordStream*> vms;
vms.push_back(&ms1);
vms.push_back(&ms2);
vms.push_back(&ms3);
vms.push_back(&ms4);
vms.push_back(&ms5);

util::Merger merger(vms);

while (merger.CurrentLoStream()) {
	cout << merger.CurrentLoStream()->CurrentKey()_c_str() << endl;
	merger.ReadNextLoKey();
}


*/
