// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include "SchedulerStatus.h"
#include "Scheduler.h"
#include <functional>
#include <vector>
#include <memory>

class Backend;
class Scheduler;
struct RequestNotes;

/**
 * Manages a set of backends of one role.
 *
 * \see BackendRole
 * \see Director
 */
class BackendCluster {
 public:
  typedef std::vector<Backend*> List;

  BackendCluster();
  ~BackendCluster();

  template <typename T>
  void setScheduler() {
    auto old = scheduler_;
    scheduler_ = new T(&cluster_);
    delete old;
  }
  Scheduler* scheduler() const { return scheduler_; }

  SchedulerStatus schedule(RequestNotes* rn);

  bool empty() const { return cluster_.empty(); }
  size_t size() const { return cluster_.size(); }
  size_t capacity() const;

  void push_back(Backend* backend);
  void remove(Backend* backend);

  void each(const std::function<void(Backend*)>& cb);
  void each(const std::function<void(const Backend*)>& cb) const;
  bool find(const std::string& name, const std::function<void(Backend*)>& cb);
  Backend* find(const std::string& name);

 protected:
  List cluster_;
  Scheduler* scheduler_;
};
