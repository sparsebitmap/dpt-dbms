
#include "stdafx.h"

#include "audit.h"

#include <time.h>	
#include "sys\timeb.h"		//for _ftime etc.

//Utils
#include "dataconv.h"
#include "liostdio.h"
#include "iowrappers.h" //scan
#include "charconv.h"
#include "pattern.h" //scan
#include "winutil.h"
//API Tiers
#include "sysfile.h"
//Diagnostics
#include "except.h"
#include "msg_core.h"

#ifdef _BBHOST
#include "iodev.h"
#endif

namespace dpt {

//******************************************************************************************
Audit::Audit(const std::string& fn, util::LineOutput* secondary_audit, 
			 const std::string& versdpt, int ac, int ak)
: audctl(ac), audkeep(ak)
#ifdef _DEBUG_LOCKS
, write_lock("Audit file")
#endif
{
	filename = fn;
	alloc_handle = SystemFile::Construct("+AUDIT", filename, BOOL_EXCL);

	//V2.29 Allow read back.
	//thefile = new util::LockingStdIOLineOutput(filename.c_str(), util::STDIO_CCLR, audkeep);
	thefile = new util::LockingStdIOLineOutput(filename.c_str(), util::STDIO_CCLR, audkeep, 
		_SH_DENYWR, _S_IREAD | _S_IWRITE, true);

	try {
		//If this weren't done at this early stage, the very first few messages would 
		//not appear in the secondary file (window or whatever).
		thefile->SetSecondary(secondary_audit);
		std::string stars(50, '*');

		std::string versionmsg("DPT Version ");
		versionmsg.append(versdpt);
		versionmsg.append("  Copyright 2000-2014");
		thefile->WriteLine(stars);
		thefile->WriteLine(versionmsg);
		thefile->Write("Start of audit file, column layout options = ");
		thefile->WriteLine(util::UlongToHexString(audctl, 2, true));

		if (!(audctl & audctl_show_date)) {
			thefile->Write("Run date: ");
			time_t time_UTC;
			time(&time_UTC);
			tm time_local = win::GetDateAndTime_tm(time_UTC);
			char datebuff[16];
			sprintf(datebuff, "%.4d/%.2d/%.2d", 
				1900+time_local.tm_year, time_local.tm_mon+1, time_local.tm_mday);
			thefile->WriteLine(datebuff);
		}		
		thefile->WriteLine(stars);
	}
	catch (...) {
		SystemFile::Destroy(alloc_handle);
		delete thefile;
		throw;
	}
}

//******************************************************************************************
Audit::~Audit()
{
	LockingSentry s(&write_lock);

	thefile->WriteLine("**************************");
	thefile->WriteLine("End of audit file");
	thefile->WriteLine("**************************");

	SystemFile::Destroy(alloc_handle);
	delete thefile;
}

//******************************************************************************************
void Audit::WriteOutputLine
(const std::string& linedata, const int usernum, const char* linetype) 
{
	//Lock until the whole line has been written
	LockingSentry s(&write_lock);

	//Current time
	time_t time_UTC;

	//DPT: Rejigged this to deviate deliberately from M204 layout.  Optional milliseconds.
//	time(&time_UTC);

	//Second count as the time from the above function does not have milliseconds.
//	if (time_UTC == audit_previous_second) {
//		audit_second_count++;
//	}
//	else {
//		audit_previous_second = time_UTC;
//		audit_second_count = 1;
//	}

	//So use _ftime which gets us the system time and has milliseconds in it.
	_timeb systime;
	_ftime(&systime);

	//One member of _timeb is a UTC time which we can use just as per the previous code:
	time_UTC = systime.time;

	//Correct for local time, and also break out the components.
	tm time_local = win::GetDateAndTime_tm(time_UTC);

	//Format the required components of the time, plus the other prefix info

	//See DPT comment above
//	char prefixbuff[64];
//	int prefixlen;
//	if (parm_audctl & audctl_show_date)
//		prefixlen = sprintf(prefixbuff, "%.4d/%.2d/%.2d %.2d:%.2d:%.2d %.3d %.2d %.2s ", 
//			1900+time_local.tm_year, time_local.tm_mon+1, time_local.tm_mday,
//			time_local.tm_hour, time_local.tm_min, time_local.tm_sec,
//			audit_second_count, usernum, linetype);
//	else
//		prefixlen = sprintf(prefixbuff, "%.2d:%.2d:%.2d %.3d %.2d %.2s ", 
//			time_local.tm_hour, time_local.tm_min, time_local.tm_sec,
//			audit_second_count, usernum, linetype);

	std::string pref;
	pref.reserve(64);

	//Date
	if (audctl & audctl_show_date) {
		pref.append(util::IntToString(1900+time_local.tm_year));
		pref.append(1, '/');
		pref.append(util::ZeroPad(time_local.tm_mon+1, 2));
		pref.append(1, '/');
		pref.append(util::ZeroPad(time_local.tm_mday, 2));
		pref.append(1, ' ');
	}

	//Time
	if (audctl & audctl_show_time) {
		pref.append(util::ZeroPad(time_local.tm_hour, 2));
		pref.append(1, ':');
		pref.append(util::ZeroPad(time_local.tm_min, 2));
		pref.append(1, ':');
		pref.append(util::ZeroPad(time_local.tm_sec, 2));

		//Milliseconds
		if (audctl & audctl_show_millisecs) {
			pref.append(1, '.');
			pref.append(util::ZeroPad(systime.millitm, 3));
		}

		pref.append(1, ' ');
	}

	//User number
	if (audctl & audctl_show_usernum) {
		pref.append(util::ZeroPad(usernum, 2));
		pref.append(1, ' ');
	}

	//Line Type
	if (audctl & audctl_show_linetype) {
		pref.append(linetype);
		pref.append(1, ' ');
	}

	thefile->Write(pref);
	thefile->WriteLine(linedata.c_str(), linedata.length());
}


//******************************************************************************************
//V2.29
void Audit::ExtractLines
(std::vector<std::string>* result, const std::string& ifrom, const std::string& ito, 
 const std::string& ipattstring, bool pattcase, int maxlines) 
{
	result->clear();
	result->reserve(1000);

	//Prep parameters
	std::string from = MakeScanDateString(ifrom);
	std::string to = MakeScanDateString(ito);
	
	std::string pattstring(ipattstring);
	if (!pattcase)
		util::ToUpper(pattstring);
	util::Pattern patt((pattstring.length()) ? std::string("*").append(pattstring).append("*") : "*");

	maxlines = (maxlines > 0) ? maxlines : (maxlines < 0) ? INT_MAX : 1000;

	//Remember file position
	LockingSentry s(&write_lock);

	util::BBStdioFile f(thefile->GetHandle(), filename.c_str());
	_int64 fileend = f.TellI64();

	try {
		//It would be nice to rig up some way to avoid going to the start of the file,
		//but that's something for later if anybody complains about the speed.
		//e.g. We could maintain a std::map<time,filepos> every 10K lines when writing.
		f.SeekI64(0);
		result->push_back(std::string());

		//Ignore the first few lines if any date selection is going on
		if (from.length() || to.length()) {
			int prelines = (audctl & audctl_show_date) ? 4 : 5;
			while (prelines--)
				f.ReadLine(result->back());
		}

		//Read and compare each line
		int selected = 0;
		while (selected < maxlines) {
			if (f.ReadLine(result->back()))
				break;

			//Match the date/time.  We know by now there will be long enough lines.
			if (from.length()) {
				if (memcmp(result->back().c_str(), from.c_str(), from.length()) <= 0)
					continue;
			}

			if (to.length()) {
				if (memcmp(result->back().c_str(), to.c_str(), to.length()) >= 0)
					//No point scanning any further
					break;
			}

			if (pattstring.length()) {
				std::string matchline(result->back());
				if (!pattcase)
					util::ToUpper(matchline);
				if (!patt.IsLike(matchline))
					continue;
			}

			//Success!
			result->push_back(std::string());
			selected++;
		}

		f.SeekI64(fileend); 

		if (selected < maxlines)
			result->push_back(std::string("*** Audit scan complete: ")
				.append(util::IntToString(selected)).append(" line(s) selected ***"));
		else
			result->push_back(std::string("*** Scan terminated because MAXLINES limit reached (")
				.append(util::IntToString(maxlines)).append(") ***"));
	}
	catch (Exception&) {
		f.SeekI64(fileend); 
		throw;
	}
	catch (...) {
		f.SeekI64(fileend); 
		throw Exception(UTIL_MISC_FILEIO_ERROR, "Unknown error scanning audit file");
	}
}

//*******************************
std::string Audit::MakeScanDateString(const std::string& istr) 
{
	//The result will be a value we can just match against the first n chars of each line
	std::string str(istr);
	if (str.length() == 0)
		return str;

	//The audit trail will need at least the time in it
	Exception exp_parm(PARM_MISC, "Insufficient AUDCTL parameter settings to perform audit date/time scan");
	if (!(audctl & audctl_show_time))
		throw exp_parm;

	//Relative seconds format
	if (str[0] == '-') {
		time_t ttnow;
		time(&ttnow);
		int delta = util::StringToInt(str.substr(1));
		ttnow -= delta;
		tm tmnow = win::GetDateAndTime_tm(ttnow);

		str = std::string();

		//Build appropriate format exactly as in the write function
		if (audctl & audctl_show_date) {
			str.append(util::IntToString(1900+tmnow.tm_year));
			str.append(1, '/');
			str.append(util::ZeroPad(tmnow.tm_mon+1, 2));
			str.append(1, '/');
			str.append(util::ZeroPad(tmnow.tm_mday, 2));
			str.append(1, ' ');
		}
		str.append(util::ZeroPad(tmnow.tm_hour, 2));
		str.append(1, ':');
		str.append(util::ZeroPad(tmnow.tm_min, 2));
		str.append(1, ':');
		str.append(util::ZeroPad(tmnow.tm_sec, 2));

		return str;
	}

	//A date/time specified as an absolute value

	//Prepend today's date if no date part given and the audit trail has dates
	if (str.length() < 10 && (audctl & audctl_show_date)) {
		time_t ttnow;
		time(&ttnow);
		tm tmnow = win::GetDateAndTime_tm(ttnow);

		std::string datepart;
		datepart.append(util::IntToString(1900+tmnow.tm_year));
		datepart.append(1, '/');
		datepart.append(util::ZeroPad(tmnow.tm_mon+1, 2));
		datepart.append(1, '/');
		datepart.append(util::ZeroPad(tmnow.tm_mday, 2));
		datepart.append(1, ' ');

		str = std::string(datepart).append(str);
	}

	//Append 00 minutes and/or seconds if not given
	if (str.length() == 2 || str.length() == 13)
		str.append(":00:00");
	else if (str.length() == 5 || str.length() == 16)
		str.append(":00");

	//A nice use for the pattern matcher
	std::string spatt;
	if (str.length() == 19)
		spatt = "####!/##!/##+##:##:##";
	else if (str.length() == 8)
		spatt = "##:##:##";

	util::Pattern patt(spatt);
	if (!patt.IsLike(str))
		throw Exception(UTIL_PARSE_ERROR, "Invalid format given for date/time string");

	if (str.length() == 19) {
		if ( !(audctl & audctl_show_date))
			throw exp_parm;

		str[10] = ' '; //so we can just do memcmp on the audit lines
	}

	return str;
}


} // close namespace
