// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <list>
#include <vector>
#include <xzero/thread/Future.h>
#include <xzero/http/client/HttpClient.h>
#include <xzero/http/client/HttpClusterSchedulerStatus.h>
#include <xzero/http/client/HttpHealthMonitor.h>
#include <xzero/net/InetAddress.h>
#include <xzero/CompletionHandler.h>
#include <xzero/Duration.h>
#include <xzero/Counter.h>
#include <xzero/Uri.h>
#include <mutex>
#include <utility>

namespace xzero {

class Executor;
class JsonWriter;

namespace http {
namespace client {

class HttpClusterRequest;

class HttpClusterMember {
public:
  class EventListener;
  typedef std::function<void(HttpClusterMember*, HttpHealthMonitor::State)>
      StateChangeNotify;

  HttpClusterMember(
      EventListener* eventListener,
      Executor* executor,
      const std::string& name,
      const InetAddress& inet,
      size_t capacity,
      bool enabled,
      bool terminateProtection,
      const std::string& protocol, // http, https, fastcgi, h2, ...
      Duration connectTimeout,
      Duration readTimeout,
      Duration writeTimeout,
      const std::string& healthCheckHostHeader,
      const std::string& healthCheckRequestPath,
      const std::string& healthCheckFcgiScriptFilename,
      Duration healthCheckInterval,
      unsigned healthCheckSuccessThreshold,
      const std::vector<HttpStatus>& healthCheckSuccessCodes);

  ~HttpClusterMember();

  Executor* executor() const { return executor_; }

  const std::string& name() const { return name_; }
  void setName(const std::string& name);

  const InetAddress& inetAddress() const { return inetAddress_; }
  void setInetAddress(const InetAddress& value);

  size_t capacity() const { return capacity_; }
  void setCapacity(size_t value);

  bool isEnabled() const noexcept { return enabled_; }
  void setEnabled(bool value) { enabled_ = value; }

  bool terminateProtection() const { return terminateProtection_; }
  void setTerminateProtection(bool value) { terminateProtection_ = value; }

  const std::string& protocol() const noexcept { return protocol_; }

  HttpHealthMonitor* healthMonitor() const { return healthMonitor_.get(); }

  HttpClusterSchedulerStatus tryProcess(HttpClusterRequest* cr);
  void release();

  void serialize(JsonWriter& json) const;

private:
  bool process(HttpClusterRequest* cr);
  void onResponseReceived(HttpClusterRequest* cr);
  void onFailure(HttpClusterRequest* cr, const std::error_code& ec);

private:
  EventListener* eventListener_;
  Executor* executor_;
  std::string name_;
  InetAddress inetAddress_;
  size_t capacity_;
  bool enabled_;
  bool terminateProtection_;
  Counter load_;
  std::string protocol_; // "http" | "fastcgi"
  Duration connectTimeout_;
  Duration readTimeout_;
  Duration writeTimeout_;
  std::unique_ptr<HttpHealthMonitor> healthMonitor_;
  std::mutex lock_;

  std::list<std::unique_ptr<HttpClient>> clients_;
};

class HttpClusterMember::EventListener {
 public:
  virtual ~EventListener() {}

  virtual void onEnabledChanged(HttpClusterMember* member) = 0;
  virtual void onCapacityChanged(HttpClusterMember* member, size_t old) = 0;
  virtual void onHealthChanged(HttpClusterMember* member,
                               HttpHealthMonitor::State old) = 0;

  /**
   * Invoked when backend is done processing with one request.
   */
  virtual void onProcessingSucceed(HttpClusterMember* member) = 0;
  virtual void onProcessingFailed(HttpClusterRequest* request) = 0;
};

} // namespace client
} // namespace http
} // namespace xzero
