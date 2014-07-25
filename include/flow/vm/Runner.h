// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <flow/Api.h>
#include <flow/FlowType.h>
#include <flow/vm/Handler.h>
#include <flow/vm/Instruction.h>
#include <base/CustomDataMgr.h>
#include <utility>
#include <list>
#include <memory>
#include <new>
#include <cstdint>
#include <cstdio>
#include <cmath>
#include <string>

namespace flow {
namespace FlowVM {

// ExecutionEngine
// VM
class FLOW_API Runner : public CustomData {
 public:
  enum State {
    Inactive,   //!< No handler running nor suspended.
    Running,    //!< Active handler is currently running.
    Suspended,  //!< Active handler is currently suspended.
  };

 private:
  Handler* handler_;
  Program* program_;
  void* userdata_;  //!< pointer to the currently executed request handler in
                    //our case
  State state_;     //!< current VM state
  size_t pc_;       //!< last saved program execution offset

  std::list<Buffer> stringGarbage_;

  Register data_[];

 public:
  ~Runner();

  static std::unique_ptr<Runner> create(Handler* handler);
  static void operator delete(void* p);

  bool run();
  void suspend();
  bool resume();

  State state() const { return state_; }
  bool isInactive() const { return state_ == Inactive; }
  bool isRunning() const { return state_ == Running; }
  bool isSuspended() const { return state_ == Suspended; }

  Handler* handler() const { return handler_; }
  Program* program() const { return program_; }
  void* userdata() const { return userdata_; }
  void setUserData(void* p) { userdata_ = p; }

  const Register* data() const { return data_; }

  FlowString* newString(const std::string& value);
  FlowString* newString(const char* p, size_t n);
  FlowString* catString(const FlowString& a, const FlowString& b);
  const FlowString* emptyString() const { return &*stringGarbage_.begin(); }

 private:
  explicit Runner(Handler* handler);

  inline bool loop();

  Runner(Runner&) = delete;
  Runner& operator=(Runner&) = delete;
};

}  // namespace FlowVM
}  // namespace flow
