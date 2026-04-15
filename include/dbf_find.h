
//V2.07. Sep 07.  Added this class to both fix a thread-safety issue with information cached
//during a find, and also to stop this stuff cluttering up the index manager class, which
//strictly speaking is a file level (i.e. system-wide) object, not context level (i.e. user).

#include "findwork.h"
#include "apiconst.h"

namespace dpt {

class DatabaseFile;
class CFRSentry;
class DatabaseServices;
class SingleDatabaseFileContext;
class BitMappedFileRecordSet;
class FoundSet;
class DirectValueCursor;
class FindSpecification;
namespace util {
	class LineOutput;
}

//*******************
class FindOperation {

	int groupix;
	SingleDatabaseFileContext* sfc;
	const FindSpecification& spec;
	const FindEnqueueType& locktype;
	FoundSet* resultset;
	const BitMappedFileRecordSet* baseset;
	DatabaseServices* dbapi;
	util::LineOutput* op;
	CFRSentry* dvc_prelock;
	FindWorkInfo fwi;
	DatabaseFile* file;

	void Perform_File$			(FindWorkNode_Leaf*, bool);
	void Perform_Singlerec		(FindWorkNode_Leaf*);
	void Perform_Set$			(FindWorkNode_Leaf*);
	void Perform_IxEQ			(FindWorkNode_Leaf*);
	void Perform_IxRange		(FindWorkNode_Leaf*);
	void Perform_Point$			(FindWorkNode_Leaf*);
	void Perform_TableBSearch	(BitMappedFileRecordSet*);
	bool Perform_EBP_Phase		(bool, BitMappedFileRecordSet*);

	void CriterionDiagnostics1(FindWorkNode_Leaf*);
	void CriterionDiagnostics2(BitMappedFileRecordSet*);
	void CriterionDiagnostics3(FindWorkNode_Leaf*);
	void FinalDiagnostics();
	static util::LineOutput* DiagOP(util::LineOutput* pop, unsigned int i) {
										return (i) ? pop : NULL;}

	BitMappedFileRecordSet* DVCFind(DirectValueCursor&, BitMappedFileRecordSet*, bool = false);

	void PrepareForPossibleNegation(const BitMappedFileRecordSet*, FindWorkNode_Leaf*);
	void NegateSetIfRequired(FindWorkNode_Leaf*, const BitMappedFileRecordSet*, BitMappedFileRecordSet**);

	struct TBSLeafInfo {
		BitMappedFileRecordSet* built_set;
		BitMappedFileRecordSet* base_set;

		TBSLeafInfo() : built_set(NULL), base_set(NULL) {}
		~TBSLeafInfo();
	};

public:
	FindOperation(int, SingleDatabaseFileContext*, FoundSet*, const FindSpecification&, 
					const FindEnqueueType&, const BitMappedFileRecordSet*, DatabaseFile*);

	void Perform();
};

}
