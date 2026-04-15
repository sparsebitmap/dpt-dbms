
#include "stdafx.h"

#include "dbapi\ctxtspec.h"
#include "ctxtspec.h"

namespace dpt {

APIContextSpecification::APIContextSpecification(const std::string& s)
: target(new ContextSpecification(s))
{
	target->refcount = 1;
}

APIContextSpecification::APIContextSpecification(const char* s)
: target(new ContextSpecification(s))
{
	target->refcount = 1;
}

APIContextSpecification::APIContextSpecification(const APIContextSpecification& from)
{
	//Assignment or or copy constructor
	target = from.target;
	if (target)
		target->refcount++;
}

APIContextSpecification::~APIContextSpecification()
{
	if (target) {
		target->refcount--;
		if (target->refcount == 0)
			delete target;
	}
}

} //close namespace


