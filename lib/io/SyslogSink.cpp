// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <x0/io/SyslogSink.h>
#include <x0/io/SinkVisitor.h>
#include <x0/Buffer.h>
#include <x0/sysconfig.h>

#if defined(HAVE_SYSLOG_H)
#include <syslog.h>
#endif

namespace x0 {

SyslogSink::SyslogSink(int level) : level_(level) {}

void SyslogSink::accept(SinkVisitor& v) { v.visit(*this); }

ssize_t SyslogSink::write(const void* buffer, size_t size) {
  const char* msg = static_cast<const char*>(buffer);

  if (msg[size - 1] == '\0') {
    syslog(level_, "%s", msg);
  } else {
    Buffer scratchBuffer;
    scratchBuffer.push_back(msg, size);
    syslog(level_, "%s", scratchBuffer.c_str());
  }

  return size;
}

void SyslogSink::open(const char* ident, int options, int facility) {
  openlog(ident, options, facility);
}

void SyslogSink::close() { closelog(); }

}  // namespace x0
