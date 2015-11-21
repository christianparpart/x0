// This file is part of the "libxzero" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//
// libxzero is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.

#pragma once

#include <list>
#include <vector>
#include <xzero/thread/Future.h>
#include <xzero/http/client/HttpClient.h>
#include <xzero/http/client/HttpClusterMember.h>
#include <xzero/http/client/HttpClusterScheduler.h>
#include <xzero/http/client/HttpHealthMonitor.h>
#include <xzero/http/HttpRequestInfo.h>
#include <xzero/http/HttpResponseInfo.h>
#include <xzero/net/IPAddress.h>
#include <xzero/CompletionHandler.h>
#include <xzero/TokenShaper.h>
#include <xzero/Duration.h>
#include <xzero/Counter.h>
#include <xzero/Uri.h>
#include <xzero/stdtypes.h>
#include <utility>
#include <istream>

namespace xzero {

class InputStream;
class Executor;

namespace http {

class HttpListener;

namespace client {

class HttpClusterMember;
class HttpClusterScheduler;
class HttpCache;

class HttpCluster {
 public:
  HttpCluster(const std::string& name, Executor* executor);

  HttpCluster(const std::string& name,
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
              Duration writeTimeout);

  ~HttpCluster();

  // {{{ configuration
  const std::string& name() const { return name_; }
  void setName(const std::string& name) { name_ = name; }

  bool isEnabled() const { return enabled_; }
  void setEnabled(bool value) { enabled_ = value; }
  void enable() { enabled_ = true; }
  void disable() { enabled_ = false; }

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
  void setExecutor(Executor* executor);

  TokenShaperError createBucket(const std::string& name, float rate, float ceil);
  Bucket* findBucket(const std::string& name) const;
  Bucket* rootBucket() const { return shaper()->rootNode(); }
  bool eachBucket(std::function<bool(Bucket*)> body);
  const RequestShaper* shaper() const { return &shaper_; }
  RequestShaper* shaper() { return &shaper_; }

  void changeScheduler(UniquePtr<HttpClusterScheduler> scheduler);
  HttpClusterScheduler* clusterScheduler() const { return scheduler_.get(); }

  /**
   * Adds a new member to the HTTP cluster.
   *
   * @param ipaddr upstream IP address
   * @param port TCP port number
   * @param capacity number of concurrent requests this member can handle at
   *                 most.
   */
  void addMember(const IPAddress& ipaddr, int port, size_t capacity);

  /**
   * Adds a new member to the HTTP cluster.
   *
   * @param name human readable name for the given member.
   * @param ipaddr upstream IP address
   * @param port TCP port number
   * @param capacity number of concurrent requests this member can handle at
   *                 most.
   * @param enabled Initial enabled-state.
   */
  void addMember(const std::string& name,
                 const IPAddress& ipaddr,
                 int port,
                 size_t capacity,
                 bool enabled);

  HttpClusterMember* findMember(const std::string& name);

  /**
   * Removes member by name.
   *
   * @param name human readable name of the member to be removed.
   */
  void removeMember(const std::string& name);
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
  void setConfiguration(const std::string& configuration);
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

 private:
  void reschedule(HttpClusterRequest* cr);
  void serviceUnavailable(HttpClusterRequest* cr, HttpStatus status = HttpStatus::ServiceUnavailable);
  bool verifyTryCount(HttpClusterRequest* cr);
  bool tryEnqueue(HttpClusterRequest* rn);
  void dequeueTo(HttpClusterMember* backend);
  HttpClusterRequest* dequeue();
  void onTimeout(HttpClusterRequest* cr);
  void onBackendStateChanged(HttpClusterMember* backend,
                             HttpHealthMonitor* healthMonitor,
                             HttpHealthMonitor::State oldState);

 private:
  // cluster's human readable representative name.
  std::string name_;

  // whether this director actually load balances or raises a 503
  // when being disabled temporarily.
  bool enabled_;

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
  std::list<HttpClusterMember*> members_;

  // member scheduler
  UniquePtr<HttpClusterScheduler> scheduler_;

  // statistical counter of how many requests are currently queued.
  Counter queued_;

  // statistical number of how many requests has been dropped so far.
  std::atomic<unsigned long long> dropped_;
};

} // namespace client
} // namespace http
} // namespace xzero
