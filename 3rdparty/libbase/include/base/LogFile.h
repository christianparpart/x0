// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <base/Buffer.h>
#include <base/io/Sink.h>
#include <base/Api.h>
#include <memory>
#include <atomic>
#include <string>
#include <unistd.h>
#include <pthread.h>
#include <ev++.h>

namespace base {

/**
 * Non-blocking file logging API.
 *
 * An instance of \c LogFile always spawns dedicated writer thread
 * used to write synchronously into the log file.
 *
 */
class BASE_API LogFile : public Sink {
 public:
  explicit LogFile(const std::string& path);
  ~LogFile();

  bool write(std::unique_ptr<Buffer>&& message);

  ssize_t write(const void* buffer, size_t size) override;
  void accept(SinkVisitor& v) override;

  size_t pending() const { return pending_.load(); }
  size_t dropped() const { return dropped_.load(); }
  size_t writeErrors() const { return writeErrors_.load(); }

  void cycle();

 private:
  void init();
  int open();

  static void* start(void* self);
  void main();

  void onStop(ev::async& async, int revents);
  void onCycle(ev::async& async, int revents);
  void onData(ev::io& io, int revents);
  size_t readSome();

 private:
  std::string path_;             ///< path to log file
  int fileFd_;                   ///< fd to log file
  int senderFd_;                 ///< fd to send message buffer to writer thread
  int receiverFd_;               ///< fd to receive messages from senders
  std::atomic<size_t> pending_;  ///< number of pending messages
  std::atomic<size_t> dropped_;  ///< number of dropped messages (due to
                                 ///overload)
  std::atomic<size_t> writeErrors_;  ///< log-file write error counter
  Buffer readBuffer_;
  pthread_t tid_;  ///< writer thread ID

  ev::loop_ref loop_;
  ev::io onData_;
  ev::async onCycle_;
  ev::async onStop_;
};

}  // namespace base
