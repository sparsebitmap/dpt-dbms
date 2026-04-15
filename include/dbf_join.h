
//The "Join Engine".
//The user doc contains a description of how this works.

namespace dpt {

class ValueSet;

class ValueSetToBTreeJoinOperation {

	ValueSet* input_set;

	bool precalc_done;
	bool merge_chosen;

	ValueSet* PerformMerge();
	ValueSet* PerformLoop();

public:
	ValueSetToBTreeJoinOperation(ValueSet*);

	void PreCalculate();
	ValueSet* Perform();
};

}
