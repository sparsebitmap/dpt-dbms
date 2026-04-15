
#include "stdafx.h"

//(Move this file up one directory level to use the "unwrapped" API.)
#include "dptdb.h"
using namespace dpt;

int main(int argc, char* argv[])
{
 	APIDatabaseServices db("CONSOLE");

	db.Core().Output()->WriteLine(
			std::string("'Hello World' from DPT version ")
			.append(db.Core().GetViewerResetter().View("VERSDPT"))
			.append("!")
		);

	int rc = db.Core().GetRouter().GetJobCode();
	printf("*\nEnd of program, rc = %d", rc);

	fflush(stdout);
	char ch[100];
	gets(ch);
	return rc;
}

