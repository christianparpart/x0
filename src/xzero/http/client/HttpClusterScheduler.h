// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/http/client/HttpClusterSchedulerStatus.h>
#include <vector>
#include <string>
#include <memory>

namespace xzero {
namespace http {
namespace client {

class HttpClusterMember;
class HttpClusterRequest;

class HttpClusterScheduler {
 public:
  typedef std::vector<HttpClusterMember*> MemberList;

  explicit HttpClusterScheduler(const std::string& name, MemberList* members);

  virtual ~HttpClusterScheduler();

  const std::string& name() const { return name_; }
  MemberList& members() const { return *members_; }

  virtual HttpClusterSchedulerStatus schedule(HttpClusterRequest* cn) = 0;

  class Chance;
  class RoundRobin;
  class LeastLoad; // TODO

 protected:
  std::string name_;
  MemberList* members_;
};

class HttpClusterScheduler::RoundRobin : public HttpClusterScheduler {
 public:
  RoundRobin(MemberList* members)
      : HttpClusterScheduler("rr", members),
        next_(0) {}

  HttpClusterSchedulerStatus schedule(HttpClusterRequest* cn) override;

 private:
  size_t next_;
};

class HttpClusterScheduler::Chance : public HttpClusterScheduler {
 public:
  Chance(MemberList* members)
      : HttpClusterScheduler("rr", members) {}

  HttpClusterSchedulerStatus schedule(HttpClusterRequest* cn) override;
};

} // namespace client
} // namespace http
} // namespace xzero
