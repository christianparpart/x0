// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/http/client/HttpClusterScheduler.h>
#include <xzero/http/client/HttpClusterMember.h>
#include <xzero/http/client/HttpClusterRequest.h>

namespace xzero {
namespace http {
namespace client {

HttpClusterScheduler::HttpClusterScheduler(const std::string& name,
                                           MemberList* members)
    : name_(name),
      members_(members) {
}

HttpClusterScheduler::~HttpClusterScheduler() {
}

HttpClusterSchedulerStatus
    HttpClusterScheduler::RoundRobin::schedule(HttpClusterRequest* cr) {
  unsigned unavailable = 0;
  size_t limit = members().size();

  for (size_t count = 0; count < limit; ++count, ++next_) {
    if (next_ >= limit) {
      next_ = 0;
    }

    switch (members()[next_]->tryProcess(cr)) {
      case HttpClusterSchedulerStatus::Success:
        return HttpClusterSchedulerStatus::Success;
      case HttpClusterSchedulerStatus::Unavailable:
        ++unavailable;
        break;
      case HttpClusterSchedulerStatus::Overloaded:
        break;
    }
  }

  return unavailable == limit
      ? HttpClusterSchedulerStatus::Unavailable
      : HttpClusterSchedulerStatus::Overloaded;
}

HttpClusterSchedulerStatus
    HttpClusterScheduler::Chance::schedule(HttpClusterRequest* cr) {
  size_t unavailable = 0;

  for (auto& member: members()) {
    switch (member->tryProcess(cr)) {
      case HttpClusterSchedulerStatus::Success:
        return HttpClusterSchedulerStatus::Success;
      case HttpClusterSchedulerStatus::Unavailable:
        ++unavailable;
        break;
      case HttpClusterSchedulerStatus::Overloaded:
        break;
    }
  }

  return unavailable < members().size()
      ? HttpClusterSchedulerStatus::Overloaded
      : HttpClusterSchedulerStatus::Unavailable;
}

} // namespace client
} // namespace http
} // namespace xzero
