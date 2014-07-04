// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#ifndef sw_x0_io_NullFilter_hpp
#define sw_x0_io_NullFilter_hpp 1

#include <x0/io/Filter.h>
#include <x0/Buffer.h>

namespace x0 {

//! \addtogroup io
//@{

/** simply passes incoming buffers through.
 */
class X0_API NullFilter : public Filter {
 public:
  NullFilter() {}

  virtual Buffer process(const BufferRef& data) { return Buffer(data); }
};

//@}

}  // namespace x0

#endif
