// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <base/Buffer.h>
#include <base/io/Sink.h>
#include <base/io/Source.h>

namespace base {

//! \addtogroup io
//@{

/** sink, storing incoming data into a buffer.
 *
 * \see sink, source
 */
class X0_API FixedBufferSink : public Sink {
 public:
  explicit FixedBufferSink(FixedBuffer& ref) : buffer_(ref) {}

  virtual void accept(SinkVisitor& v);
  virtual ssize_t write(const void* buffer, size_t size);

 public:
  void clear() { buffer_.clear(); }

  FixedBuffer& buffer() { return buffer_; }

  const FixedBuffer& buffer() const { return buffer_; }

  std::size_t size() const { return buffer_.size(); }

  bool empty() const { return buffer_.empty(); }

 private:
  FixedBuffer buffer_;
};

//@}

}  // namespace base
