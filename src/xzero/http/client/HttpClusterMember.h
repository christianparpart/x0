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
#include <xzero/net/IPAddress.h>
#include <xzero/CompletionHandler.h>
#include <xzero/Duration.h>
#include <xzero/Uri.h>
#include <xzero/stdtypes.h>
#include <utility>
#include <istream>

namespace xzero {

class InputStream;

namespace http {
namespace client {

class HttpHealthCheck;
class HttpClusterRequest;

class HttpClusterMember {
public:
  HttpClusterMember(
      Scheduler* scheduler,
      const std::string& name,
      const IPAddress& ipaddr,
      int port,
      const std::string& protocol, // http, https, fastcgi, h2, ...
      size_t capacity,
      std::unique_ptr<HttpHealthCheck> healthCheck);

  Scheduler* scheduler() const { return scheduler_; }

  const std::string& name() const { return name_; }
  void setName(const std::string& name);

  const IPAddress& ipaddress() const { return ipaddress_; }
  void setIPAddress(const IPAddress& ipaddr);

  int port() const { return port_; }
  void setPort(int value);

  size_t capacity() const { return capacity_; }
  void setCapacity(size_t value);

  HttpHealthCheck* healthCheck() const { return healthCheck_.get(); }

  HttpClusterSchedulerStatus tryProcess(HttpClusterRequest* cr);

private:
  std::string name_;
  IPAddress ipaddress_;
  int port_;
  std::string protocol_;
  size_t capacity_;
  std::unique_ptr<HttpHealthCheck> healthCheck_;

  bool enabled_;

  Scheduler* scheduler_;
  std::list<UniquePtr<HttpClient>> clients_;
};

} // namespace client
} // namespace http
} // namespace xzero