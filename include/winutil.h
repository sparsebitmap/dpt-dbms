//******************************************************************************
//Assorted wrapper functions for windows functionality.
//******************************************************************************

#if !defined(BB_WINUTIL)
#define BB_WINUTIL

#include <time.h>
#include <string>
#include <vector>

namespace dpt { namespace win {

//Time functions (low res).
tm GetDateAndTime_tm(time_t = 0, bool localize = true);
std::string GetCTime(time_t = 0);
_int64 GetWinSysTime();
double GetFTimeWithMillisecs();  //in seconds plus fraction

//Higher resolution timer if available
bool HFPC_Allowed();
_int64 HFPC_Query();
double HFPC_ConvertToSec(const _int64&);

//Thread and process CPU accounting if available
bool CPUAccountingAllowed();
_int64 GetProcessCPU();
_int64 GetThreadCPU();
inline _int64 FileTimeToInt64(void*);

//V2.03/2.05.  Not required for API programs
#if defined(_BBHOST) || defined(_BBCLIENT)
COleDateTime PseudoUTCToCODT(_int64);
_int64 CODTToPseudoUTC(const COleDateTime&);

std::string GetShellRCString(int);
void ShellBrowserAtAnchor(void* hWnd, const std::string& docname, 
				const std::string& anchor, const char* reroute);

std::string GetDefaultPrinterName();

//V2.14 Mar 09.  Now used on both client and host.
bool BrowseForFontOptions(CWnd*, std::string&, int&, bool&, bool&, bool = false);
#endif

//Decode Windows API messages into text
std::string GetErrorMessage(int, bool = true);
std::string GetLastErrorMessage(bool = true);
void SetLastErrorMessage(const std::string&);

//Removes a non-empty directory by recusrive calling
int RemoveDirectoryTree(const char*, char* errbuff = NULL);
//V3.0.  Can be more convenient although often you don't care 
void RemoveDirectoryTreeWithThrow(const char*, bool nonexistent_is_ok);
bool ListDirectoryTree(std::vector<std::string>&, const std::string& path, 
					   const std::string& mask = "*", bool getdirs = false);

//Generates unused file names if necessary
std::string GetUnusedFileName(const std::string&);

//Multitasking
void Cede(bool = false);
void SetCedeModeRealThreadSwap(bool);

//Memory management
unsigned int VirtualMemoryCalcFree(bool use_GlobalMemoryStatus_function);
//V2.23 Nov 09.  Wine auto-detect proved unreliable.
//void VirtualMemoryCalcFree_ForceUseGlobalMemoryStatus(bool);
std::string VirtualMemoryMap();

//System information
int GetProcessorCount(); //V2.28

}} //close namespace

#endif
