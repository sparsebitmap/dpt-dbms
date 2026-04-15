
//********************************************************
//V2.24 This only for gcc
#ifdef __GNUC__

#define _int64 long long
#define _int32 long
#define _int16 short
#define _int8 char

//********************************************************
//Likewise this only for MSVC
#else

//Those long STL templated class names are OK
#pragma warning (disable : 4786)

#ifdef BB_VC8

//I'm happy with the old buffer functions.  The NO_DEP macro doesn't seem to work, so pragma.
#define _AFX_SECURE_NO_DEPRECATE
#define _CRT_SECURE_NO_DEPRECATE
#pragma warning (disable : 4996)

#define _CRT_SECURE_CPP_OVERLOAD_SECURE_NAMES 0

//Prob needs to be looked at longer term, but with the message suppressed works the
//same as VC++ 6, which I was happy with.  e.g. int x < v.size() I do a lot.
#pragma warning (disable : 4018)

//Possible loss of data on assignment.  Same comment as above.  
#pragma warning (disable : 4244)

//64 bit time will have to be looked at before 2038 :-)
#define _USE_32BIT_TIME_T

//Prevents some linker problems with _invalid_parameter_noinfo()
#define _SECURE_SCL 0

#endif //VC8

#endif
