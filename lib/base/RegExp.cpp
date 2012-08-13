/* <x0/RegExp.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://redmine.trapni.de/projects/x0
 *
 * (c) 2010-2012 Christian Parpart <trapni@gentoo.org>
 */

#include <x0/RegExp.h>
#include <cstring>
#include <pcre.h>

namespace x0 {

RegExp::RegExp() :
	pattern_(),
	re_(NULL)
{
}

RegExp::RegExp(const RegExp& v) :
	pattern_(v.pattern_),
	re_()
{
	// there's no pcre_clone() unfortunately ^^
	const char *errMsg = "";
	int errOfs = 0;

	re_ = pcre_compile(pattern_.c_str(), PCRE_CASELESS | PCRE_EXTENDED, &errMsg, &errOfs, 0);
}

RegExp::RegExp(const std::string& pattern) :
	pattern_(pattern),
	re_()
{
	const char *errMsg = "";
	int errOfs = 0;

	re_ = pcre_compile(pattern_.c_str(), PCRE_CASELESS | PCRE_EXTENDED, &errMsg, &errOfs, 0);
}

RegExp::~RegExp()
{
	pcre_free(re_);
}

bool RegExp::match(const char *cstring) const
{
	if (!re_)
		return false;

	const size_t OV_COUNT = 3 * 36;
	int ov[OV_COUNT];

	int rc = pcre_exec(re_, NULL, cstring, strlen(cstring), 0, 0, ov, OV_COUNT);
	return rc > 0;
}

bool RegExp::match(const char *buffer, size_t size) const
{
	if (!re_)
		return false;

	const size_t OV_COUNT = 3 * 36;
	int ov[OV_COUNT];

	int rc = pcre_exec(re_, NULL, buffer, size, 0, 0, ov, OV_COUNT);
	return rc > 0;
}

const char *RegExp::c_str() const
{
	return pattern_.c_str();
}

} // namespace x0
