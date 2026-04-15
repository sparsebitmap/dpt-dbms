
#include "stdafx.h"

#include "dbapi\access.h"
#include "access.h"

#include "dbapi\core.h"

namespace dpt {

APIAccessController::APIAccessController(AccessController* t) 
: target(t) {}
APIAccessController::APIAccessController(const APIAccessController& t) 
: target(t.target) {}

//********************************************
unsigned int APIAccessController::GetAccountPrivs(const std::string& name)
{
	return target->GetAccountPrivs(name);
}

bool APIAccessController::CheckAccountPassword
(const std::string& name, const std::string& pwd)
{
	return target->CheckAccountPassword(name, pwd);
}


//********************************************
void APIAccessController::CreateAccount
(const APICoreServices& core, const std::string& name)
{
	target->CreateAccount(core.target, name);
}

void APIAccessController::DeleteAccount
(const APICoreServices& core, const std::string& name)
{
	target->DeleteAccount(core.target, name);
}


//********************************************
void APIAccessController::ChangeAccountPassword
(const APICoreServices& core, const std::string& name, const std::string& pwd)
{
	target->ChangeAccountPassword(core.target, name, pwd);
}

void APIAccessController::ChangeAccountPrivs
(const APICoreServices& core, const std::string& name, unsigned int privs)
{
	target->ChangeAccountPrivs(core.target, name, privs);
}


//********************************************
std::vector<std::string> APIAccessController::GetAllAccountNames()
{
	return target->GetAllAccountNames();
}

std::vector<unsigned int> APIAccessController::GetAllAccountPrivs()
{
	return target->GetAllAccountPrivs();
}

std::vector<std::string> APIAccessController::GetAllAccountHashes()
{
	return target->GetAllAccountHashes();
}


//********************************************
const std::string& APIAccessController::LastUpdateUser()
{
	return target->LastUpdateUser();
}

const std::string& APIAccessController::LastUpdateTime()
{
	return target->LastUpdateTime();
}


} //close namespace


