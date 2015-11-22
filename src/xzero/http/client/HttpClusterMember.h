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
#include <xzero/http/client/HttpClusterSchedulerStatus.h>
#include <xzero/http/client/HttpHealthMonitor.h>
#include <xzero/net/IPAddress.h>
#include <xzero/CompletionHandler.h>
#include <xzero/Duration.h>
#include <xzero/Uri.h>
#include <xzero/stdtypes.h>
#include <utility>
#include <istream>

namespace xzero {

class Executor;
class InputStream;

namespace http {
namespace client {

class HttpClusterRequest;

class HttpClusterMember {
public:
  typedef std::function<void(HttpClusterMember*, HttpHealthMonitor::State)>
      StateChangeNotify;

  HttpClusterMember(
      Executor* executor,
      const std::string& name,
      const IPAddress& ipaddr,
      int port,
      size_t capacity,
      bool enabled,
      const std::string& protocol, // http, https, fastcgi, h2, ...
      Duration connectTimeout,
      Duration readTimeout,
      Duration writeTimeout,
      const Uri& healthCheckUri,
      Duration healthCheckInterval,
      unsigned healthCheckSuccessThreshold,
      const std::vector<HttpStatus>& healthCheckSuccessCodes,
      StateChangeNotify onHealthStateChange);

  ~HttpClusterMember();

  Executor* executor() const { return executor_; }

  const std::string& name() const { return name_; }
  void setName(const std::string& name);

  const IPAddress& ipaddress() const { return ipaddress_; }
  void setIPAddress(const IPAddress& ipaddr);

  int port() const { return port_; }
  void setPort(int value);

  size_t capacity() const { return capacity_; }
  void setCapacity(size_t value);

  bool isEnabled() const noexcept { return enabled_; }
  void setEnabled(bool value) { enabled_ = value; }

  HttpHealthMonitor* healthMonitor() const { return healthMonitor_.get(); }

  HttpClusterSchedulerStatus tryProcess(HttpClusterRequest* cr);

private:
  Executor* executor_;
  std::string name_;
  IPAddress ipaddress_;
  int port_;
  size_t capacity_;
  bool enabled_;
  std::string protocol_; // "http" | "fastcgi"
  Duration connectTimeout_;
  Duration readTimeout_;
  Duration writeTimeout_;
  std::unique_ptr<HttpHealthMonitor> healthMonitor_;

  std::list<UniquePtr<HttpClient>> clients_;
};

} // namespace client
} // namespace http
} // namespace xzero
