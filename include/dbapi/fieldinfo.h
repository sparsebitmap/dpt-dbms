
#if !defined(BB_API_FIELDINFO)
#define BB_API_FIELDINFO

#include <string>

#include "cursor.h"
#include "fieldatts.h"

namespace dpt {

class FieldAttributeCursor;

class APIFieldAttributeCursor : public APICursor {
public:
	FieldAttributeCursor* target;
	APIFieldAttributeCursor(FieldAttributeCursor*);
	APIFieldAttributeCursor(const APIFieldAttributeCursor&);
	//-----------------------------------------------------------------------------------

	int NumFields();

	std::string Name();
	APIFieldAttributes Atts();
	short FID();
};

} //close namespace

#endif
