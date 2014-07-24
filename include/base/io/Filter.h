// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#ifndef sw_x0_io_filter_hpp
#define sw_x0_io_filter_hpp 1

#include <base/Buffer.h>
#include <base/io/Source.h>
#include <base/io/Sink.h>
#include <base/Api.h>
#include <memory>

namespace base {

//! \addtogroup io
//@{

/** unidirectional data processor.
 *
 * a filter is a processor, that reads from a source, and passes
 * the received data to the sink. this data may or may not be
 * transformed befor passing to the sink.
 *
 * \see source, sink
 */
class X0_API Filter {
 public:
  /** processes given input data through this Filter. */
  virtual Buffer process(const BufferRef& input) = 0;

 public:
  Buffer operator()(const BufferRef& input) { return process(input); }
};

typedef std::shared_ptr<Filter> FilterPtr;

//@}

}  // namespace base

#endif
