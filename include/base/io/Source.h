// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#ifndef sw_x0_io_source_hpp
#define sw_x0_io_source_hpp 1

#include <base/Buffer.h>
#include <base/io/Sink.h>
#include <base/Api.h>
#include <memory>

namespace base {

//! \addtogroup io
//@{

/** chunk producer.
 *
 * A source is a chunk producer, e.g. by reading sequentially from a file.
 *
 * \see FileSource, Sink, Filter
 */
class X0_API Source {
 public:
  virtual ~Source() {}

  virtual ssize_t sendto(Sink& output) = 0;
  virtual ssize_t size() const = 0;

  virtual const char* className() const = 0;
};

//@}

}  // namespace base

#endif
