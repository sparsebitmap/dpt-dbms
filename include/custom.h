
#if !defined BB_CUSTOM
#define BB_CUSTOM

namespace dpt {

const unsigned int DISABLE_BIT_COMMANDS				= 0x0001;
const unsigned int DISABLE_BIT_UL_STATEMENTS		= 0x0002;
const unsigned int DISABLE_BIT_$FUNCTIONS			= 0x0004;
const unsigned int DISABLE_BIT_$FUNC_RT_THROW		= 0x0008;
const unsigned int DISALLOW_HEX_NUM_LITERALS        = 0x0010;

//Bits controlling specific functionality
const unsigned int DISABLE_BIT_APSY_FILE_NOCLOSE	= 0x0100;

//Fudge bits - only here provisionally (I hope)
//V1.2 25/7/06
const unsigned int LSTPROC_PREPEND_CENTURY_FLAG		= 0x1000;
//V2 Jan 07
const unsigned int LIST_CAPTURE_NO_PAGINATION		= 0x2000;
const unsigned int SIRIUS_RANDOM_FLOAT_RESULT		= 0x4000;
const unsigned int SIRIUS_IMGOVL_ALWAYS_THROW		= 0x8000;
//V3.03 Summer 2012
const unsigned int REQUIRE_LOGON_AT_CONNECTION_TIME = 0x0200;
const unsigned int ENABLE_ACCESS_CONTROL            = 0x0400;

//*******************************************************************************
//Base class for the derived object kind of control, for example command objects.
//When deriving a new class choose a control bit from those available above.
class CustomDisableable {

	//Individual sub-object mask saying which bit(s) must be set to disable
	virtual const unsigned int CustomDisableMask() {return 0;}

	static unsigned int CoreDisableFlags();

protected:
	static bool MaskCheck(const unsigned int m) {return (CoreDisableFlags() & m) ? true : false;}

public:
	bool IsDisabled() {return MaskCheck(CustomDisableMask());}
};

//************************************************************************************
struct LeaveApsyOperation : public CustomDisableable {
	const unsigned int CustomDisableMask() {return DISABLE_BIT_APSY_FILE_NOCLOSE;}
};

}	//close namespace

#endif

