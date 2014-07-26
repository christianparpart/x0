// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#ifndef sw_x0_io_SinkVisitor_h
#define sw_x0_io_SinkVisitor_h 1

#include <base/Api.h>

namespace base {

class BufferSink;
class FileSink;
class FixedBufferSink;
class SocketSink;
class PipeSink;
class SyslogSink;
class LogFile;

//! \addtogroup io
//@{

/** source visitor.
 *
 * \see source
 */
class BASE_API SinkVisitor {
 public:
  virtual ~SinkVisitor() {}

  virtual void visit(BufferSink&) = 0;
  virtual void visit(FileSink&) = 0;
  virtual void visit(FixedBufferSink&) = 0;
  virtual void visit(SocketSink&) = 0;
  virtual void visit(PipeSink&) = 0;
  virtual void visit(SyslogSink&) = 0;
  virtual void visit(LogFile&) = 0;
};

//@}

}  // namespace base

#endif
