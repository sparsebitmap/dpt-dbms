
#include "stdafx.h"
#include "msgini.h"
#include <sstream>	//used in parsing the ini file 

//Utils
#include "liostdio.h"
#include "parsing.h"
#include "dataconv.h"
#include "charconv.h"
//API tiers
#include "sysfile.h"
//Diagnostics
#include "except.h"
#include "msg_core.h"

namespace dpt {

//define static members
bool MsgCtlIniSettings::created = false;

//****************************************************************************************
//Read in MSGCTL overrides from the ini file.  Luckily it's a fairly simple command, so 
//we can parse it in an ad-hoc way, and maybe revisit this when the full parsing
//functionality is available.  The format is:
//MSGCTL BB.n {TERM|NOTERM} {AUDIT|NOAUDIT} {CLASS{=}[E|I]} {RETCODE{=}n}
//****************************************************************************************
MsgCtlIniSettings::MsgCtlIniSettings
(const std::string& infn, MsgCtlRefTable* r) 
: reftable(r), data(std::map<int, MsgCtlOptions>()), num_overrides(0)
{
	if (created) 
		throw Exception(MISC_SINGLETON, "There can be only one msg IniSettings object");
	created = true;

	//Place an internal enqueue on the file just for the duration of this function
	std::string inifilename = infn;
	FileHandle alloc_handle = SystemFile::Construct("+MSGINI", inifilename, BOOL_EXCL);

	try {

		//Cater for missing ini file by creating an empty one
		try {
			util::StdIOLineInput sli(inifilename.c_str(), util::STDIO_OLD);
		}
		catch (...) {
			//No error code with stream IO, so assume the file was not there.
			//This NEW allocation will fail if it WAS actually there (e.g sharing violation)
			util::StdIOLineOutput slo(inifilename.c_str(), util::STDIO_NEW);
			slo.WriteLine("***************************************************");
			slo.WriteLine("* Place system-wide MSGCTL overrides in this file *");
			slo.WriteLine("***************************************************");
		}

		//Open the ini file
		util::StdIOLineInput inifile(inifilename.c_str(), util::STDIO_OLD);

		std::string line;

		//Read each line in
		while (true) {

			//eof
			if (inifile.ReadLine(line))
				break;

			//ignore blank lines
			if (line.length() == 0) 
				continue;

			//Ignore single comment lines (2 types - C style and M204 style).  Interestingly the
			//combined formulation below causes VC++ to report a memory leak if the first condition
			//is met, but not the second.  Possible compiler bug?
	//		if (line.substr(0, 2) == "//" || line.substr(0, 1) == "*") continue;
			if (line[0] == '*') 
				continue;
			else if (line.length() > 1) {
				if (line[0] == '/' && line[1] == '/') 
					continue;
			}

			//Uppercase the whole line in this situation
			util::ToUpper(line);

			//Somebody's bound to put tabs in there
			size_t tabpos = 0;
			while ((tabpos = line.find('\t', tabpos)) != std::string::npos)
				line[tabpos] = ' ';	

			std::string command = util::GetWord(line, 1);

			int msgnum;
			MsgCtlOptions mc;
			char msg[256];
			bool thrown = false;
			try {
				//Check the MSGCTL command name itself is present
				if (command != "MSGCTL") 
					throw "Not a MSGCTL command.";

				//The message number must be next
				std::string messageID = util::GetWord(line, 2);
				if (messageID.find("DPT.") != 0) 
					throw "Invalid message ID format - must be DPT.x";
				
				//Validate the number by retrieving the default settings for the message.  These will
				//also be used as a base onto which to apply the up-coming overrides.
				msgnum = atol(messageID.substr(4).c_str());
				mc = reftable->GetMsgCtl(msgnum);

				//Then get any other options in the string
				int pos = util::PositionOfWord(line, 3);
				if (pos == (int)std::string::npos)
					throw "No message control options given";

				mc = ParseMsgCtlCommand(line.substr(pos), mc);
			}

			//This is a bit klugey but saves repeating the big concatenation
			catch (const char* e) {
				strcpy(msg, e);
				thrown = true;
			}
			catch (Exception& e) {
				strcpy(msg, e.What().c_str());
				thrown = true;
			}

			if (thrown) {
				throw Exception(SYS_BAD_INIFILE, std::string
					("Error reading msgctl ini file, line='")
					.append(inifile.PrevLine())
					.append("'. ")
					.append(msg));
			}

			//All OK, so create an entry in the settings table (or overwrite if 2 for same message) 
			data[msgnum] = mc;

			//Note that at least one override has been specified
			num_overrides++;
		}

		SystemFile::Destroy(alloc_handle);
	}
	catch (...) {
		SystemFile::Destroy(alloc_handle);
		throw;
	}
}

//****************************************************************************************
//This is used during system start up
//****************************************************************************************
void MsgCtlIniSettings::SummarizeOverrides(std::vector<std::string>* result) const
{
	std::map<int, MsgCtlOptions>::const_iterator mci;
	for (mci = data.begin(); mci != data.end(); mci++) {
		std::string s("DPT.");
		s.append(util::IntToString(mci->first))
			.append(mci->second.term ? " TERM" : " NOTERM")
			.append(mci->second.audit ? " AUDIT" : " NOAUDIT")
			.append(mci->second.error ? " CLASS=E" : " CLASS=I")
			.append(" RETCODE=")
			.append(util::IntToString(mci->second.rcode));

		result->push_back(s);
	}
}

//****************************************************************************************
//Get hold of the current system MSGCTL settings.  This functions supplies the override
//value if there is one, otherwise the system default.
//****************************************************************************************
MsgCtlOptions MsgCtlIniSettings::GetMsgCtl(const int msgnum) const
{
	//(No need to lock as these structures are only ever read-only)

	//check the message number (let exception fly through)
	MsgCtlOptions ref = reftable->GetMsgCtl(msgnum);

	//Now check for an actual override
	std::map<int, MsgCtlOptions>::const_iterator mci = data.find(msgnum);

	if (mci == data.end()) 
		return ref;
	else
		return mci->second;
}

} //close namespace

