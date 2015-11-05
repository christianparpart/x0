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
#include <xzero/http/HttpRequestInfo.h>
#include <xzero/http/HttpResponseInfo.h>
#include <xzero/net/IPAddress.h>
#include <xzero/CompletionHandler.h>
#include <xzero/Duration.h>
#include <xzero/Uri.h>
#include <xzero/stdtypes.h>
#include <utility>
#include <istream>

namespace xzero {
namespace http {
namespace client {

class HttpClusterMember {
public:
  HttpClusterMember(
      const std::string& name,
      const IPAddress& ipaddr,
      int port,
      const std::string& protocol, // http, https, fastcgi, h2, ...
      size_t capacity,
      Duration healthCheckInterval);

private:
  std::string name_;
  IPAddress ipaddress_;
  int port_;
  std::string protocol_;
  size_t capacity_;

  bool enabled_;

  std::list<UniquePtr<HttpClient>> clients_;
};

/*!
 * Reflects the result of a request scheduling attempt.
 */
enum class SchedulerStatus {
  //! Request not scheduled, as all backends are offline and/or disabled.
  Unavailable,

  //! Request scheduled, Backend accepted request.
  Success,

  //!< Request not scheduled, as all backends available but overloaded or offline/disabled.
  Overloaded
};

// {{{ ClientAbortAction
/**
 * Action/behavior how to react on client-side aborts.
 */
enum class ClientAbortAction {
  /**
   * Ignores the client abort.
   * That is, the upstream server will not notice that the client did abort.
   */
  Ignore = 0,

  /**
   * Close both endpoints.
   *
   * That is, closes connection to the upstream server as well as finalizes
   * closing the client connection.
   */
  Close = 1,

  /**
   * Notifies upstream.
   *
   * That is, the upstream server will be gracefully notified.
   * For FastCGI an \c AbortRequest message will be sent to upstream.
   * For HTTP this will cause the connection to the upstream server
   * to be closed (same as \c Close action).
   */
  Notify = 2,
};

// Try<ClientAbortAction> parseClientAbortAction(
//     const BufferRef& value);
// 
// std::string tos(ClientAbortAction value);
// }}}

class HttpClusterNotes {
 public:

 private:
  HttpListener* responseListener_;

  // Number of request schedule attempts.
  size_t tryCount;

  // the bucket (node) this request is to be scheduled via.
  //TokenShaper<RequestNotes>::Node* bucket;
};

class HttpClusterScheduler { // {{{
 public:
  typedef std::vector<std::unique_ptr<HttpClusterMember>> MemberList;

  explicit HttpClusterScheduler(const std::string& name, MemberList* members);
  virtual ~HttpClusterScheduler();

  const std::string& name() const { return name_; }
  MemberList& members() const { return *members_; }

  virtual SchedulerStatus schedule(HttpClusterNotes* cn) = 0;

 protected:
  std::string name_;
  MemberList* members_;
};

class RoundRobin : public HttpClusterScheduler {
 public:
  RoundRobin(MemberList* members) : HttpClusterScheduler("rr", members) {}
  SchedulerStatus schedule(HttpClusterNotes* cn) override;
};

class Chance : public HttpClusterScheduler {
 public:
  Chance(MemberList* members) : HttpClusterScheduler("rr", members) {}
  SchedulerStatus schedule(HttpClusterNotes* cn) override;
};
// }}}

class HttpHealthCheck {
 public:
  HttpHealthCheck(
      const Uri& url,
      Duration interval,
      const std::vector<HttpStatus>& successCodes = {HttpStatus::Ok});

  const Uri& url() const { return url_; }
  Duration interval() const { return interval_; }
  const std::vector<HttpStatus>& successCodes() const { return successCodes_; };

 private:
  Uri url_;
  Duration interval_;
  std::vector<HttpStatus> successCodes_;
};

class HttpCluster {
public:
  HttpCluster();

  HttpCluster(const std::string& name,
              bool enabled,
              bool stickyOfflineMode,
              bool allowXSendfile,
              bool enqueueOnUnavailable,
              size_t queueLimit,
              Duration queueTimeout,
              Duration retryAfter,
              size_t maxRetryCount);

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

  void changeScheduler(UniquePtr<HttpClusterScheduler> scheduler);
  HttpClusterScheduler* clusterScheduler() const { return scheduler_.get(); }

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

  void send(HttpRequestInfo&& requestInfo,
            const std::string& requestBody,
            HttpListener* responseListener);

  Future<std::pair<HttpResponseInfo, std::unique_ptr<std::istream>>> send(
      HttpRequestInfo&& requestInfo,
      const std::string& requestBody);

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

  // path to the local directory this director is serialized from/to.
  std::string storagePath_;

  // cluster member vector
  std::vector<HttpClusterMember> members_;

  // member scheduler
  UniquePtr<HttpClusterScheduler> scheduler_;
};

} // namespace client
} // namespace http
} // namespace xzero
