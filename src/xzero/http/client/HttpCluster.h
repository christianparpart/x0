// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/thread/Future.h>
#include <xzero/http/client/HttpClient.h>
#include <xzero/http/client/HttpClusterMember.h>
#include <xzero/http/client/HttpClusterScheduler.h>
#include <xzero/http/client/HttpHealthMonitor.h>
#include <xzero/http/HttpRequestInfo.h>
#include <xzero/http/HttpResponseInfo.h>
#include <xzero/net/InetAddress.h>
#include <xzero/CompletionHandler.h>
#include <xzero/TokenShaper.h>
#include <xzero/Duration.h>
#include <xzero/Counter.h>
#include <xzero/Uri.h>
#include <utility>
#include <istream>
#include <vector>
#include <list>

namespace xzero {

class InputStream;
class Executor;
class IniFile;
class JsonWriter;

namespace http {

class HttpListener;

namespace client {

class HttpClusterMember;
class HttpClusterScheduler;
class HttpCache;

struct HttpHealthMonitorSettings {
  std::string hostHeader = "healthMonitor";
  std::string requestPath = "/";
  std::string fcgiScriptFilename = "";
  Duration interval = 4_seconds;
  unsigned successThreshold = 3;
  std::vector<HttpStatus> successCodes = {HttpStatus::Ok};
};

struct HttpClusterSettings {
  bool enabled = true;
  bool stickyOfflineMode = false;
  bool allowXSendfile = true;
  bool enqueueOnUnavailable = true;
  size_t queueLimit = 1000;
  Duration queueTimeout = 30_seconds;
  Duration retryAfter = 30_seconds;
  size_t maxRetryCount = 3;
  Duration connectTimeout = 4_seconds;
  Duration readTimeout = 30_seconds;
  Duration writeTimeout = 8_seconds;
  HttpHealthMonitorSettings healthMonitor;
};

class HttpCluster : public HttpClusterMember::EventListener {
 public:
  HttpCluster(const std::string& name,
              const std::string& storagePath,
              Executor* executor);

  HttpCluster(const std::string& name,
              const std::string& storagePath,
              Executor* executor,
              bool enabled,
              bool stickyOfflineMode,
              bool allowXSendfile,
              bool enqueueOnUnavailable,
              size_t queueLimit,
              Duration queueTimeout,
              Duration retryAfter,
              size_t maxRetryCount,
              Duration connectTimeout,
              Duration readTimeout,
              Duration writeTimeout,
              const std::string& healthCheckHostHeader,
              const std::string& healthCheckRequestPath,
              const std::string& healthCheckFcgiScriptFilename,
              Duration healthCheckInterval,
              unsigned healthCheckSuccessThreshold,
              const std::vector<HttpStatus>& healthCheckSuccessCodes);

  ~HttpCluster();

  // {{{ configuration
  const std::string& name() const { return name_; }
  void setName(const std::string& name) { name_ = name; }

  bool isEnabled() const { return enabled_; }
  void setEnabled(bool value) { enabled_ = value; }
  void enable() { enabled_ = true; }
  void disable() { enabled_ = false; }

  bool isMutable() const { return mutable_; }
  void setMutable(bool value) { mutable_ = value; }

  bool stickyOfflineMode() const { return stickyOfflineMode_; }
  void setStickyOfflineMode(bool value) { stickyOfflineMode_ = value; }

  bool allowXSendfile() const { return allowXSendfile_; }
  void setAllowXSendfile(bool value) { allowXSendfile_ = value; }

  bool enqueueOnUnavailable() const { return enqueueOnUnavailable_; }
  void setEnqueueOnUnavailable(bool value) { enqueueOnUnavailable_ = value; }

  size_t queueLimit() const { return queueLimit_; }
  void setQueueLimit(size_t value) { queueLimit_ = value; }

  Duration queueTimeout() const { return queueTimeout_; }
  void setQueueTimeout(Duration value) { queueTimeout_ = value; }

  Duration retryAfter() const { return retryAfter_; }
  void setRetryAfter(Duration value) { retryAfter_ = value; }

  size_t maxRetryCount() const { return maxRetryCount_; }
  void setMaxRetryCount(size_t value) { maxRetryCount_ = value; }

  Duration connectTimeout() const noexcept { return connectTimeout_; }
  void setConnectTimeout(Duration value) { connectTimeout_ = value; }

  Duration readTimeout() const noexcept { return readTimeout_; }
  void setReadTimeout(Duration value) { readTimeout_ = value; }

  Duration writeTimeout() const noexcept { return writeTimeout_; }
  void setWriteTimeout(Duration value) { writeTimeout_ = value; }

  typedef TokenShaper<HttpClusterRequest> RequestShaper;
  typedef RequestShaper::Node Bucket;

  Executor* executor() const noexcept { return executor_; }

  TokenShaperError createBucket(const std::string& name, float rate, float ceil);
  Bucket* findBucket(const std::string& name) const;
  Bucket* rootBucket() const { return shaper()->rootNode(); }
  bool eachBucket(std::function<bool(Bucket*)> body);
  const RequestShaper* shaper() const { return &shaper_; }
  RequestShaper* shaper() { return &shaper_; }

  bool setScheduler(const std::string& scheduler);
  void setScheduler(std::unique_ptr<HttpClusterScheduler> scheduler);
  HttpClusterScheduler* scheduler() const { return scheduler_.get(); }

  /**
   * Adds a new member to the HTTP cluster without any capacity constrain.
   *
   * @param addr     upstream TCP/IP address and port
   */
  void addMember(const InetAddress& addr);

  /**
   * Adds a new member to the HTTP cluster.
   *
   * @param addr     upstream TCP/IP address and port
   * @param capacity number of concurrent requests this member can handle at
   *                 most.
   */
  void addMember(const InetAddress& addr, size_t capacity);

