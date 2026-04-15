
#include "stdafx.h"

#include "parmini.h"

#include <sstream>	//used in parsing the ini file

//Utils
#include "liostdio.h"
#include "dataconv.h"
#include "charconv.h"
#include "parsing.h"
//API tiers
#include "sysfile.h"
#include "parmref.h"
//Diagnistics
#include "except.h"
#include "msg_core.h"

namespace dpt {

//define static members
bool ParmIniSettings::created = false;

//****************************************************************************************
//Read in the values from the ini file
//****************************************************************************************
ParmIniSettings::ParmIniSettings
(const std::string& infn, ParmRefTable* r) 
: reftable(r),
  some_used_max_or_min(false),
  num_overrides(0)
{
	if (created) 
		throw Exception(MISC_SINGLETON, "There can be only one parm IniSettings object");
	created = true;
	
	//Place an internal enqueue on the file just for the duration of this function
	std::string inifilename = infn;
	FileHandle alloc_handle = SystemFile::Construct("+PARMINI", inifilename, BOOL_EXCL);

	try {

		//Cater for missing ini file by creating an empty one
		try {
			util::StdIOLineInput sli(inifilename.c_str(), util::STDIO_OLD);
		}
		catch (...) {
			//No error code with stream IO, so assume the file was not there.
			//This NEW allocation will fail if it WAS actually there (e.g sharing violation)
			util::StdIOLineOutput slo(inifilename.c_str(), util::STDIO_NEW);
			slo.WriteLine("******************************************************");
			slo.WriteLine("* Place system-wide parameter overrides in this file *");
			slo.WriteLine("******************************************************");
		}

		//It should exist now, even if empty
		util::StdIOLineInput inifile(inifilename.c_str(), util::STDIO_OLD);
		
		for (;;) {
			std::string line;
			std::string parmname;
			std::string parmval;
			std::string useval;

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
				if (line[0] == '/' && line[1] =='/') 
					continue;
			}

			//Strip out the first equals sign if there is one
			//V2 - 25/9/06 - Unnecessary now tokenize is used below.
//			if (int eqpos = line.find('=')) {
//				if (eqpos != std::string::npos) 
//					line[eqpos] = ' ';
//			}

			//Somebody's bound to put tabs in there
			size_t tabpos = 0;
			while ((tabpos = line.find('\t', tabpos)) != std::string::npos)
				line[tabpos] = ' ';	

			//Read two strings.  Anything after this is ignored and can be comments if desired.
			//V2 - 25/9/06 - Tokenize allows quoted values with spaces (see also above).
			//parmname = util::GetWord(line, 1);
			//parmval = util::GetWord(line, 2);
			std::vector<std::string> tokens;
			util::Tokenize(tokens, line, " =", true, '\'', false, true);
			if (tokens.size() > 0)
				parmname = tokens[0];
			if (tokens.size() > 1)
				parmval = tokens[1];

			bool hexchar_supplied = false;
			bool hexnum_supplied = false;
			bool charquotes_supplied = false;

			//Parm name is always uppercased
			util::ToUpper(parmname);

			//Check it's a valid parameter name and value.  This function replaces values
			//outside valid numeric ranges with the appropriate maximum or minimum.  It will
			//also throw if the value is invalid or not resettable.
			try {
				ParmRefInfo refinfo = reftable->GetRefInfo(parmname);

				std::string pref = parmval.substr(0, 2);

				//New parameter values can be specified as e.g. C'XYZ' or X'FF'
				if (pref == "C\'" || pref == "c\'") {
					charquotes_supplied = true;
					parmval.erase(0, 2);
					if (parmval[parmval.length() - 1] == '\'') 
						parmval.erase(parmval.length() - 1);
				}
				//Of just 'abc' which is the same as C'abc'
				else if (parmval[0] == '\'') {
					charquotes_supplied = true;
					parmval.erase(0, 1);
					if (parmval[parmval.length() - 1] == '\'') 
						parmval.erase(parmval.length() - 1);
				}
				else if (pref == "X\'" || pref == "x\'") {
					parmval.erase(0, 2);
					if (parmval[parmval.length() - 1] == '\'') 
						parmval.erase(parmval.length() - 1);

					//Conversion is different for alpha and numeric parameters
					if (refinfo.type == num) {
						hexnum_supplied = true;
						parmval = util::UlongToString(util::HexStringToUlong(parmval));
					}
					else {
						hexchar_supplied = true;
						parmval = util::HexStringToAsciiString(parmval);
					}
				}
				//V2 17/9/06 
				//Only uppercase if not supplied quoted
				else {
					util::ToUpper(parmval);
				}

				//V2.12 Special case for code page values as unfortunately the pattern
				//matcher accepts a max repeat of 255 and we need 256!  No big deal - we can
				//go straight to reset here as these aren't really parameters as such.
				if (parmname == "CODESA2E")
					util::SetCodePageA2E(useval = parmval);
				else if (parmname == "CODESE2A")
					util::SetCodePageE2A(useval = parmval);
				else
					useval = reftable->ValidateResetDetails(parmname, parmval, true);
			}
			catch (Exception& e) {
				std::string text = e.What();
				util::ReplaceChar(text, '\0', ' ');
				throw Exception(e.Code(), std::string
					("Error reading parameter ini file, line='")
					.append(inifile.PrevLine())
					.append("'. ")
					.append(text));		
			}

			//Note that at least one override has been specified
			num_overrides++;
			
			//Record if the max or min was used
			bool used_max_or_min = false;
			if (useval != parmval) {
				used_max_or_min = true;
				some_used_max_or_min = true;
			}

			//load up the map representing this run (duplicates just overwrite eachother)
			data[parmname] = ParmIniVal(useval, used_max_or_min, 
				hexchar_supplied, hexnum_supplied, charquotes_supplied);
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
void ParmIniSettings::SummarizeOverrides(std::vector<std::string>* result) const
{
	std::map<std::string, ParmIniVal>::const_iterator mci;
	for (mci = data.begin(); mci != data.end(); mci++) {
		std::string s = std::string(mci->first).append(1, '=');

		//V2 17/9/06 
		//Nice to echo back in similar format to how they were specified
		std::string v = mci->second.value;
		util::ReplaceChar(v, '\0', ' ');
		if (mci->second.hexchar_supplied) {
			s.append("X'").append(util::AsciiStringToHexString(v)).append("'");
			s.append(" ('").append(v).append("')");
		}
		else if (mci->second.hexnum_supplied)
			s.append(util::UlongToHexString(util::StringToInt(v), 0, true));
		else if (mci->second.charquotes_supplied)
			s.append("'").append(v).append("'");
		else
			s.append(v);
	
		//Highlight ones where the supplied value was not actually used
		if (mci->second.used_max_or_min) 
			s.append("(*)");

		result->push_back(s);
	}
}

//****************************************************************************************
//This function sort of pretends that it has a value for every parameter
//when in fact it gets them as needed from the overall defaults table if required.
//****************************************************************************************
//const std::string& ParmIniSettings::GetParmValue(const std::string& parmname) const //V3.0
const std::string& ParmIniSettings::GetParmValue
(const std::string& parmname, const std::string* override_default) const
{
	//(No need to lock as these structures are only ever read-only)

	//check the parameter name (let exception fly through)
	const ParmRefInfo& ref = reftable->GetRefInfo(parmname);

	//Now check for an actual override
	std::map<std::string, ParmIniVal>::const_iterator mci = data.find(parmname);
	
	if (mci == data.end()) {
		if (override_default)
			return *override_default;
		else
			return ref.default_value;
	}
	else { 
		return mci->second.value;
	}
}

//****************************************************************************************
bool ParmIniSettings::ParmIsNumeric(const std::string& parmname) const
{
	const ParmRefInfo& ref = reftable->GetRefInfo(parmname);
	return (ref.type == num);
}

} //close namespace

