/* <flow/RegExp.h>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://redmine.xzero.ws/projects/flow
 *
 * (c) 2010 Christian Parpart <trapni@gentoo.org>
 */

#ifndef sw_flow_RegExp_h
#define sw_flow_RegExp_h

#include <pcre.h>
#include <string>

namespace x0 {

class RegExp
{
private:
	std::string pattern_;
	pcre *re_;

public:
	explicit RegExp(const std::string& pattern);
	RegExp();
	RegExp(const RegExp& v);
	~RegExp();

	bool match(const char *cstring) const;
	bool match(const char *buffer, size_t size) const;

	const char *c_str() const;
};

} // namespace x0

#endif
