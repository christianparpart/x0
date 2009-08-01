/* <x0/header.hpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 *
 * (c) 2009 Chrisitan Parpart <trapni@gentoo.org>
 */

#ifndef x0_header_hpp
#define x0_header_hpp (1)

#include <string>
#include <x0/types.hpp>

namespace x0 {

/**
 * \ingroup core
 * \brief represents an HTTP header (name/value pair).
 */
struct header {
	std::string name;		//!< header name field
	std::string value;		//!< header value field

	header() : name(), value() { }
	header(const header& v) : name(v.name), value(v.value) { }
	header(const std::string& name, const std::string& value) : name(name), value(value) { }
};

} // namespace x0

#endif
