
#include "stdafx.h"

#include "sys\types.h"		//for _ftime etc.
#include "sys\timeb.h"		//for _ftime etc.

#include "liostdio.h"
#include "bbstack.h"
#include "lockable.h"
#include "dataconv.h"
#include "except.h"
#include "msg_util.h"
#include "winutil.h"
#include <sys\types.h>
#include <sys\stat.h>
#include "windows.h"
#include "winspool.h"
#include "shellapi.h"

#if defined(_BBHOST) || defined(_BBCLIENT)
//static COleDateTime CODT_BASE_UTC = COleDateTime( (time_t)0 );  //VS 2008
static COleDateTime CODT_BASE_UTC = COleDateTime();
#endif

namespace dpt { namespace win {

//*****************************************************************************************
//The ANSI functions like localtime() are not threadsafe, since they use a statically-
//allocated buffer so this function is used to return a time structure to a thread.
//Hmmm - well actually they may be threadsafe if using the multithreaded runtime, but 
//I'm leaving the lock in here anyway
//*****************************************************************************************
static Lockable date_and_time_lock;
bool real_thread_swap_on_cede = false;
static ThreadSafeString custom_error_string = ThreadSafeString("Bug? Custom WinAPI message text not set");

//***********************
tm GetDateAndTime_tm(time_t input, bool localize)
{
	//Zero input means now
	time_t t(input);
	if (!input) 
		time(&t);

	//Lock while we manipulate the shared static buffer
	LockingSentry s(&date_and_time_lock);
	if (localize)
		return  *(localtime(&t));
	else
		return  *(gmtime(&t));
}

//***********************
//Often used for messages where it's neater and "Day Mon dd hh:mm:ss YYYY" is acceptable
std::string GetCTime(time_t input)
{
	//Zero input means now
	time_t t(input);
	if (!input) 
		time(&t);

	LockingSentry s(&date_and_time_lock);
	char buff[26];
	strcpy(buff, ctime(&t));
	buff[24] = 0;               //it has a newline on it
	return buff;
}

//***********************
double GetFTimeWithMillisecs()
{
	_timeb now;		
	_ftime(&now);
	double secs = now.time;

	double msecs = now.millitm;
	secs += msecs/1000;

	return secs;
}

//***************************************************************************************
//Timer notes:
//This is decent for recording time intervals so long as they're not too short, or are 
//being aggregated over lots of calls.  The result is in units of 100ns, but actually 
//depending on the machine it will usually be 10ms or 15ms (100K units).  This is the 
//unchangeable scheduler clock frequency on Windows, and that's where the system time
//is maintained.  Clock() and GetTickCount() amongst other things hook into the same data 
//so have the same effective resolution in the end.  For shorter intervals, multimedia
//timers can go down to 1ms, but after that it's the performance counter (see later).  
//***************************************************************************************
_int64 GetWinSysTime()
{
//	SYSTEMTIME stime;
	FILETIME ftime;

//	GetSystemTime(&stime);
//	SystemTimeToFileTime(&stime, &ftime);

	//V2.12. April 2008. I didn't know about this shortcut before.  
	//Works much faster, although result is not rounded to nearest msec.
	GetSystemTimeAsFileTime(&ftime);

	return FileTimeToInt64(&ftime);
}

//*****************************************************************************************
//The call cost of QueryPerformanceCounter is much higher than the above, or than e.g. 
//GetTickCount, but this is the only way to get resolution down in the sub microsecond
//range.  Each quantum here is about 0.8 ms on NT systems (although I have heard it may
//depend on processor speed, so we have to check the frequency value first).
//*****************************************************************************************
LARGE_INTEGER hfpc_frequency;
const BOOL hfpc_allowed = QueryPerformanceFrequency(&hfpc_frequency);
bool HFPC_Allowed() {return hfpc_allowed == TRUE;}
const double hfpc_frequency_double = hfpc_frequency.QuadPart;

//**************************
_int64 HFPC_Query()
{
	if (!hfpc_allowed)
		return 0;

	LARGE_INTEGER li;
	QueryPerformanceCounter(&li);
	return li.QuadPart;
}

//**************************
//Decided to go with a FP result in seconds rather than _in64 in nanoseconds, to allow 
//for much faster future machines where the answer might be <1ns.
double HFPC_ConvertToSec(const _int64& ct)
{
	if (!hfpc_allowed)
		return 0;

	return ct / hfpc_frequency_double;
}

//***************************************************************************************
//These may or may not be allowed (require NT 3.5 or greater).  They're used for 
//displaying the DPT CPU statistic when requested for a thread or the system as a whole.
//***************************************************************************************
bool CPUAccountingAllowed()
{
	static bool tested(false);
	static bool allowed(false);

	if (tested)
		return allowed;

	//Will be allowed to use CPU time accounting?
	OSVERSIONINFO osvi;
	ZeroMemory(&osvi, sizeof(OSVERSIONINFO));
	osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);

