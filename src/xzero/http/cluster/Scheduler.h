// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/http/cluster/SchedulerStatus.h>
#include <vector>
#include <string>
#include <memory>

namespace xzero::http::cluster {

class Backend;
class Context;

class Scheduler {
 public:
  using MemberList = std::vector<std::unique_ptr<Backend>>;

  explicit Scheduler(const std::string& name, MemberList* members);

  virtual ~Scheduler();

  const std::string& name() const { return name_; }
  MemberList& members() const { return *members_; }

  virtual SchedulerStatus schedule(Context* cn) = 0;

  class Chance;
  class RoundRobin;
  class LeastLoad; // TODO

 protected:
  std::string name_;
  MemberList* members_;
};

class Scheduler::RoundRobin : public Scheduler {
 public:
  RoundRobin(MemberList* members)
      : Scheduler("rr", members),
        next_(0) {}

  SchedulerStatus schedule(Context* cn) override;

 private:
  size_t next_;
};

class Scheduler::Chance : public Scheduler {
 public:
  Chance(MemberList* members)
      : Scheduler("rr", members) {}

  SchedulerStatus schedule(Context* cn) override;
};

} // namespace xzero::http::cluster