  /**
   * Adds a new member to the HTTP cluster.
   *
   * @param name human readable name for the given member.
   * @param addr upstream TCP/IP address and port
   * @param capacity number of concurrent requests this member can handle at
   *                 most.
   * @param enabled Initial enabled-state.
   * @param terminateProtection
   * @param protocol
   * @param healthCheckInterval
   */
  void addMember(const std::string& name,
                 const InetAddress& addr,
                 size_t capacity,
                 bool enabled,
                 bool terminateProtection,
                 const std::string& protocol,
                 Duration healthCheckInterval);

  HttpClusterMember* findMember(const std::string& name);

  /**
   * Removes member by name.
   *
   * @param name human readable name of the member to be removed.
   */
  void removeMember(const std::string& name);

  // // .... FIXME: proper API namespacing. maybe cluster->healthMonitor()->{testUri, interval, consecutiveSuccessCount, ...}
  const std::string& healthCheckHostHeader() const noexcept { return healthCheckHostHeader_; }
  void setHealthCheckHostHeader(const std::string& value);

  const std::string& healthCheckRequestPath() const noexcept { return healthCheckRequestPath_; }
  void setHealthCheckRequestPath(const std::string& value);

  Duration healthCheckInterval() const noexcept { return healthCheckInterval_; }
  void setHealthCheckInterval(Duration value);

  unsigned healthCheckSuccessThreshold() const noexcept { return healthCheckSuccessThreshold_; }
  void setHealthCheckSuccessThreshold(unsigned value);

  const std::vector<HttpStatus>& healthCheckSuccessCodes() const noexcept { return healthCheckSuccessCodes_; }
  void setHealthCheckSuccessCodes(const std::vector<HttpStatus>& value);
  // }}}

  // {{{ serialization
  /**
   * Retrieves the configuration as a text string.
   */
  std::string configuration() const;

  /**
   * Sets the cluster configuration as defined by given string.
   *
   * @see std::string configuration() const
   */
  void setConfiguration(const std::string& configuration,
                        const std::string& path);

  void saveConfiguration();
  // }}}

  /**
   * Passes given request to a cluster member to be served.
   *
   * Uses the root bucket.
   *
   * @param cr request to schedule
   */
  void schedule(HttpClusterRequest* cr);

  /**
   * Passes given request @p cr to a cluster member to be served,
   * honoring given @p bucket.
   *
   * @param cr request to schedule
   * @param bucket a TokenShaper bucket to allocate this request into.
   */
  void schedule(HttpClusterRequest* cr, Bucket* bucket);

  void serialize(JsonWriter& json) const;

  // HttpClusterMember::EventListener overrides
  void onEnabledChanged(HttpClusterMember* member) override;
  void onCapacityChanged(HttpClusterMember* member, size_t old) override;
  void onHealthChanged(HttpClusterMember* member,
                       HttpHealthMonitor::State old) override;
  void onProcessingSucceed(HttpClusterMember* member) override;
  void onProcessingFailed(HttpClusterRequest* request) override;

 private:
  void reschedule(HttpClusterRequest* cr);
  void serviceUnavailable(HttpClusterRequest* cr, HttpStatus status = HttpStatus::ServiceUnavailable);
  bool verifyTryCount(HttpClusterRequest* cr);
  void enqueue(HttpClusterRequest* cr);
  void dequeueTo(HttpClusterMember* backend);
  HttpClusterRequest* dequeue();
  void onTimeout(HttpClusterRequest* cr);
  void loadBackend(const IniFile& settings, const std::string& key);
  void loadBucket(const IniFile& settings, const std::string& key);

 private:
  // cluster's human readable representative name.
  std::string name_;

  // whether this director actually load balances or raises a 503
  // when being disabled temporarily.
  bool enabled_;

  bool mutable_;

  // whether a backend should be marked disabled if it becomes online again
  bool stickyOfflineMode_;

  // whether or not to evaluate the X-Sendfile response header.
  bool allowXSendfile_;

  // whether to enqueue or to 503 the request when the request could not
  // be delivered (no backend is UP).
  bool enqueueOnUnavailable_;

  // how many requests to queue in total.
  size_t queueLimit_;

  // how long a request may be queued.
  Duration queueTimeout_;

  // time a client should wait before retrying a failed request.
  Duration retryAfter_;

  // number of attempts to pass request to a backend before giving up
  size_t maxRetryCount_;

  // backend connect() timeout
  Duration connectTimeout_;

  // backend response read timeout
  Duration readTimeout_;

  // backend request write timeout
  Duration writeTimeout_;

  // Executor used for request shaping and health checking
  Executor* executor_;

  // path to the local directory this director is serialized from/to.
  std::string storagePath_;

  RequestShaper shaper_;

  // cluster member vector
  std::vector<HttpClusterMember*> members_;

  // health check: test URL
  std::string healthCheckHostHeader_;
  std::string healthCheckRequestPath_;
  std::string healthCheckFcgiScriptFilename_;

  // health-check: test interval
  Duration healthCheckInterval_;

  // health-check: number of consecutive success responsives before setting
  // a backend (back to) online.
  unsigned healthCheckSuccessThreshold_;

  // health-check: list of HTTP status codes to treat as success.
  std::vector<HttpStatus> healthCheckSuccessCodes_;

  // member scheduler
  std::unique_ptr<HttpClusterScheduler> scheduler_;

  // statistical counter for accumulated cluster load (all members)
  Counter load_;

  // statistical counter of how many requests are currently queued.
  Counter queued_;

  // statistical number of how many requests has been dropped so far.
  std::atomic<unsigned long long> dropped_;
};

} // namespace client
} // namespace http
} // namespace xzero
