
#if !defined BB_CONST_GROUP
#define BB_CONST_GROUP

namespace dpt {

enum GroupType {
	NO_GROUP = 0,     //treated as ANY_GROUP in most functions
	PERM_GROUP = 1,
	TEMP_GROUP = 2,
	ANY_GROUP = 3
};

} //close namespace

#endif
