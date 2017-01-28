// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

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
