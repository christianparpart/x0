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
#include <xzero/CompletionHandler.h>
#include <xzero/http/client/HttpClient.h>

namespace xzero {
namespace http {
namespace client {

class HttpUpstream {
public:

private:
  std::list<HttpClient> clients_;
  HttpClient healthMonitor_;
};

class HttpUpstreamScheduler {
};

class HttpCluster {
public:
  HttpCluster();
  ~HttpCluster();

  //! Retrieves the configuration as a text string.
  std::string configuration() const;

  /**
   * Sets the cluster configuration as defined by given string.
   *
   * @see std::string configuration() const
   */
  void setConfiguration(const std::string& configuration);

  void addUpstream(const IPAddress& ipaddr, int port);
  void addUpstream(const std::string& name,
                   const IPAddress& ipaddr,
                   int port,
                   int weight,
                   bool enabled);

  void changeScheduler(UniquePtr<Scheduler> scheduler);

  Future<HttpResponseMessage> send(
      HttpRequestInfo&& requestInfo,
      const std::string& requestBody = "");

  void send(HttpRequestInfo&& requestInfo,
            const std::string& requestBody,
            HttpListener* responseListener);

private:
  std::vector<Client> clients_;
  UniquePtr<Scheduler> scheduler_;
};

} // namespace client
} // namespace http
} // namespace xzero