	if (GetVersionEx(&osvi))
		if (osvi.dwPlatformId == VER_PLATFORM_WIN32_NT)
			if (osvi.dwMajorVersion >= 3)			//3.51
				allowed = true;

	tested = true;
	return allowed;
}

//***************************************************************************************
_int64 GetProcessCPU()
{
	if (!CPUAccountingAllowed())
		return 0;

	HANDLE hprocess = GetCurrentProcess();

	FILETIME ft_ct; //no use to us here
	FILETIME ft_et; //""
	FILETIME ft_kmt;
	FILETIME ft_umt;
	int rc = GetProcessTimes(hprocess, &ft_ct, &ft_et, &ft_kmt, &ft_umt);

	if (!rc)
		throw Exception(MISC_OS_ROUTINE_ERROR, "Error calling WINAPI GetProcessTimes()");

	//Let's add in the kernel time too
	_int64 kmt = FileTimeToInt64(&ft_kmt);
	_int64 umt = FileTimeToInt64(&ft_umt);

	return (umt + kmt);
}

//***************************************************************************************
_int64 GetThreadCPU()
{
	if (!CPUAccountingAllowed())
		return 0;

	HANDLE hthread = GetCurrentThread();

	FILETIME ft_ct; //no use to us here
	FILETIME ft_et; //""
	FILETIME ft_kmt;
	FILETIME ft_umt;
	int rc = GetThreadTimes(hthread, &ft_ct, &ft_et, &ft_kmt, &ft_umt);

	if (!rc)
		throw Exception(MISC_OS_ROUTINE_ERROR, "Error calling WINAPI GetThreadTimes()");

	//Let's add in the kernel time too
	_int64 kmt = FileTimeToInt64(&ft_kmt);
	_int64 umt = FileTimeToInt64(&ft_umt);

	return (umt + kmt);
}

//***************************************************************************************
inline _int64 FileTimeToInt64(void* vft)
{
	FILETIME* ft = (FILETIME*) vft;

	if (!ft)
		return 0;

	ULARGE_INTEGER result;
	result.HighPart = ft->dwHighDateTime;
	result.LowPart = ft->dwLowDateTime;
	return result.QuadPart;
}

//***********************
#if defined(_BBHOST) || defined(_BBCLIENT)
COleDateTime PseudoUTCToCODT(_int64 secs)
{
	//This should be happy with dates well beyond 2038 (also < 1970).  The validity
	//range is what CODT can handle which is Jan 1 100 till 31 Dec 9999.
	//Note that the CODTS constructor only takes ints, so factor out days.
	_int64 days = secs / 86400;
	secs -= days * 86400;
	COleDateTimeSpan ts(days, 0, 0, secs);
	return CODT_BASE_UTC + ts;
}

//***********************
_int64 CODTToPseudoUTC(const COleDateTime& input_date)
{
	COleDateTimeSpan utc_rebase = input_date - CODT_BASE_UTC;

	//See comments re the reverse conversion above.  (utc_rebase can be negative).
	_int64 days = utc_rebase.GetTotalDays();
	_int64 daysecs = days * 86400;

	//This part will usually be zero since the input date would not have a time part.
	//The exception is WEB format dates which do.
	COleDateTimeSpan remsecspan = utc_rebase - COleDateTimeSpan(days, 0, 0, 0);
	_int64 remsecs = remsecspan.GetTotalSeconds();
	_int64 totsecs = daysecs + remsecs;

	return totsecs;
}
#endif

//*****************************************************************************************
//Neaten up Windows API error message retrieval.
//*****************************************************************************************
std::string GetErrorMessage(int errcode, bool strip_eol)
{
	//V3.0. Allow custom messages in parallel with WinAPI mechanism.  See next function.
	const char* text = NULL;
	std::string buffcopy;
	DWORD source_flag = FORMAT_MESSAGE_FROM_SYSTEM;

	if (errcode == 0x20000000) {
		source_flag = FORMAT_MESSAGE_FROM_STRING;
		buffcopy = custom_error_string.Value();
		text = buffcopy.c_str();
	}

	LPVOID lpMsgBuf;
	FormatMessage( 
		FORMAT_MESSAGE_ALLOCATE_BUFFER | 
		source_flag | 
		FORMAT_MESSAGE_IGNORE_INSERTS,
		text,
		errcode,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
		(LPTSTR) &lpMsgBuf,
		0,
		NULL 
	);

	std::string result = (LPCTSTR)lpMsgBuf;
	LocalFree( lpMsgBuf );

	if (strip_eol) {
		if (result[result.length()-1] == '\n')
			result.resize(result.length()-1);
		if (result[result.length()-1] == '\r')
			result.resize(result.length()-1);
	}

	return result;
}

//********************************************
std::string GetLastErrorMessage(bool strip_eol)
{
	return GetErrorMessage(GetLastError(), strip_eol);
}

//********************************************
//V3.0.  This is threadsafe although not guaranteed to give the right message if two
//threads used this mechanism around the same time.  Not widely used so it's OK for now.
void SetLastErrorMessage(const std::string& text)
{
	//Set bit indicating an application-defined message
	SetLastError(0x20000000);
	custom_error_string.Set(text);
}

#if defined(_BBHOST) || defined(_BBCLIENT)
//*****************************************************************************************
std::string GetShellRCString(int rc)
{
	switch (rc) {
	case 0: return "The operating system is out of memory or resources";
	case ERROR_FILE_NOT_FOUND:  return "The specified file was not found. ";
	case ERROR_PATH_NOT_FOUND:  return "The specified path was not found. ";
	case ERROR_BAD_FORMAT:  return "The .exe file is invalid (non-Win32® .exe or error in .exe image). ";
	case SE_ERR_ACCESSDENIED:  return "The operating system denied access to the specified file.";  
	case SE_ERR_ASSOCINCOMPLETE:  return "The file name association is incomplete or invalid. ";
	case SE_ERR_DDEBUSY:  return "The DDE transaction could not be completed because other DDE transactions were being processed. ";
	case SE_ERR_DDEFAIL:  return "The DDE transaction failed. ";
	case SE_ERR_DDETIMEOUT:  return "The DDE transaction could not be completed because the request timed out. ";
	case SE_ERR_DLLNOTFOUND:  return "The specified dynamic-link library was not found.  ";
//	case SE_ERR_FNF:  return "The specified file was not found.  ";
	case SE_ERR_NOASSOC:  return "There is no application associated with the given file name extension. This error will also be returned if you attempt to print a file that is not printable. ";
	case SE_ERR_OOM:  return "There was not enough memory to complete the operation. ";
//	case SE_ERR_PNF:  return "The specified path was not found. ";
	case SE_ERR_SHARE:  return "A sharing violation occurred."; 
	}
	
	return "Call successful.";
}
#endif

//*****************************************************************************************
//This function returns WINAPI codes in some cases, e.g. success.
//*****************************************************************************************
int RDT_Finish
(int rc, const char* errfile = NULL, char* errbuff = NULL)
{
	if (errfile && errbuff)
		strcpy(errbuff, errfile);
	return rc;
}

//**************************
int RemoveDirectoryTree(const char* dirname, char* errbuff)
{
	//First decide if it's a directory and deletable
	DWORD gfa = GetFileAttributes(dirname);

	//Almost certainly does not exist
	if (gfa == (DWORD)-1)
		return RDT_Finish(GetLastError(), dirname, errbuff);

	//Not a directory
	if ( ! (gfa & FILE_ATTRIBUTE_DIRECTORY))
		return RDT_Finish(ERROR_DIRECTORY, dirname, errbuff);

	if (gfa & FILE_ATTRIBUTE_READONLY || 
		gfa & FILE_ATTRIBUTE_ARCHIVE ||
		gfa & FILE_ATTRIBUTE_SYSTEM)
			return RDT_Finish(ERROR_ACCESS_DENIED, dirname, errbuff);

	WIN32_FIND_DATA dir_data;
	HANDLE handle = INVALID_HANDLE_VALUE; 

	char findmask[512];
	strcpy(findmask, dirname);
	strcat(findmask, "\\*.*");

	int rc = ERROR_SUCCESS;

	//If there are contents, delete them
	handle = FindFirstFile(findmask, &dir_data);
	if (handle != INVALID_HANDLE_VALUE) {

		//Loop on the contents
		int more = 1;
		while (more) {

			//Construct complete path name for the file/dir
			const char* shortname = dir_data.cFileName;
			char path[512];
			strcpy(path, dirname);
			strcat(path, "\\");
			strcat(path, shortname);

			//Directory - delete it with a recursive call to this func
			if (dir_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {

				//Ignore . and ..
				bool pseudo_dir = false;
				if (shortname[0] == '.') {
					if (shortname[1] == 0)
						pseudo_dir = true;
					else if(shortname[1] == '.')
						if (shortname[2] == 0)
							pseudo_dir = true;
				}

				if (!pseudo_dir)
					rc = RemoveDirectoryTree(path, errbuff);
			}

			//Regular file
			else {
				if (!DeleteFile(path))
					rc = RDT_Finish(GetLastError(), path, errbuff);
			}

			//The reason for not returning above is so FindClose can be called at loop end
			if (rc != ERROR_SUCCESS)
				break;

			//Go round for the next item in the directory
			more = FindNextFile(handle, &dir_data);
		}
	}

	//Any errors in the loop come to here
	FindClose(handle);
	if (rc != ERROR_SUCCESS)
		return RDT_Finish(rc); //file/dir name with error already noted

	//Finally delete the directory itself
	if (!RemoveDirectory(dirname))
		return RDT_Finish(GetLastError(), dirname, errbuff);

	//All OK - the directory is extinct
	return RDT_Finish(ERROR_SUCCESS, "", errbuff);
}

//**************************
void RemoveDirectoryTreeWithThrow(const char* dirname, bool nonexistent_is_ok)
{
	char errbuff[32768];
	int rc = RemoveDirectoryTree(dirname, errbuff);

	if (rc != ERROR_SUCCESS) {
		if (rc == ERROR_FILE_NOT_FOUND || rc == ERROR_PATH_NOT_FOUND) {
			if (nonexistent_is_ok)
				return;
		}

		throw Exception(UTIL_MISC_FILEIO_ERROR, std::string
			("RemoveDirectoryTree() error removing file/subdir \'")
			.append(errbuff).append("\'. WinAPI message: ").append(GetErrorMessage(rc)));
	}
}
	
//**************************
bool ListDirectoryTree(
	std::vector<std::string>& files, const std::string& inpath, 
	const std::string& mask, bool getdirs)
{
    HANDLE hFind = INVALID_HANDLE_VALUE;
    WIN32_FIND_DATA ffd;
	std::string spec;
	util::Stack<std::string> directories(100);

	std::string path(inpath);
    directories.Push(path);
    files.clear();

    while (!directories.IsEmpty()) {
        path = directories.Top();
        spec = path + "\\" + mask;
        directories.Pop();

        hFind = FindFirstFile(spec.c_str(), &ffd);
        if (hFind == INVALID_HANDLE_VALUE)  {
            return false;
        } 

        do {
			//Skip parent directories
            if (strcmp(ffd.cFileName, ".") != 0 && 
                strcmp(ffd.cFileName, "..") != 0) {

				//Is it a directory?
                if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {

					//Yes so stack name for recursion
                    directories.Push(path + "\\" + ffd.cFileName);

					//And also record name if the user wanted dir names
					if (getdirs)
	                    files.push_back(path + "\\" + ffd.cFileName);
                }
                else {
					//It's a file so record name if the user wanted file names
					if (!getdirs)
	                    files.push_back(path + "\\" + ffd.cFileName);
                }
            }
        } while (FindNextFile(hFind, &ffd) != 0);

        if (GetLastError() != ERROR_NO_MORE_FILES) {
            FindClose(hFind);
            return false;
        }

        FindClose(hFind);
        hFind = INVALID_HANDLE_VALUE;
    }

    return true;
}

//***************************************************************************************
std::string GetUnusedFileName(const std::string& suggested)
{
	struct _stat s;
	int tries = 0;

	if (suggested.length() > _MAX_PATH)
		throw Exception(UTIL_MISC_FILEIO_ERROR, "Suggested file name too long.");

	//Pull it apart so we can insert numbers before the extension
	char fbuff[_MAX_PATH +1];
	strcpy(fbuff, suggested.c_str());
	char* dot = strchr(fbuff, '.');

	std::string extbit;
	if (dot) {
		extbit = dot;
		*dot = 0;
	}
	std::string mainbit = fbuff;
	std::string suffbit;
	std::string tryname;

	//Successively tweak the suggested name till we get a failed _stat
	for (;;) {

		//Ensure the generated name isn't too long
		while (mainbit.length() + suffbit.length() + extbit.length() > _MAX_PATH)
			mainbit.erase(mainbit.length() - 1, 1);

		tryname = std::string(mainbit).append(suffbit).append(extbit);

		//OK - no such file
		if (_stat(tryname.c_str(), &s) != 0)
			break;

		//Sanity check here.  When generating large sets of temporary files we might
		//reasonably go into the hundreds, or even just maybe thousands, but...
		if (tries++ > 10000)
			throw Exception(UTIL_MISC_FILEIO_ERROR,
				"Doh! I give up trying to find an unused file name.");

		suffbit = std::string("_").append(util::IntToString(tries));
	}

	char buff[_MAX_PATH];
	_fullpath(buff, tryname.c_str(), _MAX_PATH);
	tryname = buff;

	return tryname;
}

#if defined(_BBHOST) || defined(_BBCLIENT)
//***************************************************************************************
//Copied this direct off MSDN but changed return value to a string instead of BOOL
//***************************************************************************************
std::string GetDefaultPrinterName()
{
	//---------------------------------------------------
	//Prepare parameters required by the standard code
	const DWORD MAXBUFFERSIZE = 512; //not sure what this is supposed to be
	DWORD dwBufferSize = MAXBUFFERSIZE;
	char pPrinterName[MAXBUFFERSIZE];
	DWORD* pdwBufferSize = &dwBufferSize;
	//---------------------------------------------------

	BOOL bFlag;
	OSVERSIONINFO osv;
	TCHAR cBuffer[MAXBUFFERSIZE];
	PRINTER_INFO_2 *ppi2 = NULL;
	DWORD dwNeeded = 0;
	DWORD dwReturned = 0;

	// What version of Windows are you running?
	osv.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
	GetVersionEx(&osv);

	// If Windows 95 or 98, use EnumPrinters...
	if (osv.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS)
	{

		// The first EnumPrinters() tells you how big our buffer should
		// be in order to hold ALL of PRINTER_INFO_2. Note that this will
		// usually return FALSE. This only means that the buffer (the 4th
		// parameter) was not filled in. You don't want it filled in here...
		EnumPrinters(PRINTER_ENUM_DEFAULT, NULL, 2, NULL, 0, &dwNeeded, &dwReturned);
		if (dwNeeded == 0) 
		  return std::string();

		// Allocate enough space for PRINTER_INFO_2...
		ppi2 = (PRINTER_INFO_2 *)GlobalAlloc(GPTR, dwNeeded);
		if (!ppi2)
		  return std::string();

		// The second EnumPrinters() will fill in all the current information...
		bFlag = EnumPrinters(PRINTER_ENUM_DEFAULT, NULL, 2, (LPBYTE)ppi2, dwNeeded, &dwNeeded, &dwReturned);
		if (!bFlag)
		{
		  GlobalFree(ppi2);
		  return std::string();
		}

		// If given buffer too small, set required size and fail...
		if ((DWORD)lstrlen(ppi2->pPrinterName) >= *pdwBufferSize)
		{
		  *pdwBufferSize = (DWORD)lstrlen(ppi2->pPrinterName) + 1;
		  GlobalFree(ppi2);
		  return std::string();
		}

		// Copy printer name into passed-in buffer...
		lstrcpy(pPrinterName, ppi2->pPrinterName);

		// Set buffer size parameter to min required buffer size...
		*pdwBufferSize = (DWORD)lstrlen(ppi2->pPrinterName) + 1;

		GlobalFree(ppi2);
	}

	// If Windows NT, use the GetDefaultPrinter API for Windows 2000,
	// or GetProfileString for version 4.0 and earlier...
	else if (osv.dwPlatformId == VER_PLATFORM_WIN32_NT)
	{
#if(WINVER >= 0x0500)
		if (osv.dwMajorVersion >= 5) // Windows 2000 or later
		{
		  bFlag = GetDefaultPrinter(pPrinterName, pdwBufferSize);
		  if (!bFlag)
			return std::string();
		}

		else // NT4.0 or earlier
#endif
		{
		  // Retrieve the default string from Win.ini (the registry).
		  // String will be in form "printername,drivername,portname".
		  if (GetProfileString("windows", "device", ",,,", cBuffer, MAXBUFFERSIZE) <= 0)
			return std::string();
  
		  // Printer name precedes first "," character...
		  strtok(cBuffer, ",");
  
		  // If given buffer too small, set required size and fail...
		  if ((DWORD)lstrlen(cBuffer) >= *pdwBufferSize)
		  {
			*pdwBufferSize = (DWORD)lstrlen(cBuffer) + 1;
			return std::string();
		  }
  
		  // Copy printer name into passed-in buffer...
		  lstrcpy(pPrinterName, cBuffer);
  
		  // Set buffer size parameter to min required buffer size...
		  *pdwBufferSize = (DWORD)lstrlen(cBuffer) + 1;
		}
	}

	return pPrinterName;
}

//*************************************************************************************
void ShellBrowserAtAnchor
(void* vhWnd, const std::string& docname, const std::string& anchor, const char* reroute)
{
	HWND hWnd = (HWND) vhWnd;

	static const char* fileprefix = "file://"; //V2.28 firefox requires

	char exe[MAX_PATH];
	int rc = (int) FindExecutable(docname.c_str(), NULL, exe);

	if (rc > 32) {
		std::string url(docname);
		//util::ReplaceString(url, " ", "%20");

		if (anchor.length() > 0)
			url.append(1, '#').append(anchor);

		/*
		V2.24 Jan 2010.  Anchors note.
		Jump direct to anchor used to work but no longer seems to, at least on Vista+IE8.
		Apparently there is no way now to shell out IE and have it reliably go to an 
		anchor, so following various suggestions googled and used by other people in a
		similar situation, we instead create a temporary document which does an auto-recirect 
		to the anchor using a HTML HREF.  Rather tedious, and not guaranteed to be supported 
		on all browsers either.  Hence an option is to revert to the old way.
		*/
		int rc = 0;
		if (reroute) {

			//Create temp doc in local directory (which we know we're allowed to write)
			//rathter than doc directory, which maybe shared and protected.
			std::string tempdoc = "helptemp.html";
			{
				util::StdIOLineOutput lo(tempdoc, util::STDIO_CCLR);
				lo.WriteLine("<HTML>");
				lo.WriteLine("<!-- Temporary document to redirect from help button to doc chapter. >");
				lo.WriteLine("<!-- (Should support more browsers - see DPT V2.24 release notes).   >");
				lo.WriteLine("<HEAD>");
					lo.Write("  <meta http-equiv=\"Refresh\" content=\"0; url=");
						lo.Write(fileprefix); 
						lo.Write(url);
						lo.WriteLine("\">");
				lo.WriteLine("</HEAD>");
				lo.WriteLine("<BODY ALIGN=CENTER>");
					lo.WriteLine("<P>Browser redirect not working, please click this manual link:");
					lo.WriteLine("<P><P>");
					lo.Write("&gt&gt <A HREF=\"");
						lo.Write(url);
						lo.Write("\">");
						lo.Write(reroute);
						lo.WriteLine("</A> &lt&lt");
					lo.WriteLine("<P><P>(Alternatively see toggle in the DPT installation options menu)</P>");
				lo.WriteLine("</BODY>");
				lo.WriteLine("</HTML>");
			}

			char path[_MAX_PATH];
			_fullpath(path, tempdoc.c_str(), _MAX_PATH);
			tempdoc = std::string("\"").append(path).append("\""); //V2.28
			rc = (int) ShellExecute(hWnd, NULL, exe, tempdoc.c_str(), NULL, SW_SHOW);
		}

		//Here's the straightforward anchor way
		else {
//			url = std::string(fileprefix).append("\"").append(url).append("\""); //V2.28
			url = std::string("\"").append(fileprefix).append(url).append("\""); //V2.28
			rc = (int) ShellExecute(hWnd, NULL, exe, url.c_str(), NULL, SW_SHOW);
		}

		if (rc > 32)
			return;
	}

	std::string msg("Opening browser on ");
	msg.append(docname);
	msg.append("...\n\n>>> ");

	msg.append(win::GetShellRCString(rc));
	msg.append("\n\nTry changing the option settings?");

	MessageBox(hWnd, msg.c_str(),
		"Error Opening Documentation File", MB_OK | MB_ICONWARNING);
}
#endif

//***************************************************************************************
//The purpose of this is to provide a systematic way of getting the system to use 
//real thread switches instead of Sleep(0) if required.  This requirement came up when
//I started playing with thread priorities.  All the spin locks have a problem when
//they are held by a low priority thread - if the higher priority threads just use 
//Sleep(0), the lock holder never gets a chance to run and release the lock.
//I mentioned this in the DPT system manager's guide section on threading.
//***************************************************************************************
void Cede(bool force)
{
	if (force || real_thread_swap_on_cede)
		Sleep(1);
	else
		Sleep(0);
}
void SetCedeModeRealThreadSwap(bool b)
{
	real_thread_swap_on_cede = b;
}

//***************************************************************************************
//Return value is true if OK was selected.
//***************************************************************************************
#if defined(_BBHOST) || defined(_BBCLIENT)
bool BrowseForFontOptions
(CWnd* cwnd, std::string& name, int& size, bool& bold, bool& italic, bool allow_nonfixed)
{
	//Current font
	LOGFONT f;
	CHOOSEFONT cf;

	//This stuff specifies an initial entry to have selected in the chooser dialog.
	//We set it to match the current values of the variables passed in above.
	f.lfHeight = 0;
//	f.lfWidth = 0;
	f.lfEscapement = 0;
	f.lfOrientation = 0;
	f.lfStrikeOut = FALSE;
	f.lfUnderline = FALSE;
	f.lfCharSet = DEFAULT_CHARSET;
	f.lfOutPrecision = OUT_DEFAULT_PRECIS;
	f.lfClipPrecision = CLIP_DEFAULT_PRECIS;
	f.lfQuality = DEFAULT_QUALITY;
	f.lfPitchAndFamily = FIXED_PITCH | FF_DONTCARE;

	//Convert point size to character height to initialize the dialog
	int ppi = cwnd->GetDC()->GetDeviceCaps(LOGPIXELSY);
	int charheight = -(size * ppi) / 72;
	f.lfHeight = charheight;

	strcpy(f.lfFaceName, name.c_str());
	f.lfWeight = (bold) ? FW_BOLD : FW_NORMAL;
	f.lfItalic = (italic) ? TRUE : FALSE;

	cf.lStructSize = sizeof(CHOOSEFONT);
	cf.hwndOwner = cwnd->m_hWnd;
	cf.lpLogFont = &f;
	cf.Flags = 	CF_FORCEFONTEXIST | CF_INITTOLOGFONTSTRUCT | CF_NOSCRIPTSEL |
				CF_NOVERTFONTS | CF_SCREENFONTS | CF_SELECTSCRIPT;

	//V2.14 March 09.
	if (!allow_nonfixed)
		cf.Flags |= CF_FIXEDPITCHONLY;
	
	if (ChooseFont(&cf) == 0)
		return false;

	name = f.lfFaceName;
	size = cf.iPointSize / 10;
	bold = (f.lfWeight > FW_NORMAL);
	italic = (f.lfItalic) ? true : false;

	return true;
}
#endif

//***************************************************************************************

//V2.23 Nov 09.  Wine auto-detect proved unreliable as it relied on the specific kluge
//return value shown here, which is subject to change with Wine versions.
/*
//Initialize control flag.  
bool TestGMSSupport() {
	MEMORYSTATUS ms;
	GlobalMemoryStatus(&ms);
	return (ms.dwAvailVirtual != 0 &&
		//Wine returns total-64K ATOW for the avail member.
		ms.dwAvailVirtual != ms.dwTotalVirtual - 64*1024);
}
static bool use_GlobalMemoryStatus = TestGMSSupport();

// **********************
//Override that decision later if desired
void VirtualMemoryCalcFree_ForceUseGlobalMemoryStatus(bool flag) {use_GlobalMemoryStatus = flag;}
*/

//**********************
//V2.23 Nov 09.  Control flag is now a parameter not a global variable.
unsigned int VirtualMemoryCalcFree(bool use_GlobalMemoryStatus)
{
	//This winAPI function does the job, but (ATOW) is not fully implemented on Wine.
	if (use_GlobalMemoryStatus) {
		MEMORYSTATUS ms;
		GlobalMemoryStatus(&ms);
		return ms.dwAvailVirtual;
	}

	//Simulate the above.  This is many times slower, and increasing as more regions are
	//allocated (1 trip into kernel mode and back per VQ call I think).  Use sparingly!
	//NB: This route always seems to report 8192 bytes less than the above.  I have no 
	//idea why so For the time being I've kluged it by 8K here :-)
	unsigned int total_free = 8192;
	MEMORY_BASIC_INFORMATION mbi;
	
	char* addr = 0;
	while (VirtualQuery(addr, &mbi, sizeof(mbi))) {

		//Pages neither committed nor reserved
		if (mbi.State == MEM_FREE)
			total_free += mbi.RegionSize;

		addr += mbi.RegionSize;
	}

	return total_free;
}

//***************************************************************************************
std::string VirtualMemoryMap()
{
	std::string s;

//	unsigned int total_free = 0;
	MEMORY_BASIC_INFORMATION mbi;
	
	char* addr = 0;
	while (VirtualQuery(addr, &mbi, sizeof(mbi))) {

		if (addr > 0) 
			s.append("\n");

		s.append("Base: ");
		s.append(util::UlongToHexString((unsigned long)addr, 8));
		s.append("    Length:");
		s.append(util::SpacePad(mbi.RegionSize, 8, true, true, 0));
		s.append("    State:");
		
		if (mbi.State == MEM_FREE) 
			s.append("FREE");
		else {
			if (mbi.State == MEM_COMMIT) 
				s.append("COMMIT");
			else
				s.append("RESERVE");

			if (mbi.AllocationProtect == PAGE_READONLY)
				s.append(", READONLY");
			if (mbi.AllocationProtect == PAGE_READWRITE)
				s.append(", READWRITE");
			if (mbi.AllocationProtect == PAGE_WRITECOPY)
				s.append(", WRITECOPY");
			if (mbi.AllocationProtect == PAGE_EXECUTE)
				s.append(", EXECUTE");
			if (mbi.AllocationProtect == PAGE_EXECUTE_READ)
				s.append(", EXECUTE_READ");
			if (mbi.AllocationProtect == PAGE_EXECUTE_READWRITE)
				s.append(", EXECUTE_READWRITE");
			if (mbi.AllocationProtect == PAGE_EXECUTE_WRITECOPY)
				s.append(", EXECUTE_WRITECOPY");
			if (mbi.AllocationProtect == PAGE_GUARD)
				s.append(", GUARD");
			if (mbi.AllocationProtect == PAGE_NOACCESS)
				s.append(", NOACCESS");
			if (mbi.AllocationProtect == PAGE_NOCACHE)
				s.append(", NOCACHE");

			if (mbi.Type == MEM_IMAGE)
				s.append(", IMAGE");
			if (mbi.Type == MEM_MAPPED)
				s.append(", MAPPED");
			if (mbi.Type == MEM_PRIVATE)
				s.append(", PRIVATE");
		}
		
		addr += mbi.RegionSize;
	}

	return s;
}

//***************************************************************************************
int GetProcessorCount()
{
	SYSTEM_INFO sysinfo; 
	GetSystemInfo(&sysinfo); 
 
	return sysinfo.dwNumberOfProcessors; 
}

}} //close namespace
