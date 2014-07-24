// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <base/Api.h>
#include <base/io/Sink.h>

namespace base {

class X0_API SyslogSink : public Sink {
 private:
  int level_;

 public:
  explicit SyslogSink(int level);

  virtual void accept(SinkVisitor& v);
  virtual ssize_t write(const void* buffer, size_t size);

  static void open(const char* ident = nullptr, int options = 0,
                   int facility = 0);
  static void close();
};

}  // namespace base
