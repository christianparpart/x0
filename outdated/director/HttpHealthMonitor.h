// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include "HealthMonitor.h"
#include <base/Buffer.h>
#include <base/Socket.h>
#include <ev++.h>

/**
 * HTTP Health Monitor.
 */
class HttpHealthMonitor : public HealthMonitor {
 public:
  explicit HttpHealthMonitor(xzero::HttpWorker& worker);
  ~HttpHealthMonitor();

  virtual void setRequest(const char* fmt, ...);
  virtual void reset();
  virtual void onCheckStart();

 private:
  void onConnectDone(base::Socket*, int revents);
  void io(base::Socket*, int revents);
  void writeSome();
  void readSome();
  void onTimeout(base::Socket* s);

 private:
  base::Socket socket_;
  base::Buffer request_;
  size_t writeOffset_;
  base::Buffer response_;
};
