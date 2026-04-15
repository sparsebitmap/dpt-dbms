
#include "stdafx.h"

#include "dbapi\core.h"
#include "core.h"
#include "file.h"

namespace dpt {

//***************************************
APIMsgRouter APICoreServices::GetRouter() {
	return APIMsgRouter(target->GetRouter());}
APIViewerResetter APICoreServices::GetViewerResetter() {
	return APIViewerResetter(target->GetViewerResetter());}
APIStatViewer APICoreServices::GetStatViewer() {
	return APIStatViewer(target->GetStatViewer());}
APIAccessController APICoreServices::GetAccessController() {
	return APIAccessController(target->GetAccessController());}

//***************************************
void APICoreServices::AuditLine(const std::string& text, const char* type)
{
	target->AuditLine(text, type);
}

//***************************************
//V2.29
void APICoreServices::ExtractAuditLines
(std::vector<std::string>* result, const std::string& from, const std::string& to, 
 const std::string& patt, bool pattcase, int maxlines)
{
	target->ExtractAuditLines(result, from, to, patt, pattcase, maxlines);
}

//***************************************
void APICoreServices::Quiesce(const APICoreServices& c)
{
	CoreServices::Quiesce(c.target);
}

void APICoreServices::Unquiesce(const APICoreServices& c)
{
	CoreServices::Unquiesce(c.target);
}

bool APICoreServices::IsQuiesceing()
{
	return CoreServices::IsQuiesceing();
}

void APICoreServices::ScheduleForBump(int bumpee, bool user_0_too)
{
	CoreServices::ScheduleForBump(bumpee, user_0_too);
}

bool APICoreServices::IsScheduledForBump()
{
	return target->IsScheduledForBump();
}


//***************************************
std::vector<std::string> APICoreServices::GetAllocatedFileNames(FileType ft)
{
	std::vector<FileHandle> info;

	AllocatedFile::ListAllocatedFiles(info, BOOL_SHR, ft);

	std::vector<std::string> result;
	for (size_t x = 0; x < info.size(); x++) {
		if (info[x].GetType() & ft)
			result.push_back(info[x].GetDD());
	}

	return result;
}


//***************************************
std::vector<int> APICoreServices::GetUsernos(const std::string& vs)
{
	return CoreServices::GetUsernos(vs);
}

int APICoreServices::GetUsernoOfThread(const unsigned int tid)
{
	return CoreServices::GetUsernoOfThread(tid);
}


//***************************************
const std::string& APICoreServices::GetUserID()
{
	return target->GetUserID();
}

util::LineOutput* APICoreServices::Output()
{
	return target->Output();
}

int APICoreServices::GetUserNo()
{
	return target->GetUserNo();
}

unsigned int APICoreServices::GetThreadID()
{
	return target->GetThreadID();
}

int APICoreServices::SetWT(int newwt)
{
	return target->SetWT(newwt);
}

int APICoreServices::GetWT()
{
	return target->GetWT();
}

//***************************************
void APICoreServices::Tick(const char* activity)
{
	target->Tick(activity);
}


//***************************************
bool APICoreServices::InteractiveYesNo(const std::string& prompt, bool default_response)
{
	return target->InteractiveYesNo(prompt, default_response);
}

void APICoreServices::RegisterInteractiveYesNoFunc
(bool (*userfunc) (const std::string&, bool, void*))
{
	target->RegisterInteractiveYesNoFunc(userfunc);
}

void APICoreServices::RegisterInteractiveYesNoObj(void* userobj)
{
	target->RegisterInteractiveYesNoObj(userobj);
}



} //close namespace


