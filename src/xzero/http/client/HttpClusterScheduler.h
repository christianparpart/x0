// This file is part of the "libxzero" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//
// libxzero is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.

#pragma once

#include <xzero/http/client/HttpClusterSchedulerStatus.h>

namespace xzero {
namespace http {
namespace client {

class HttpClusterScheduler {
 public:
  typedef std::vector<std::unique_ptr<HttpClusterMember>> MemberList;

  explicit HttpClusterScheduler(const std::string& name, MemberList* members);
  virtual ~HttpClusterScheduler();

  const std::string& name() const { return name_; }
  MemberList& members() const { return *members_; }

  virtual HttpClusterSchedulerStatus schedule(HttpClusterRequest* cn) = 0;

  class Chance;
  class RoundRobin;

 protected:
  std::string name_;
  MemberList* members_;
};

class HttpClusterScheduler::RoundRobin : public HttpClusterScheduler {
 public:
  RoundRobin(MemberList* members) : HttpClusterScheduler("rr", members) {}
  HttpClusterSchedulerStatus schedule(HttpClusterRequest* cn) override;
};

class HttpClusterScheduler::Chance : public HttpClusterScheduler {
 public:
  Chance(MemberList* members) : HttpClusterScheduler("rr", members) {}
  HttpClusterSchedulerStatus schedule(HttpClusterRequest* cn) override;
};

} // namespace client
} // namespace http
} // namespace xzero
