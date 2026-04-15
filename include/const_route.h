
#if !defined BB_CONST_ROUTE
#define BB_CONST_ROUTE

namespace dpt {

//Bit setting flags for the MSGCTL parameter
enum MSGCTLSettings {
	MSGCTL_SUPPRESS_PREFIX	= 1, 
	MSGCTL_SUPPRESS_INFO	= 2, 
	MSGCTL_SUPPRESS_ERROR	= 4,
	MSGCTL_AUDIT_PROC_INFO  = 32,
	MSGCTL_ALL_BITS	        = 39
};


} //close namespace

#endif
