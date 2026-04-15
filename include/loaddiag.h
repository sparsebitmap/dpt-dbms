
//Simple message-issuing object shared by deferred-update processing, and fast unloads and loads.

#if !defined(BB_LOADDIAG)
#define BB_LOADDIAG

#include <string>

namespace dpt {

class CoreServices;
class StatViewer;

const int LOADDIAG_DEFAULT = 0;
const int LOADDIAG_NONE    = 1;
const int LOADDIAG_LOW     = 2;
const int LOADDIAG_MED     = 3;
const int LOADDIAG_HIGH    = 4;
const int LOADDIAG_FINAL   = LOADDIAG_LOW;
const int LOADDIAG_BRIEF   = LOADDIAG_LOW;
const int LOADDIAG_CONCISE = LOADDIAG_LOW;
const int LOADDIAG_CHUNK   = LOADDIAG_MED;
const int LOADDIAG_VERBOSE = LOADDIAG_HIGH;

class LoadDiagnostics {
	int level;
	bool doseps;
	CoreServices* core;

	void Audit(CoreServices*, const std::string&, int);
	std::string Sep(char c = '*') {return std::string(70, c);}

public:
	//V2.19 This is now just incremental levels instead if bit switches.
	LoadDiagnostics(int l) : level(l), doseps(true) {}

	int GetLevel() {return level;}
	void SetLevel(int l) {level = l;}

	bool IsBrief() {return (level == LOADDIAG_BRIEF);}
	bool IsVerbose() {return (level == LOADDIAG_VERBOSE);}
	bool Any() {return (level > LOADDIAG_NONE);}
	bool IsHigh() {return IsVerbose();}

	void AuditForce(CoreServices* c, const std::string& s) {Audit(c, s, LOADDIAG_NONE);}
	void AuditForceSep(CoreServices* c, char ch = '*') {if (doseps) Audit(c, Sep(ch), LOADDIAG_NONE);}
	void AuditFinal(CoreServices* c, const std::string& s) {Audit(c, s, LOADDIAG_FINAL);}
	void AuditFinalSep(CoreServices* c, char ch = '*') {if (doseps) Audit(c, Sep(ch), LOADDIAG_FINAL);}
	void AuditChunk(CoreServices* c, const std::string& s) {Audit(c, s, LOADDIAG_CHUNK);}
	void AuditBrief(CoreServices* c, const std::string& s) {AuditFinal(c, s);}
	void AuditBriefSep(CoreServices* c, char ch = '*') {if (doseps) AuditFinalSep(c, ch);}
	void AuditVerbose(CoreServices* c, const std::string& s) {Audit(c, s, LOADDIAG_VERBOSE);}
	void AuditVerboseSep(CoreServices* c, char ch = '*') {if (doseps) Audit(c, Sep(ch), LOADDIAG_VERBOSE);}
	void StartSLStats(StatViewer*, const std::string& = "BLDX");

	//V3.0 For fast load/unload, just makes for neater calling.
	void SetCorePtr(CoreServices* c) {core = c;}

	//Alternate names
	void AuditLow(const std::string& s) {AuditFinal(core, s);}
	void AuditLowSep(char ch = '*') {AuditBriefSep(core, ch);}
	void AuditMed(const std::string& s) {AuditChunk(core, s);}
	void AuditHigh(const std::string& s) {AuditVerbose(core, s);}
	void AuditHighSep(char ch = '*') {if (doseps) AuditVerboseSep(core, ch);}

	//Less separators in fastload mode
	void EnableSeparators() {doseps = true;}
	void DisableSeparators() {doseps = false;}
};

} //close namespace

#endif
