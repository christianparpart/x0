// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#ifndef sw_x0_io_NullSource_hpp
#define sw_x0_io_NullSource_hpp 1

#include <x0/io/Source.h>
#include <memory>

namespace x0 {

//! \addtogroup io
//@{

/** an empty source, sending nothing.
 *
 * \see Buffer, Source, Sink
 */
class X0_API NullSource : public Source {
 public:
  ssize_t sendto(Sink& sink);
  ssize_t size() const override;

  virtual const char* className() const;
};

//@}

}  // namespace x0

#endif
