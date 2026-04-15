
#if !defined(BB_MSGOPTS)
#define BB_MSGOPTS

namespace dpt {

struct MsgCtlOptions {
	bool term;
	bool audit;
	bool error;
	bool custom_option;
	int rcode;

	MsgCtlOptions() : term(true), audit(true), error(true), 
		custom_option(false), rcode(9999) {}
	MsgCtlOptions(bool t, bool a, bool i, int r, bool tr = false) 
		: term(t), audit(a), error(i), custom_option(tr), rcode(r) {}

	MsgCtlOptions(const MsgCtlOptions& o) 
		: term(o.term), audit(o.audit), error(o.error), 
		custom_option(o.custom_option), rcode(o.rcode) {}
};

} //close namespace

#endif
