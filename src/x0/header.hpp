/* <x0/header.hpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 *
 * (c) 2009 Chrisitan Parpart <trapni@gentoo.org>
 */

#ifndef x0_header_hpp
#define x0_header_hpp (1)

#include <x0/io/buffer.hpp>
#include <string>

#include <x0/types.hpp>
#include <x0/api.hpp>
#include <string>

namespace x0 {

//! \addtogroup core
//@{

/**
 * \brief represents an HTTP header (name/value pair).
 */
template<typename T>
struct header {
	T name;			//!< header name field
	T value;		//!< header value field

	header() : name(), value() { }
	header(const header& v) : name(v.name), value(v.value) { }
	header(const T& name, const T& value) : name(name), value(value) { }
};

typedef header<buffer_ref> request_header;
typedef header<std::string> response_header;

//@}

} // namespace x0

#endif
