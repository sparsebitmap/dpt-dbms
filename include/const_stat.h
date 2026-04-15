
#if !defined BB_CONST_STAT
#define BB_CONST_STAT

namespace dpt {

enum StatLevel {
	STATLEVEL_SYSTEM_FINAL  = 0,
	STATLEVEL_USER_LOGOUT   = 1,
	STATLEVEL_USER_SL       = 2,
	STATLEVEL_FILE_CLOSE    = 3
};

}

#endif
