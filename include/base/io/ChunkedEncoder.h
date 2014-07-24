// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#ifndef sw_x0_io_ChunkedEncoder_hpp
#define sw_x0_io_ChunkedEncoder_hpp 1

#include <base/io/Filter.h>

namespace base {

//! \addtogroup io
//@{

/**
 */
class X0_API ChunkedEncoder : public Filter {
 public:
  ChunkedEncoder();

  virtual Buffer process(const BufferRef& data);

 private:
  bool finished_;
};

//@}

}  // namespace base

#endif
