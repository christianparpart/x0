// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <base/Buffer.h>
#include <base/Duration.h>
#include <base/Logging.h>
#include <base/Socket.h>
#include <base/SocketSpec.h>
#include <base/JsonWriter.h>
#include <xzero/HttpStatus.h>
#include <xzero/HttpWorker.h>
#include <xzero/HttpMessageParser.h>
#include <ev++.h>

class Backend;

enum class HealthState { Undefined, Offline, Online };

/**
 * Implements HTTP server health monitoring.
 *
 * \note not thread-safe.
 */
class HealthMonitor : protected base::Logging,
                      protected xzero::HttpMessageParser {
 public:
  enum class Mode { Paranoid, Opportunistic, Lazy };

 protected:
  Mode mode_;
  Backend* backend_;
  xzero::HttpWorker& worker_;
  base::Duration interval_;
  HealthState state_;

  std::function<void(HealthMonitor*, HealthState)> onStateChange_;

  xzero::HttpStatus expectCode_;

  ev::timer timer_;

  size_t successThreshold;  //!< number of consecutive succeeding responses
  // before marking changing state to *online*.

  size_t failCount_;     //!< total fail count
  size_t successCount_;  //!< consecutive success count
  time_t offlineTime_;   //!< total time this node has been offline

  xzero::HttpStatus responseCode_;
  bool processingDone_;

 public:
  explicit HealthMonitor(
      xzero::HttpWorker& worker,
      HttpMessageParser::ParseMode parseMode = HttpMessageParser::RESPONSE);
  virtual ~HealthMonitor();

  Mode mode() const { return mode_; }
  const std::string& mode_str() const;
  void setMode(Mode value);

  HealthState state() const { return state_; }
  void setState(HealthState value);
  const std::string& state_str() const;
  bool isOnline() const { return state_ == HealthState::Online; }

  Backend* backend() const { return backend_; }
  void setBackend(Backend* backend);

  void update();

  const base::Duration& interval() const { return interval_; }
  void setInterval(const base::Duration& value);

  void setExpectCode(xzero::HttpStatus value) { expectCode_ = value; }
  xzero::HttpStatus expectCode() const { return expectCode_; }

  void setStateChangeCallback(
      const std::function<void(HealthMonitor*, HealthState)>& callback);

  virtual void setRequest(const char* fmt, ...) = 0;
  void reset() override;

  void start();
  void stop();

  template <typename T>
  inline void post(T function) {
    worker_.post(function);
  }

 protected:
  virtual void onCheckStart();

  void logSuccess();
  void logFailure();

  void recheck();

  // response (HttpMessageParser)
  bool onMessageBegin(int versionMajor, int versionMinor, int code,
                      const base::BufferRef& text) override;
  bool onMessageHeader(const base::BufferRef& name,
                       const base::BufferRef& value) override;
  bool onMessageContent(const base::BufferRef& chunk) override;
  bool onMessageEnd() override;
};

base::JsonWriter& operator<<(base::JsonWriter& json,
                             const HealthMonitor& monitor);
