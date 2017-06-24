// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero-flow/Api.h>
#include <xzero-flow/FlowType.h>
#include <xzero-flow/vm/Handler.h>
#include <xzero-flow/vm/Instruction.h>
#include <xzero/CustomDataMgr.h>
#include <xzero/RegExp.h>
#include <utility>
#include <list>
#include <memory>
#include <new>
#include <cstdint>
#include <cstdio>
#include <cmath>
#include <string>

namespace xzero {
namespace flow {
namespace vm {

// ExecutionEngine
// VM
class XZERO_FLOW_API Runner : public CustomData {
 public:
  enum State {
    Inactive,   //!< No handler running nor suspended.
    Running,    //!< Active handler is currently running.
    Suspended,  //!< Active handler is currently suspended.
  };

 private:
  std::shared_ptr<Handler> handler_;

  /**
   * We especially keep this ref to prevent ensure handler has
   * access to the program until the end, which is not garranteed
   * as it is only having a weak reference to the program (to avoid cycling
   * references).
   */
  std::shared_ptr<Program> program_;

  //! pointer to the currently evaluated HttpRequest/HttpResponse our case
  std::pair<void*,void*> userdata_;

  RegExpContext regexpContext_;

  State state_;     //!< current VM state
  size_t pc_;       //!< last saved program execution offset

  std::list<Buffer> stringGarbage_;

  Register data_[];

 public:
  ~Runner();

  static std::unique_ptr<Runner> create(std::shared_ptr<Handler> handler);
  static void operator delete(void* p);

  bool run();
  void suspend();
  bool resume();
  void rewind();

  State state() const { return state_; }
  bool isInactive() const { return state_ == Inactive; }
  bool isRunning() const { return state_ == Running; }
  bool isSuspended() const { return state_ == Suspended; }

  std::shared_ptr<Handler> handler() const { return handler_; }
  std::shared_ptr<Program> program() const { return program_; }
  void* userdata() const { return userdata_.first; }
  void* userdata2() const { return userdata_.second; }
  void setUserData(void* p, void* q = nullptr) {
    userdata_.first = p;
    userdata_.second = q;
  }

  template<typename P, typename Q>
  inline void setUserData(std::pair<P, Q> udata) {
    setUserData(udata.first, udata.second);
  }

  const RegExpContext* regexpContext() const noexcept { return &regexpContext_; }
  RegExpContext* regexpContext() noexcept { return &regexpContext_; }

  const Register* data() const { return data_; }

  FlowString* newString(const std::string& value);
  FlowString* newString(const char* p, size_t n);
  FlowString* catString(const FlowString& a, const FlowString& b);
  const FlowString* emptyString() const { return &*stringGarbage_.begin(); }

 private:
  explicit Runner(std::shared_ptr<Handler> handler);

  inline bool loop();

  Runner(Runner&) = delete;
  Runner& operator=(Runner&) = delete;
};

}  // namespace vm
}  // namespace flow
}  // namespace xzero
