/* <x0/HttpHeader.h>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.ws/
 *
 * (c) 2009-2010 Christian Parpart <trapni@gentoo.org>
 */

#ifndef x0_httpheader_h
#define x0_httpheader_h (1)

#include <x0/Buffer.h>
#include <string>

#include <x0/Types.h>
#include <x0/Api.h>
#include <string>

namespace x0 {

//! \addtogroup core
//@{

/**
 * \brief represents an HTTP header (name/value pair).
 */
template<typename T>
struct HttpHeader {
	T name;			//!< header name field
	T value;		//!< header value field

	HttpHeader() : name(), value() { }
	HttpHeader(const HttpHeader& v) : name(v.name), value(v.value) { }
	HttpHeader(const T& name, const T& value) : name(name), value(value) { }
};

typedef HttpHeader<BufferRef> HttpRequestHeader;
typedef HttpHeader<std::string> HttpResponseHeader;

//@}

} // namespace x0

#endif
