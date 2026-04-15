//****************************************************************************************
//Interface to the DPT password file
//****************************************************************************************

#if !defined(BB_API_ACCESS)
#define BB_API_ACCESS

#include <string>
#include <vector>

namespace dpt {

class APICoreServices;
class AccessController;

class APIAccessController {
public:
	AccessController* target;
	APIAccessController(AccessController*);
	APIAccessController(const APIAccessController&);
	//-----------------------------------------------------------------------------------

	unsigned int GetAccountPrivs(const std::string& name);
	bool CheckAccountPassword(const std::string& name, const std::string& pwd);

	void CreateAccount(const APICoreServices&, const std::string& name);
	void DeleteAccount(const APICoreServices&, const std::string& name);

	void ChangeAccountPassword(const APICoreServices&, 
							const std::string& name, const std::string& pwd);
	void ChangeAccountPrivs(const APICoreServices&, 
							const std::string& name, unsigned int privs);

	//These return arrays ordered by user name
	std::vector<std::string> GetAllAccountNames();
	std::vector<unsigned int> GetAllAccountPrivs();
	std::vector<std::string> GetAllAccountHashes();

	const std::string& LastUpdateUser();
	const std::string& LastUpdateTime();
};

} //close namespace

#endif
