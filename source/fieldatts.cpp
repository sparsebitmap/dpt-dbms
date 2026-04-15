
#include "stdafx.h"

#include "fieldatts.h"

//Utils
#include "parsing.h"
#include "dataconv.h"
//API Tiers
//Diagnostics
#include "msg_db.h"
#include "except.h"

namespace dpt {

unsigned char FieldAttributes::FLT = 1;
unsigned char FieldAttributes::INV = 2;
unsigned char FieldAttributes::UAE = 4;
unsigned char FieldAttributes::ORD = 8;
unsigned char FieldAttributes::ONM = 16;
unsigned char FieldAttributes::NM = 32; //V2.14
unsigned char FieldAttributes::BLOB = 64; //V3.0

//**********************************************************************************
FieldAttributes::FieldAttributes
(FieldStorageFormat storfmt, FieldBtreeType bttype, 
 FieldStorageUpdatePosition storupos, unsigned char s, bool nomerge)
: flags(0), splitpct(s), refcount(0)
{
	if (storfmt == FDEF_FLOAT) flags |= FLT;
	if (storfmt == FDEF_INVISIBLE) flags |= INV;
	if (storfmt == FDEF_BLOB) flags |= BLOB;      //V3.0

	if (storupos == FDEF_UPDATE_AT_END) flags |= UAE;

	if (bttype == FDEF_ORDERED_CHARACTER) flags |= ORD;
	if (bttype == FDEF_ORDERED_NUMERIC) {flags |= ORD; flags |= ONM;}

	if (nomerge) flags |= NM; //V2.14

	ValidityCheck();
}

//**************************************
FieldAttributes::FieldAttributes
(bool flt, bool inv, bool uae, bool ord, bool onm, unsigned char s, bool nm, bool b)
: flags(0), splitpct(s), refcount(0)
{
	if (flt) flags |= FLT;
	if (inv) flags |= INV;
	if (uae) flags |= UAE;
	if (ord) flags |= ORD;
	if (onm) flags |= ONM;
	if (nm)  flags |= NM; //V2.14
	if (b)   flags |= BLOB; //V2.14

	ValidityCheck();
}

//**********************************************************************************
void FieldAttributes::ValidityCheck(bool fromfile) const
{
	std::string msg;

	//Some attributes are incompatible, but note that we allow combinations such as 
	//INVISIBLE/FLOAT, or NORD/SPLITPCT=50 where one part is simply ignored.
	if (IsOrdered()) {
		//See tech notes for more reasoning behind this - it's just for initial simplicity.  
		if (splitpct < 1 || splitpct > 99)
			msg = "SPLITPCT must be a value from 1 to 99";
	}
	else {
		if (IsInvisible())
			msg = "Invisible fields must be indexed";
	}

	//V3.0.
	if (IsBLOB()) {
		if (!IsString())
			msg = "BLOB fields must be STRING";
		if (IsOrdered())
			msg = "BLOB fields may not be indexed";
	}

	if (msg.length() != 0) {
		if (fromfile)
			msg.append(" (possible file corruption)");
		throw Exception(DBA_FLDATT_INCOMPATIBLE, msg);
	}
}

//**********************************************************************************
void FieldAttributes::RedefineValidityCheck(const FieldAttributes& newatts) const
{
	//V2.19 June 09.  Large changes to what this function allows now that all the
	//redefine options are supported.
	const char* err = NULL;
	int bigchanges = 0;

	//Attributes requiring large changes to file data - only one at a time allowed
	if (IsVisible() != newatts.IsVisible())
		bigchanges++;
	
	if (IsFloat() && newatts.IsString() || IsString() && newatts.IsFloat())
		bigchanges++;
	
	if (IsBLOB() != newatts.IsBLOB()) //V3.0
		bigchanges++;
	
	if (IsOrdered() != newatts.IsOrdered())
		bigchanges++;

	if (IsOrdered() && newatts.IsOrdered() && IsOrdNum() != newatts.IsOrdNum())
		bigchanges++;

	if (bigchanges > 1)
		throw Exception(DBA_FLDATT_INCOMPATIBLE, 
			"Field attribute changes are too complex - issue one change at a time");
	
	//Other miscellaneous checks
	if (IsUpdateAtEnd() != newatts.IsUpdateAtEnd()) {
		if (!IsVisible() && !newatts.IsVisible())
			err = "You can not redefine the UP/UAE attribute for an invisisble field";
	}
	if (Splitpct() != newatts.Splitpct()) {
		if (!IsOrdered() && !newatts.IsOrdered())
			err = "You can not redefine SPLITPCT for a non-ordered field";
	}
	if (IsNoMerge() != newatts.IsNoMerge()) {
		if (!IsOrdered() && !newatts.IsOrdered())
			err = "You can not redefine NO MERGE for a non-ordered field";
	}
	
	if (flags == newatts.flags && splitpct == newatts.splitpct && bigchanges == 0)
		err = "No new attributes given, or all matched the existing attributes";

	if (err)
		throw Exception(DBA_FLDATT_INCOMPATIBLE, err);
}

//**********************************************************************************
bool FieldAttributes::GroupConsistencyCheck(const FieldAttributes& atts2) const
{
	//Splitpct and UP/UAE only affect internal storage, so let them be different
	unsigned char flags1 = flags;
	unsigned char flags2 = atts2.flags;

	flags1 |= UAE;
	flags2 |= UAE;

	return (flags1 == flags2);
}








//**************************************************************************************
void FieldAttsParser::BadParse(const std::string& msg)
{
	throw Exception(DBA_FIELDATTS_ERROR, msg);
}

//**************************************************************************************
void FieldAttsParser::ParseFieldAttributes
(const std::string& i, int cursor, std::string* sloadbadatts)
{
	std::string line(i);
	cursor = line.find_first_not_of(' ', cursor);
	if ((size_t)cursor == std::string::npos)
		return;

	int lbpos = cursor;
	std::string next_word = util::GetWord(line, 1, ' ', cursor);
	if (next_word == "WITH") {
		cursor += 4;
		line.append(1, ' ');

		if (line.find_first_of("()", cursor) != std::string::npos)
			BadParse("Field attributes should not be bracketed when using the 'WITH' syntax");
	}
	//Remove brackets if the bracketed format is used
	else if (next_word[0] == '(') {
		size_t rbpos = line.find(')', lbpos);
		if (rbpos == std::string::npos)
			BadParse("Missing right bracket after field attributes");

		int lastcharpos = line.find_last_not_of(' ');
		if (line[lastcharpos] != ')')
			throw Exception(DBA_FIELDATTS_ERROR, "Invalid extra option(s) at end of command");

		//Apart from the brackets the syntax after this is the same, so just remove them
		line[lbpos] = ' ';
		line[rbpos] = ' ';
	}
	else {
		BadParse("Field attributes must be in brackets or preceded by the keyword 'WITH'");
	}

	std::vector<std::string> tokens;

	//I'm allowing commas or no commas as desired in either format.
//* * * 
//M204 check required: Are equals signs allowed in e.g. SPLITPCT=50?  Currently yes here.
//* * * 
	util::Tokenize(tokens, line, " ,=", true, 0, 0, true, cursor);

	int num_unknown_atts = 0;
	for (size_t x = 0; x < tokens.size(); x++) {

		const std::string& s = tokens[x];
		std::string msg;

		if (s == "FLOAT")
			opt_float = true;

		else if (s == "STR" || s == "STRING")
			opt_string = true;

		else if (s == "VIS" || s == "VISIBLE")
			opt_visible = true;

		else if (s == "INV" || s == "INVISIBLE")
			opt_invisible = true;

		else if (s == "UP")
			opt_up = true;

		else if (s == "UE")
			opt_ue = true;

		//V2.19 Decided NORD is nicer.   See also the (ABBREV DDL) option in DisplayField
//		else if (s == "NON-ORD" || s == "NON-ORDERED") 
		else if (s == "NON-ORD" || s == "NON-ORDERED" || s == "NORD")
			opt_non_ordered = true;

		//V2.14 Jan 09. DPT custom option.
		else if (s == "NM")
			opt_no_merge = true;

		//V3.0 Nov 10.
		else if (s == "BLOB")
			opt_blob = true;
		else if (s == "NBLOB")
			opt_noblob = true;

		//-------------------
		//Multi-token options
		else if (s == "SPLT" || s == "SPLITPCT") {
			if (x == tokens.size() - 1)
				msg = "Missing SPLITPCT value";
			else {
				x++;
				opt_splitpct = util::StringToInt(tokens[x]);
			}
		}

		else if (s == "ORD" || s == "ORDERED") {
			opt_ordered = true;

			//The index type is optional - see later
			if (x != tokens.size() - 1) {
				int y = x + 1;
				const std::string& oitype = tokens[y];
				if (oitype == "CHAR" || oitype == "CHARACTER") {
					x = y;
					opt_ordchar = true;
				}
				else if (oitype == "NUM" || oitype == "NUMERIC") {
					x = y;
					opt_ordnum = true;
				}
				
			}
		}

		else if (s == "UPDATE") {
			if (x >= tokens.size() - 2)
				msg = "Missing or invalid UPDATE option";
			else {
				x++;
				std::string u1 = tokens[x];
				x++;
				std::string u2 = tokens[x];

				if (u1 == "IN" && u2 == "PLACE")
					opt_up = true;
				else if (u1 == "AT" && u2 == "END")
					opt_ue = true;
				else
					msg = "Invalid UPDATE option";
			}
		}

		//V2.14 Jan 09. DPT custom option.
		else if (s == "NO" || s == "DU") {
			if (x >= tokens.size() - 1)
				msg = "Missing or invalid defer option";
			else {
				x++;
				std::string n1 = tokens[x];

				if (n1 == "MERGE") {
					if (s == "NO")
						opt_no_merge = true;
					else
						opt_du_merge = true;
				}
				else
					msg = "Invalid defer option";
			}
		}

		//V3.0 Nov 10.
		else if (s == "BINARY-LARGE-OBJECT") {
			opt_blob = true;
		}

		else {
			msg = "Invalid or unsupported field attribute: ";
			msg.append(s);

			//Oct 09, in prep for V3.0.  Trying to let M204 DDL output through
			num_unknown_atts++;
			if (sloadbadatts)
				sloadbadatts->append(s).append(" ");
		}

		//See above.  Let bad atts through in a load.
		//if (msg.length() > 0)
		if (msg.length() > 0 && (!sloadbadatts || num_unknown_atts == 0))
			throw Exception(DBA_FIELDATTS_ERROR, msg);
	}

	//Check for contradictions here but leave other incompatibilities to the API.
	std::string contra_msg;
	if (opt_float && opt_string)
		contra_msg.append("STRING/FLOAT  ");
	if (opt_visible && opt_invisible)
		contra_msg.append("VISIBLE/INVISIBLE  ");
	if (opt_up && opt_ue)
		contra_msg.append("UPDATE IN PLACE/UPDATE AT END  ");
	if (opt_ordered && opt_non_ordered)
		contra_msg.append("ORDERED/NON-ORDERED  ");
	if (opt_ordnum && opt_ordchar)
		contra_msg.append("ORDERED NUMERIC/ORDERED CHARACTER  ");

	if (contra_msg.length() > 0) {
		throw Exception(DBA_FIELDATTS_ERROR, 
			std::string("Contradictory file attributes specified: ").append(contra_msg));
	}
}

//**************************************************************************************
FieldAttributes FieldAttsParser::MakeAtts()
{
	FieldAttributes result;

	//Set non-default flags if specified
	if (opt_float) 
		result.SetFloatFlag();
	if (opt_invisible) 
		result.SetInvisibleFlag();
	if (opt_ue) 
		result.SetUpdateAtEndFlag();
	if (opt_ordered) 
		result.SetOrderedFlag();
	if (opt_ordnum) 
		result.SetOrdNumFlag();
	if (opt_no_merge) 
		result.SetNoMergeFlag();
	if (opt_blob) 
		result.SetBLOBFlag();
	if (opt_splitpct != -1) 
		result.SetSplitPct(opt_splitpct);

	return result;
}

} //close namespace


