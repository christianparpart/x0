// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/http/cluster/SchedulerStatus.h>
#include <xzero/http/cluster/HealthMonitor.h>
#include <xzero/http/client/HttpClient.h>
#include <xzero/thread/Future.h>
#include <xzero/net/InetAddress.h>
#include <xzero/CompletionHandler.h>
#include <xzero/Duration.h>
#include <xzero/Counter.h>
#include <xzero/Uri.h>
#include <vector>
#include <list>
#include <mutex>
#include <utility>

namespace xzero {
  class Executor;
  class JsonWriter;
}

namespace xzero::http::cluster {

class Context;

/**
 * Represents a cluster member within a cluster.
 *
 * @see Cluster
 */
class Backend {
 public:
  class EventListener;

  using HttpClient = xzero::http::client::HttpClient;
  using StateChangeNotify = std::function<void(Backend*,
                                               HealthMonitor::State)>;

  using Protocol = std::string;

  /**
   * Constructs a backend.
   *
   * @p eventListener interface with a set of callbacks being invoked whenever
   *                  a state change is happening to this cluster member
   * @p executor      Executor to be used for health checks
   * @p name          unique backend name within the cluster
   * @p inet          Internet address (IP:port) the backend is listening on
   * @p capacity      number of requests the backend can handle concurrently
   * @p enabled       flag whether or not this backend is initially enabled
   * @p terminateProtection flag indicating whether this backend is protected
   *                        against accidental termination
   * @p protocol      Backend protocol (such as: "http", "https", "fastcgi", "h2")
   * @p connectTimeout duration until connect attempts are timing out
   * @p readTimeout   duration until read attempts are timing out
   * @p writeTimeout  duration until write attempts are timing out
   * @p healthCheckHostHeader health check' Host request header
   * @p healthCheckRequestPath health check's request path
   * @p healthCheckFcgiScriptFilename health check's fastcgi script filename
   *                                  (only necessary when using protocol "fastcgi")
   * @p healthCheckInterval wait time between health checks
   * @p healthCheckSuccessThreshold threshold of successfully health checks
   *                                until an offline node is marked online again
   * @p healthCheckSuccessThreshold list of HTTP status codes treated as healthy
   *                                responses
   */
  Backend(EventListener* eventListener,
          Executor* executor,
          const std::string& name,
          const InetAddress& inet,
          size_t capacity,
          bool enabled,
          bool terminateProtection,
          const Protocol& protocol, // http, https, fastcgi, h2, ...
          Duration connectTimeout,
          Duration readTimeout,
          Duration writeTimeout,
          const std::string& healthCheckHostHeader,
          const std::string& healthCheckRequestPath,
          const std::string& healthCheckFcgiScriptFilename,
          Duration healthCheckInterval,
          unsigned healthCheckSuccessThreshold,
          const std::vector<HttpStatus>& healthCheckSuccessCodes);

  ~Backend();

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

  const Protocol& protocol() const noexcept { return protocol_; }

  HealthMonitor* healthMonitor() const { return healthMonitor_.get(); }

  [[nodiscard]] SchedulerStatus tryProcess(Context* cr);
  void release();

  void serialize(JsonWriter& json) const;

 private:
  bool process(Context* cr);
  void onResponseReceived(Context* cr, const HttpClient::Response& r);
  void onFailure(Context* cr, const std::error_code& ec);

 private:
  EventListener* eventListener_;
  Executor* executor_;
  std::string name_;
  InetAddress inetAddress_;
  size_t capacity_;
  bool enabled_;
  bool terminateProtection_;
  Counter load_;
  Protocol protocol_;
  Duration connectTimeout_;
  Duration readTimeout_;
  Duration writeTimeout_;
  std::unique_ptr<HealthMonitor> healthMonitor_;
  std::mutex lock_;
};

class Backend::EventListener {
 public:
  virtual ~EventListener() {}

  virtual void onEnabledChanged(Backend* member) = 0;
  virtual void onCapacityChanged(Backend* member, size_t old) = 0;
  virtual void onHealthChanged(Backend* member,
                               HealthMonitor::State old) = 0;

  /**
   * Invoked when backend is done processing with one request.
   */
  virtual void onProcessingSucceed(Backend* member) = 0;

  /**
   * Invoked when given @p request has failed processing.
   */
  virtual void onProcessingFailed(Context* request) = 0;
};

} // namespace xzero::http::cluster
