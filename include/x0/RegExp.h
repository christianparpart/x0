/* <flow/RegExp.h>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://redmine.xzero.io/projects/flow
 *
 * (c) 2009-2012 Christian Parpart <trapni@gentoo.org>
 */

#ifndef sw_flow_RegExp_h
#define sw_flow_RegExp_h

#include <x0/Api.h>
#include <pcre.h>
#include <string>
#include <vector>

namespace x0 {

class BufferRef;

class X0_API RegExp
{
private:
	std::string pattern_;
	pcre *re_;

public:
	typedef std::vector<std::pair<const char*, size_t>> Result;

public:
	explicit RegExp(const std::string& pattern);
	RegExp();
	RegExp(const RegExp& v);
	~RegExp();

	bool match(const char *buffer, size_t size, Result* result = nullptr) const;
	bool match(const BufferRef& buffer) const;
	bool match(const char *cstring) const;

	const char *c_str() const;
};

} // namespace x0

#endif
