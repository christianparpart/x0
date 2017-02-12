// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include "HealthMonitor.h"
#include "FastCgiProtocol.h"
#include <base/Buffer.h>
#include <base/Socket.h>
#include <xzero/HttpStatus.h>
#include <ev++.h>

/**
 * HTTP Health Monitor.
 */
class FastCgiHealthMonitor : public HealthMonitor {
 public:
  explicit FastCgiHealthMonitor(xzero::HttpWorker& worker);
  ~FastCgiHealthMonitor();

  virtual void setRequest(const char* fmt, ...);
  virtual void reset();
  virtual void onCheckStart();

 private:
  template <typename T, typename... Args>
  void write(Args&&... args);
  void write(FastCgi::Type type, const base::Buffer& buffer);

  void onConnectDone(base::Socket*, int revents);
  void io(base::Socket*, int revents);
  bool writeSome();
  bool readSome();
  void onTimeout(base::Socket* s);

  bool processRecord(const FastCgi::Record* record);
  void onStdOut(const base::BufferRef& chunk);
  void onStdErr(const base::BufferRef& chunk);
  void onEndRequest(int appStatus, FastCgi::ProtocolStatus protocolStatus);

 private:
  std::unordered_map<std::string, std::string> fcgiParams_;

  base::Socket socket_;

  base::Buffer writeBuffer_;
  size_t writeOffset_;

  base::Buffer readBuffer_;
  size_t readOffset_;
};
