// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#ifndef x0_httpheader_h
#define x0_httpheader_h (1)

#include <xzero/Api.h>
#include <base/Buffer.h>
#include <string>
#include <cassert>

namespace xzero {

//! \addtogroup http
//@{

/**
 * \brief represents an HTTP header (name/value pair).
 */
template <typename T>
struct XZERO_API HttpHeader {
  T name;   //!< header name field
  T value;  //!< header value field

  HttpHeader() : name(), value() {}
  HttpHeader(const HttpHeader& v) : name(v.name), value(v.value) {}
  HttpHeader(const T& name, const T& value) : name(name), value(value) {}

  HttpHeader(const std::initializer_list<T>& pair) : name(), value() {
    assert(pair.size() == 2 && "Expects a pair of 2 elements.");
    size_t i = 0;
    for (const auto& item : pair) {
      if (!i)
        name = item;
      else
        value = item;
      ++i;
    }
  }
};

typedef HttpHeader<base::BufferRef> HttpRequestHeader;

//@}

}  // namespace xzero

#endif
