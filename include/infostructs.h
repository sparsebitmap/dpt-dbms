
#ifndef BB_INFOSTRUCTS
#define BB_INFOSTRUCTS

namespace dpt {

//*****************************************************************************
struct BTreeAnalyzeInfo {
	bool numeric;
	int rootpage;
	int depth;
	_int64 totvals;
	int leafpage_tot;
	int branchpage_tot;
	double leafpage_avefree_frac;
	double branchpage_avefree_frac;

	BTreeAnalyzeInfo() : numeric(false), rootpage(-1), depth(-1), totvals(0), 
						leafpage_tot(0), branchpage_tot(0),
						leafpage_avefree_frac(0.0), branchpage_avefree_frac(0.0) {}
};

//*****************************************************************************
struct InvertedListAnalyze1Info {
	_int64 val_unique;
	_int64 val_invlist;
	_int64 seg_unique;
	_int64 seg_semi;
	_int64 seg_multi;
	_int64 seg_comp;
	_int64 segrecs_unique;
	_int64 segrecs_semi;
	_int64 segrecs_multi;
	_int64 segrecs_comp;

	InvertedListAnalyze1Info() : val_unique(0), val_invlist(0),
		seg_unique(0), seg_semi(0), seg_multi(0), seg_comp(0),
		segrecs_unique(0), segrecs_semi(0), segrecs_multi(0), segrecs_comp(0) {}

	_int64 TotVals() {return val_unique + val_invlist;}
	_int64 TotILSegs() {return seg_unique + seg_semi + seg_multi + seg_comp;}
	_int64 TotILRecs() {return segrecs_unique + segrecs_semi + segrecs_multi + segrecs_comp;}
};

//*****************************************************************************
struct InvertedListAnalyze2Info {
	int		ilmr_totpages;
	_int64	ilmr_totrecs;
	double	ilmr_avefree_frac;
	int		il_totpages;
	_int64	il_totrecs;
	double	il_avefree_frac;

	InvertedListAnalyze2Info() : ilmr_totpages(0), ilmr_totrecs(0), ilmr_avefree_frac(0.0),
		il_totpages(0), il_totrecs(0), il_avefree_frac(0.0) {}
};

//*****************************************************************************
//This is for use in cases where we want to use the session level IODev
//as a pure line mode device rather than a "managed" one, with OUTLPP, OUTCCC,
//and headers etc. Most importantly these are cases where we want to dump a
//potentially large amount of info to the terminal without forcing the user
//to keep hitting enter every 22 lines.
//*****************************************************************************
#ifdef _BBHOST
class IODev;
#define BB_OPDEVICE IODev
#else
namespace util {class LineOutput;}
#define BB_OPDEVICE util::LineOutput
#endif


} //close namespace

#endif
