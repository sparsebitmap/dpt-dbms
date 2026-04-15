//*****************************************************************************************
// All the DPT API headers, grouped for convenience.
// Or obviously include only the ones you need for faster compiles.
//*****************************************************************************************

#if !defined BB_DPTDB
#define BB_DPTDB

//1. Assorted constants, typedefs, message/exception codes
#include "apiconst.h"
#include "msgcodes.h"
#include "msg_core.h"
#include "msg_file.h"
#include "msg_db.h"
#include "except.h"

//2. Utility classes
#include "lineio.h"
#include "floatnum.h"
#include "fieldval.h"

//3. Core level service objects
#include "core.h"
#include "parmvr.h"
#include "statview.h"
#include "msgroute.h"

//4. Database level service objects
#include "dbserv.h"
#include "grpserv.h"
#include "seqserv.h"

//5. MROs and info structs
#include "ctxtspec.h"
#include "dbctxt.h"
#include "seqfile.h"
#include "fieldatts.h"
#include "fieldinfo.h"
#include "findspec.h"
#include "foundset.h"
#include "reccopy.h"
#include "reclist.h"
#include "record.h"
#include "recread.h"
#include "recset.h"
#include "seqfile.h"
#include "sortset.h"
#include "sortspec.h"
#include "valset.h"
#include "valdirect.h"

//6. Assuming you may find these handy
//#include "handles.h"
//#include "garbage.h"

#endif
