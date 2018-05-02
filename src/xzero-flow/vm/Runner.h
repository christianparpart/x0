// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero-flow/LiteralType.h>
#include <xzero-flow/util/RegExp.h>
#include <xzero-flow/vm/Handler.h>

#include <xzero/CustomDataMgr.h>
#include <xzero/defines.h>
#include <xzero/logging.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iosfwd>
#include <list>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace xzero::flow {

// ExecutionEngine
// VM
class Runner : public CustomData {
 public:
  enum State {
    Inactive,   //!< No handler running nor suspended.
    Running,    //!< Active handler is currently running.
    Suspended,  //!< Active handler is currently suspended.
  };

  using Value = uint64_t;

  class Stack { // {{{
   public:
    explicit Stack(size_t stackSize) : stack_() {
      stack_.reserve(stackSize);
    }

    void push(Value value) {
      stack_.push_back(value);
    }

    Value pop() {
      XZERO_ASSERT(stack_.size() > 0, "BUG: Cannot pop from empty stack.");
      Value v = stack_.back();
      stack_.pop_back();
      return v;
    }

    void discard(size_t n) {
      assert(n <= stack_.size());
      n = std::min(n, stack_.size());
      stack_.resize(stack_.size() - n);
    }

    size_t size() const { return stack_.size(); }

    Value operator[](int relativeIndex) const {
      if (relativeIndex < 0) {
        XZERO_ASSERT(static_cast<size_t>(-relativeIndex - 1) < stack_.size(),
                     "vm: Attempt to load from stack beyond stack top");
        return stack_[stack_.size() + relativeIndex];
      } else {
        XZERO_ASSERT(static_cast<size_t>(relativeIndex) < stack_.size(),
                     "vm: Attempt to load from stack beyond stack top");
        return stack_[relativeIndex];
      }
    }

    Value& operator[](int relativeIndex) {
      if (relativeIndex < 0) {
        XZERO_ASSERT(static_cast<size_t>(-relativeIndex - 1) < stack_.size(),
                     "vm: Attempt to load from stack beyond stack top");
        return stack_[stack_.size() + relativeIndex];
      } else {
        XZERO_ASSERT(static_cast<size_t>(relativeIndex) < stack_.size(),
                     "vm: Attempt to load from stack beyond stack top");
        return stack_[relativeIndex];
      }
    }

    Value operator[](size_t absoluteIndex) const {
      return stack_[absoluteIndex];
    }

    Value& operator[](size_t absoluteIndex) {
      return stack_[absoluteIndex];
    }

   private:
    std::vector<Value> stack_;
  };
  // }}}

 private:
  const Handler* handler_;

  /**
   * We especially keep this ref to prevent ensure handler has
   * access to the program until the end, which is not garranteed
   * as it is only having a weak reference to the program (to avoid cycling
   * references).
   */
  const Program* program_;

  //! pointer to the currently evaluated HttpRequest/HttpResponse our case
  std::pair<void*,void*> userdata_;

  util::RegExpContext regexpContext_;

  State state_;     //!< current VM state
  size_t pc_;       //!< last saved program execution offset

  size_t sp_;       //!< current stack depth (XXX used in debugging only)
  Stack stack_;     //!< runtime stack

  std::list<std::string> stringGarbage_;

 public:
  explicit Runner(const Handler* handler);
  ~Runner();

  bool run();
  void suspend();
  bool resume();
  void rewind();

  size_t getInstructionPointer() const noexcept { return sp_; }
  size_t getStackPointer() const noexcept { return sp_; }

  size_t instructionOffset() const noexcept { return pc_; }
  State state() const noexcept { return state_; }
  bool isInactive() const noexcept { return state_ == Inactive; }
  bool isRunning() const noexcept { return state_ == Running; }
  bool isSuspended() const noexcept { return state_ == Suspended; }

  const Handler* handler() const noexcept { return handler_; }
  const Program* program() const noexcept { return program_; }
  void* userdata() const noexcept { return userdata_.first; }
  void* userdata2() const noexcept { return userdata_.second; }
  void setUserData(void* p, void* q = nullptr) noexcept {
    userdata_.first = p;
    userdata_.second = q;
  }

  template<typename P, typename Q>
  inline void setUserData(std::pair<P, Q> udata) noexcept {
    setUserData(udata.first, udata.second);
  }

  const util::RegExpContext* regexpContext() const noexcept { return &regexpContext_; }
  util::RegExpContext* regexpContext() noexcept { return &regexpContext_; }

  const Stack& stack() const noexcept { return stack_; }
  Value stack(int si) const { return stack_[si]; }

  FlowNumber getNumber(int si) const { return static_cast<FlowNumber>(stack_[si]); }
  const FlowString& getString(int si) const { return *(FlowString*) stack_[si]; }
  const IPAddress& getIPAddress(int si) const { return *(IPAddress*) stack_[si]; }
  const Cidr& getCidr(int si) const { return *(Cidr*) stack_[si]; }
  const util::RegExp& getRegExp(int si) const { return *(util::RegExp*) stack_[si]; }

  const FlowString* getStringPtr(int si) const { return (FlowString*) stack_[si]; }
  const Cidr* getCidrPtr(int si) const { return (Cidr*) stack_[si]; }

  FlowString* newString(const std::string& value);
  FlowString* newString(const char* p, size_t n);
  FlowString* catString(const FlowString& a, const FlowString& b);
  const FlowString* emptyString() const { return &*stringGarbage_.begin(); }

 private:
  void push(Value value) { stack_.push(value); }
  Value pop() { return stack_.pop(); }
  void discard(size_t n) { stack_.discard(n); }
  void pushString(const FlowString* value) { push((Value) value); }

  bool loop();

  Runner(Runner&) = delete;
  Runner& operator=(Runner&) = delete;
};

}  // namespace xzero::flow
