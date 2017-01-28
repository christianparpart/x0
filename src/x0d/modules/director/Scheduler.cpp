// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include "Scheduler.h"
#include "Backend.h"
#include "RequestNotes.h"
#include <xzero/HttpWorker.h>

Scheduler::Scheduler(BackendList* backends) : backends_(backends) {}

Scheduler::~Scheduler() {}

// ChanceScheduler
const std::string& ChanceScheduler::name() const {
  static const std::string myName("chance");
  return myName;
}

SchedulerStatus ChanceScheduler::schedule(RequestNotes* rn) {
  size_t unavailable = 0;

  for (auto backend : backends()) {
    switch (backend->tryProcess(rn)) {
      case SchedulerStatus::Success:
        return SchedulerStatus::Success;
      case SchedulerStatus::Unavailable:
        ++unavailable;
        break;
      case SchedulerStatus::Overloaded:
        break;
    }
  }

  return unavailable < backends().size() ? SchedulerStatus::Overloaded
                                         : SchedulerStatus::Unavailable;
}

// RoundRobinScheduler
const std::string& RoundRobinScheduler::name() const {
  static const std::string myName("rr");
  return myName;
}

SchedulerStatus RoundRobinScheduler::schedule(RequestNotes* rn) {
  unsigned unavailable = 0;

  for (size_t count = 0, limit = backends().size(); count < limit;
       ++count, ++next_) {
    if (next_ >= limit) next_ = 0;

    switch (backends()[next_++]->tryProcess(rn)) {
      case SchedulerStatus::Success:
        return SchedulerStatus::Success;
      case SchedulerStatus::Unavailable:
        ++unavailable;
        break;
      case SchedulerStatus::Overloaded:
        break;
    }
  }

  return backends().size() == unavailable ? SchedulerStatus::Unavailable
                                          : SchedulerStatus::Overloaded;
}
