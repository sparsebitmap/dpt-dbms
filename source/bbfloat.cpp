
#include "stdafx.h"

#include "bbfloat.h"

#include "dataconv.h"
#include "except.h"
#include "msg_util.h"
#include "string.h"
#include <float.h>
#include <math.h> //V2.24 Many funcs like _nextafter() moved in gcc42 - Roger M.

#ifdef _BBHOST
#include "dataconv2.h"
#endif

namespace dpt {

const double RangeCheckedDouble::MAXIMUM_POSITIVE_VALUE = 9.99999999999999E75;
const double RangeCheckedDouble::MINIMUM_POSITIVE_VALUE = 1.0E-75;
const double RangeCheckedDouble::MAXIMUM_NEGATIVE_VALUE = -9.99999999999999E75;
const double RangeCheckedDouble::MINIMUM_NEGATIVE_VALUE = -1.0E-75;

bool RangeCheckedDouble::thrownumrange = false;
bool RangeCheckedDouble::allowhex = false;

//***************************************************************************************
void RangeCheckedDouble::Set(const double& d)
{
	//V2.11.  Jan 08.  Fixed the 2.04.
	if (d == 0.0)
		data = d;
	else

	//V2.04 Mar 07.  This is more functional for the API even though in UL we set to 0.
	if (TooLargeNumber(d)) {
		if (thrownumrange) {
			if (d > 0.0)
				throw Exception(UTIL_DATACONV_RANGE, "Number is >= 1E76");
			else
				throw Exception(UTIL_DATACONV_RANGE, "Number is <= -1E76");
		}
		data = 0.0;
	}
	else if (TooSmallNumber(d)) {
		if (thrownumrange)
			throw Exception(UTIL_DATACONV_RANGE, "Number is smaller than 1E-76");
		data = 0.0;
	}
	else
		data = d;
}

//***************************************************************************************
void RangeCheckedDouble::Set(const std::string& s, bool assignment)
{
	try {
		//In this common case we can save a few instructions
		if (s.length() == 0)
			data = 0.0;
		else 
			//data = util::StringToPlainDouble(s); //V3.03
			data = util::StringToPlainDouble(s, NULL, allowhex);

		if (data == 0.0)
			return;

		if (TooLargeNumber(data) || TooSmallNumber(data)) {
			std::string msg("Invalid numeric literal: '");
			msg.append(s);
			if (TooLargeNumber(data)) {
				if (data > 0.0)
					msg.append("' is >= 1E76");
				else
					msg.append("' is <= -1E76");
			}
			else {
				msg.append("' is smaller than 1E-76");
			}

			throw Exception(UTIL_DATACONV_RANGE, msg);
		}
	}
	catch (Exception& e) {
		std::string msg(e.What());

		if (assignment) {
			msg.append(" - zero assumed");
			data = 0.0;
		}

		throw Exception(e.Code(), msg);
	}
}

//**********************************************************************************
//This version's used in the PRINT statement and the debugger DISPLAY etc.
//**********************************************************************************
std::string RangeCheckedDouble::ToStringForDisplay() const
{
	//Common case
	if (IsZero())
		return std::string(1, '0');

	//The largest possible number has 75 digits and smallest or (2+74+15) = 91 digits
	char buff[128];

	ToStringBufferForDisplay(buff);
	return buff;
}

//**********************************************************************************
//Can be called independently for more efficiency if desired
//**********************************************************************************
int RangeCheckedDouble::ToStringBufferForDisplay(char* buff) const
{
	//Common case
	if (IsZero()) {
		buff[0] = '0';
		buff[1] = 0;
		return 1;
	}

	//Using "G" format is the only way I can see to get printf to use significant digits 
	//rather than decimal places as the controller of precision, so I've gone with that
	//in most cases, except where the exponent >15 or <4, where G gives an Eformat 
	//number, which would be unlike M204.  In those cases things are all happening to 
	//the left or right of the decimal point, so klugery is 'acceptably' neat.  These are
	//uncommon cases so the RT overhead is usually just 3* FP comparison, which is OK
	//as the implication of this func is that some kind of output is happening anyway.

	if (data < 1E-4 && data > -1E-4) {
		//Max exponent we are allowing elsewhere is 75.  See next func re _nextafter().
		int ix = sprintf(buff, "%.89f", _nextafter(data, (data > 0.0) ? DBL_MAX : -DBL_MAX ));

		//Trim (probably quite a lot of) zero padding created by that
		for (ix--; ix >= 0; buff[ix--] = 0)
			if (buff[ix] != '0')
				break;

		//Always chop the final nonzero digit as it's almost certainly noise from _nextafter()
		buff[--ix] = 0;

		//Then trim "real" trailing zeroes
		for (ix--; ix >= 0; buff[ix--] = 0)
			if (buff[ix] != '0')
				break;

		//In case all DP are lost
		if (buff[ix] == '.')
			buff[--ix] = 0;

		return ix + 1;
	}
	else if (data >= 1E15 || data < -1E15) {
		//All 15 DSF must therefore be non-fractional, so 0DP is OK
		int ix = sprintf(buff, "%.0f", _nextafter(data, (data > 0.0) ? DBL_MAX : -DBL_MAX ));

		//Zero any noise or DSF > 15 (see other comments)
		for (char* c = buff + ((data < 0) ? 16 : 15); *c != 0; c++)
			*c = '0';

		return ix;
	}
	//Hopefully the most common case - leave all the klugery to the FP chip.
	else
		return sprintf(buff, "%.15G", data);
}

//***********************************************************************************
//This function is used when dealing with FIXED and STRING variables and temporaries,
//where we want the value padded out to the appropriate number of DP, and extra DP
//to be truncated, not rounded.
//Unfortunately neither printf nor _fcvt respects rounding modes set via _controlfp()
//and always round.  Therefore we can't directly use the "f" mode of printf with the
//required number of dp (which might otherwise have been nice because it pads 
//out with zeros).   Instead we request lots of DP then truncate manually.
//My strategy is to request 27 DP, so that even if the tinyest of 18 dsf gets rounded
//(e.g. in 0.000000000999999999999999999) the rounding will still not affect the
//9th decimal place.  i.e. We want only truncation to be visible to the  UL user.
//***********************************************************************************

//V2.26 share with string2 overload
//std::string RangeCheckedDouble::ToStringWithFixedDP(const int dp) const
const char* RangeCheckedDouble::ToStringWithFixedDP(const int dp, char* buff) const
{
	if (dp > 9 || dp < 0)
		throw Exception(BUG_MISC, "Bug: Invalid DP in ToStringWithFixedDP");

	//The maximum length would be with exponents of 75.
//	char buff[128];

	//We have a slight problem when the data value is "supposed" to be something like
	//6.0 but the internal representation has it as 5.9999999999999999.  Common sense 
	//suggests this should happen 50% of the time, although the C++ compiler seems to 
	//"go over" if it can when the numbers are specified at compile time, and things
	//like strtod() do the same if integers are specified.
	//Luckily the maximum specifiable DP in UL is 9, so if the discrepancy is in the
	//16th decimal place, say, we can safely assume it's representation granularity.
	//The following seems to work OK.
//	sprintf(buff, "%.27f", data);
	sprintf(buff, "%.27f", _nextafter(data, (data > 0.0) ? DBL_MAX : -DBL_MAX ));

	//Take off trailing digits back to dp
	char* cursor = buff + strlen(buff) - 1;
	char* stop = cursor - 27 + dp;
	while (cursor != stop)
		*cursor-- = 0;

	//And the dot too if that leaves nothing after it
	if (dp == 0)
		*cursor = 0;

	//Returning "-0" or "-0.00" for negatives losing all their significant digits is ugly
	if (buff[0] != '-') 
		return buff;
	
	for (char* c = buff+1; *c != 0; c++)
		if (*c != '0' && *c != '.')
			return buff;

	return buff+1;
}








//***************************************************************************************
//The more refined 15 dsf class
//***************************************************************************************
void RoundedDouble::Check15DSF(const std::string& s, bool assignment)
{
	//In this common case we can save a few instructions
	if (s.length() == 0)
		return;

	//I'm torn between various ways of doing this - this is very neat but slower:
	//	char buff15[128];
	//	sprintf(buff15, "%.15G", result);
	//	char buff16[128];
	//	sprintf(buff15, "%.16G", result);
	//	if (!strcmp(buff15, buff16))
	//		exception because there was something in the 16th sig fig

	//This is quicker, especially for short strings, but might not be absolutely correct
	//Note that by calling this function there is no need to then call Round().

	size_t first_sf = s.find_first_not_of("0.-+ ");
	if (first_sf == std::string::npos)
		//totally zero
		return;

	//With an E-format number the mantissa can have 15 dsf.  Skip back past the
	//exponent when locating the last significant figure.
	size_t last_sf = s.find_last_not_of("0.Ee+- ", s.find_last_of("Ee"));
	if (last_sf == std::string::npos)
		//zero mantissa
		return;

	//How many sig fig?
	int dsf = last_sf - first_sf + 1;

	//With a decimal point in the middle that's one less sf in reality, (we only 
	//care about this if we're right on the borderline)
	if (dsf == 16) {
		size_t dot_pos = s.find('.');
		if (dot_pos > first_sf && dot_pos < last_sf)
			dsf--;
	}

	if (dsf > 15) {
		std::string msg("Invalid numeric literal: '");
		msg.append(s);
		msg.append("' has > 15 significant digits");

		if (assignment) {
			msg.append(" - zero assumed");
			data = 0.0;
		}

		throw Exception(UTIL_DATACONV_DSF, msg);
	}
}

//***************************************************************************************
//Used after addition and subtraction, and one or two other places.  
//***************************************************************************************
void RoundedDouble::Round()
{
	//Common case needs no extra work
	if (data == 0.0)
		return;

	//Not worried about this coming out in E-format as we're going straight back to float
	char buff[32];
	sprintf(buff, "%.15G", data);

	//This can't fail in theory, as it just reverses the above
	data = strtod(buff, NULL);
}









//***************************************************************************************
//V2.26
//***************************************************************************************
#ifdef _BBHOST
void RangeCheckedDouble::Set(const std::string2& s, bool assignment)
{
	try {
		//In this common case we can save a few instructions
		if (s.length() == 0)
			data = 0.0;
		else
			//data = util::String2ToPlainDouble(s); //V3.03
			data = util::String2ToPlainDouble(s, NULL, allowhex);

		if (data == 0.0)
			return;

		if (TooLargeNumber(data) || TooSmallNumber(data)) {
			std::string2 msg("Invalid numeric literal: '");
			msg.append(s);
			if (TooLargeNumber(data)) {
				if (data > 0.0)
					msg.append("' is >= 1E76");
				else
					msg.append("' is <= -1E76");
			}
			else {
				msg.append("' is smaller than 1E-76");
			}

			throw Exception(UTIL_DATACONV_RANGE, msg);
		}
	}
	catch (Exception& e) {
		std::string msg(e.What());

		if (assignment) {
			msg.append(" - zero assumed");
			data = 0.0;
		}

		throw Exception(e.Code(), msg);
	}
}

//***************************************************************************************
std::string2 RangeCheckedDouble::ToString2ForDisplay() const
{
	//Common case
	if (IsZero())
		return std::string2(1, '0');

	//The largest possible number has 75 digits and smallest or (2+74+15) = 91 digits
	char buff[128];

	ToStringBufferForDisplay(buff);
	return buff;
}

//***************************************************************************************
void RoundedDouble::Check15DSF(const std::string2& s, bool assignment)
{
	//In this common case we can save a few instructions
	if (s.length() == 0)
		return;

	//I'm torn between various ways of doing this - this is very neat but slower:
	//	char buff15[128];
	//	sprintf(buff15, "%.15G", result);
	//	char buff16[128];
	//	sprintf(buff15, "%.16G", result);
	//	if (!strcmp(buff15, buff16))
	//		exception because there was something in the 16th sig fig

	//This is quicker, especially for short strings, but might not be absolutely correct
	//Note that by calling this function there is no need to then call Round().

	size_t first_sf = s.find_first_not_of("0.-+ ");
	if (first_sf == std::string::npos)
		//totally zero
		return;

	//With an E-format number the mantissa can have 15 dsf.  Skip back past the
	//exponent when locating the last significant figure.
	size_t last_sf = s.find_last_not_of("0.Ee+- ", s.find_last_of("Ee"));
	if (last_sf == std::string::npos)
		//zero mantissa
		return;

	//How many sig fig?
	int dsf = last_sf - first_sf + 1;

	//With a decimal point in the middle that's one less sf in reality, (we only 
	//care about this if we're right on the borderline)
	if (dsf == 16) {
		size_t dot_pos = s.find('.');
		if (dot_pos > first_sf && dot_pos < last_sf)
			dsf--;
	}

	if (dsf > 15) {
		std::string2 msg("Invalid numeric literal: '");
		msg.append(s);
		msg.append("' has > 15 significant digits");

		if (assignment) {
			msg.append(" - zero assumed");
			data = 0.0;
		}

		throw Exception(UTIL_DATACONV_DSF, msg);
	}
}


#endif //bbhost





} //close namespace
