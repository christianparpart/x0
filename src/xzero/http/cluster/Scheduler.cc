// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/http/cluster/Scheduler.h>
#include <xzero/http/cluster/Backend.h>
#include <xzero/http/cluster/Context.h>

namespace xzero::http::cluster {

Scheduler::Scheduler(const std::string& name, MemberList* members)
    : name_(name),
      members_(members) {
}

Scheduler::~Scheduler() {
}

SchedulerStatus Scheduler::RoundRobin::schedule(Context* cr) {
  unsigned unavailable = 0;
  size_t limit = members().size();

  for (size_t count = 0; count < limit; ++count, ++next_) {
    if (next_ >= limit) {
      next_ = 0;
    }

    switch (members()[next_]->tryProcess(cr)) {
      case SchedulerStatus::Success:
        return SchedulerStatus::Success;
      case SchedulerStatus::Unavailable:
        ++unavailable;
        break;
      case SchedulerStatus::Overloaded:
        break;
    }
  }

  return unavailable == limit
      ? SchedulerStatus::Unavailable
      : SchedulerStatus::Overloaded;
}

SchedulerStatus Scheduler::Chance::schedule(Context* cr) {
  size_t unavailable = 0;

  for (auto& member: members()) {
    switch (member->tryProcess(cr)) {
      case SchedulerStatus::Success:
        return SchedulerStatus::Success;
      case SchedulerStatus::Unavailable:
        ++unavailable;
        break;
      case SchedulerStatus::Overloaded:
        break;
    }
  }

  return unavailable < members().size()
      ? SchedulerStatus::Overloaded
      : SchedulerStatus::Unavailable;
}

} // namespace xzero::http::cluster
