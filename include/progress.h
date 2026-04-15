//*******************************************************************************************
// Functions and declarations to support progress reporting throughout the system.
//*******************************************************************************************

#if !defined(BB_PROGRESS)
#define BB_PROGRESS

#include <string>

namespace dpt {

//Supplied by the activity to the progress function
const int PROGRESS_REGISTER			= 1;
const int PROGRESS_DEREGISTER		= 2;
const int PROGRESS_START			= 3;
const int PROGRESS_END				= 4;
const int PROGRESS_UPDATE			= 5;

//Call type
const int PROGRESS_OPERATION		= 1;
const int PROGRESS_ACTIVITY			= 2;
const int PROGRESS_ACTIVITY_GROUP	= 3;

//Returned by the progress function
const int PROGRESS_PROCEED					= 1;
const int PROGRESS_CANCEL_ACTIVITY			= 2;
//This is a system-#defined constant and VC2005 doesn't like this redef.
//const int PROGRESS_CANCEL					= PROGRESS_CANCEL_ACTIVITY;
const int PROGRESS_CANCEL_ACTIVITY_GROUP	= 4;
const int PROGRESS_CANCEL_OPERATION			= 8;
const int PROGRESS_CANCEL_ALL				= PROGRESS_CANCEL_OPERATION;
const int PROGRESS_ABORT_OPERATION			= 16;

class ProgressReportableActivity;

//Progress reporting functions must adhere to this prototype
typedef int (*ProgressFunction) (const int, const ProgressReportableActivity*, void*);

//Standard simple ones to be used as defaults
inline int SimplyContinue(const double, const ProgressReportableActivity*) {
	return PROGRESS_PROCEED;}
inline int AbortASAP(const double, const ProgressReportableActivity*) {
	return PROGRESS_CANCEL_OPERATION;}



//************************************************************************
class ProgressReportableActivity {
	ProgressFunction progress_function;
	void* progress_context;
	ProgressReportableActivity* parent;

	std::string name;
	int type;
	int return_options;

protected:
	int ProgressReport(const int);

public:
	ProgressReportableActivity(const std::string&, ProgressFunction, void* = NULL, 
		ProgressReportableActivity* = NULL);
	virtual ~ProgressReportableActivity();

	std::string Name() const {return name;}
	int Type() const {return type;}
	int ReturnOptions() const {return return_options;}
	void ValidateReturnOption(const int);

	virtual std::string Info() const {return std::string();}
	virtual int NumSteps() const {return 0;}
	virtual int StepsComplete() const {return 0;}
	virtual double PercentComplete() const {return 0.0;}
};

} //close namespace

#endif
