// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <string>
#include <list>
#include <memory>

namespace xzero::http {
  class HttpRequest;
  class HttpResponse;
}

namespace xzero::http::cluster {

class Cluster;

class Api {
 public:
  virtual ~Api() {}

  // retrieves all available cluster
  virtual std::list<Cluster*> listCluster() = 0;

  // gets one cluster
  virtual Cluster* findCluster(const std::string& name) = 0;

  // creates a new cluster
  virtual Cluster* createCluster(const std::string& name,
                                 const std::string& path) = 0;

  // destroys a cluster
  virtual void destroyCluster(const std::string& name) = 0;
};

} // namespace xzero::http::cluster
