// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <vector>
#include <base/Counter.h>
#include <base/Severity.h>
#include <base/LogMessage.h>
#include "SchedulerStatus.h"

class Backend;
struct RequestNotes;

class Scheduler {
 public:
  typedef std::vector<Backend*> BackendList;

 protected:
  BackendList* backends_;

 protected:
  BackendList& backends() const { return *backends_; }

 public:
  explicit Scheduler(BackendList* backends);
  virtual ~Scheduler();

  virtual const std::string& name() const = 0;
  virtual SchedulerStatus schedule(RequestNotes* rn) = 0;
};

class ChanceScheduler : public Scheduler {
 public:
  explicit ChanceScheduler(BackendList* backends) : Scheduler(backends) {}

  const std::string& name() const override;
  SchedulerStatus schedule(RequestNotes* rn) override;
};

class RoundRobinScheduler : public Scheduler {
 public:
  explicit RoundRobinScheduler(BackendList* backends)
      : Scheduler(backends), next_(0) {}

  const std::string& name() const override;
  SchedulerStatus schedule(RequestNotes* rn) override;

 private:
  size_t next_;
};
