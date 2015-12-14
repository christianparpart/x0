// This file is part of the "libxzero" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//
// libxzero is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.
#pragma once

#include <string>
#include <list>
#include <memory>

namespace xzero {
namespace http {

class HttpRequest;
class HttpResponse;

namespace client {

class HttpCluster;

class HttpClusterApi {
 public:
  ~HttpClusterApi() {}

  // retrieves all available cluster
  virtual std::list<HttpCluster*> listCluster() = 0;

  // gets one cluster
  virtual HttpCluster* findCluster(const std::string& name) = 0;

  // creates a new cluster
  virtual HttpCluster* createCluster(const std::string& name,
                                     const std::string& path) = 0;

  // destroys a cluster
  virtual void destroyCluster(const std::string& name) = 0;
};

} // namespace client
} // namespace http
} // namespace xzero
