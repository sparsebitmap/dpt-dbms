
#include "stdafx.h"

#include "dbapi\dbserv.h"
#include "dbapi\ctxtspec.h"
#include "dbserv.h"
#include "recovery.h"

namespace dpt {

APIDatabaseServices::APIDatabaseServices(
	util::LineOutput* output_destination,
	const std::string& userid,
	const std::string& parm_ini_filename,
	const std::string& msgctl_ini_filename,
	const std::string& audit_filename,
	util::LineOutput* secondary_audit)
{
	target = new DatabaseServices(
		output_destination, 
		userid, 
		parm_ini_filename,
		msgctl_ini_filename,
		audit_filename,
		secondary_audit);

	target->refcount = 1;
}

//***************************************
APIDatabaseServices::APIDatabaseServices(
	const std::string& output_filename,
	const std::string& userid,
	const std::string& parm_ini_filename,
	const std::string& msgctl_ini_filename,
	const std::string& audit_filename)
{
	target = new DatabaseServices(
		output_filename, 
		userid, 
		parm_ini_filename,
		msgctl_ini_filename,
		audit_filename);

	target->refcount = 1;
}

//***************************************
APIDatabaseServices::APIDatabaseServices(const APIDatabaseServices& from)
{
	target = from.target;
	target->refcount++;
}

//***************************************
APIDatabaseServices::APIDatabaseServices(DatabaseServices* from)
{
	//Used by the lockin sentry 
	target = from;
	target->refcount++;
}

//***************************************
APIDatabaseServices::~APIDatabaseServices()
{
	if (target) {
		target->refcount--;
		if (target->refcount == 0)
			delete target;
	}
}

//V2.06 - Jun 07.
void APIDatabaseServices::Destroy() {target->Destroy();}

//***************************************
//V2.27 April 2010.
void APIDatabaseServices::CreateAndChangeToUniqueWorkingDirectory(bool delete_at_closedown)
{
	DatabaseServices::CreateAndChangeToUniqueWorkingDirectory(delete_at_closedown);
}

//***************************************
APICoreServices APIDatabaseServices::Core() {return APICoreServices(target->Core());}
APIGroupServices APIDatabaseServices::GrpServs() {return APIGroupServices(target->Groups());}
APISequentialFileServices APIDatabaseServices::SeqServs() {return APISequentialFileServices(target->SeqFiles());}

//***************************************
void APIDatabaseServices::Allocate(const std::string& dd, const std::string& dsn, 
	FileDisp disp, const std::string& alias)
{
	target->Allocate(dd, dsn, disp, alias);
}
void APIDatabaseServices::Free(const std::string& dd)
{
	target->Free(dd);
}

//***************************************
void APIDatabaseServices::Create(const std::string& dd,
	int bsize, int brecppg, int breserve, int breuse, 
	int dsize, int dreserve, int dpgsres, int fileorg)
{
	target->Create(dd, bsize, brecppg, breserve, breuse, dsize, dreserve, dpgsres, fileorg);
}

//*****************************************************************************************
//V2.14 Jan 09.  Rejigged/simplified these 3 OpenContext APIs.
APIDatabaseFileContext APIDatabaseServices::OpenContext(const APIContextSpecification& spec)
{
	return APIDatabaseFileContext(target->OpenContext(*(spec.target)));
}

//***************************************
APIDatabaseFileContext APIDatabaseServices::OpenContext_DUMulti(
		const APIContextSpecification& spec,
		const std::string& duname_n, const std::string& duname_a, int dunumlen, DUFormat du_format)
{
	//V2.23 reinstated the DUFormat parameter which got lost (in 2.14 I think - see above comment).
//	return APIDatabaseFileContext(
//		target->OpenContext_DUMulti(*(spec.target), duname_n, duname_a, dunumlen));
	return APIDatabaseFileContext(
		target->OpenContext_DUMulti(*(spec.target), duname_n, duname_a, dunumlen, du_format));
}

//***************************************
APIDatabaseFileContext APIDatabaseServices::OpenContext_DUSingle(
		const APIContextSpecification& spec)
{
	return APIDatabaseFileContext(target->OpenContext_DUSingle(*(spec.target)));
}

//*****************************************************************************************
APIDatabaseFileContext APIDatabaseServices::FindOpenContext(const APIContextSpecification& spec) const
{
	return APIDatabaseFileContext(target->FindOpenContext(*(spec.target)));
}

std::vector<APIDatabaseFileContext> APIDatabaseServices::ListOpenContexts
(bool include_singles, bool include_groups) const
{
	std::vector<DatabaseFileContext*> temp = 
		target->ListOpenContexts(include_singles, include_groups);

	std::vector<APIDatabaseFileContext> result;
	for (size_t x = 0; x < temp.size(); x++)
		result.push_back(APIDatabaseFileContext(temp[x]));

	return result;
}

bool APIDatabaseServices::CloseContext(const APIDatabaseFileContext& c)
{
	return target->CloseContext(c.target);
}

void APIDatabaseServices::CloseAllContexts(bool force)
{
	target->CloseAllContexts(force);
}

//***************************************
void APIDatabaseServices::Commit(bool if_backoutable, bool if_nonbackoutable)
{
	target->Commit(if_backoutable, if_nonbackoutable);
}

void APIDatabaseServices::Backout(bool discreet)
{
	target->Backout(discreet);
}

void APIDatabaseServices::AbortTransaction()
{
	target->AbortTransaction();
}

bool APIDatabaseServices::UpdateIsInProgress()
{
	return target->UpdateIsInProgress();
}

bool APIDatabaseServices::TBOIsOn()
{
	return DatabaseServices::TBOIsOn();
}

bool APIDatabaseServices::ForceBatchCommit()
{
	return DatabaseServices::ForceBatchCommit();
}


//***************************************
void APIDatabaseServices::Checkpoint(int cpto)
{
	target->Checkpoint(cpto);
}

bool APIDatabaseServices::ChkpIsEnabled()
{
	return DatabaseServices::ChkpIsEnabled();
}

bool APIDatabaseServices::ChkAbortRequest()
{
	return target->ChkAbortRequest();
}

int APIDatabaseServices::GetNumTimedOutChkps()
{
	return target->GetNumTimedOutChkps();
}

time_t APIDatabaseServices::GetLastChkpTime()
{
	return DatabaseServices::GetLastChkpTime();
}

time_t APIDatabaseServices::GetCurrentChkpTime()
{
	return DatabaseServices::GetCurrentChkpTime();
}

time_t APIDatabaseServices::GetNextChkpTime()
{
	return DatabaseServices::GetNextChkpTime();
}

//***************************************
int APIDatabaseServices::Rollback1()
{
	return target->Rollback1();
}

void APIDatabaseServices::Rollback2()
{
	target->Rollback2();
}

int APIDatabaseServices::RecoveryFailedCode()
{
	return Recovery::FailedCode();
}

const std::string& APIDatabaseServices::RecoveryFailedReason()
{
	return Recovery::FailedReason();
}


//***************************************
int APIDatabaseServices::Tidy(int bufage)
{
	return target->Tidy(bufage);
}

//***********************************************************************************
APIUserLockInSentry::APIUserLockInSentry(int usernum)
: other(NULL)
{
	DatabaseServices* dbapi = DatabaseServices::GetHandleAndLockIn(usernum);
	if (dbapi)
		other = new APIDatabaseServices(dbapi);
}

APIUserLockInSentry::~APIUserLockInSentry()
{
	if (other) {
		other->target->LetOut();
		delete other;
	}
}

} //close namespace
