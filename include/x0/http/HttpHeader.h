/* <x0/HttpHeader.h>
 *
 * This file is part of the x0 web server project and is released under AGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2014 Christian Parpart <trapni@gmail.com>
 */

#ifndef x0_httpheader_h
#define x0_httpheader_h (1)

#include <x0/Buffer.h>
#include <x0/Api.h>
#include <string>

#include <x0/Types.h>
#include <x0/Api.h>
#include <string>
#include <cassert>

namespace x0 {

//! \addtogroup http
//@{

/**
 * \brief represents an HTTP header (name/value pair).
 */
template<typename T>
struct X0_API HttpHeader {
	T name;			//!< header name field
	T value;		//!< header value field

	HttpHeader() : name(), value() { }
	HttpHeader(const HttpHeader& v) : name(v.name), value(v.value) { }
	HttpHeader(const T& name, const T& value) : name(name), value(value) { }

	HttpHeader(const std::initializer_list<T>& pair) :
		name(), value()
	{
		assert(pair.size() == 2 && "Expects a pair of 2 elements.");
		size_t i = 0;
		for (const auto& item: pair) {
			if (!i)
				name = item;
			else
				value = item;
			++i;
		}
	}
};

typedef HttpHeader<BufferRef> HttpRequestHeader;

//@}

} // namespace x0

#endif
