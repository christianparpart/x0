// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#ifndef sw_x0_io_PipeSink_hpp
#define sw_x0_io_PipeSink_hpp 1

#include <base/io/Sink.h>
#include <base/io/Pipe.h>
#include <string>

namespace base {

//! \addtogroup io
//@{

/** pipe sink.
 */
class BASE_API PipeSink : public Sink {
 private:
  Pipe* pipe_;

 public:
  explicit PipeSink(Pipe* pipe);
  ~PipeSink();

  Pipe* pipe() const;

  virtual void accept(SinkVisitor& v);
  virtual ssize_t write(const void* buffer, size_t size);
};

//@}

// {{{ inlines
inline Pipe* PipeSink::pipe() const { return pipe_; }
// }}}

}  // namespace base

#endif
