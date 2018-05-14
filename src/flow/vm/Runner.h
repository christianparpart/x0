// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <flow/LiteralType.h>
#include <flow/util/RegExp.h>
#include <flow/util/assert.h>
#include <flow/vm/Handler.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <iosfwd>
#include <list>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace xzero::flow {

// ExecutionEngine
// VM
class Runner {
 public:
  enum State {
    Inactive,   //!< No handler running nor suspended.
    Running,    //!< Active handler is currently running.
    Suspended,  //!< Active handler is currently suspended.
  };

  using Value = uint64_t;

  using TraceLogger = std::function<void(Instruction instr, size_t ip, size_t sp)>;

  class Stack { // {{{
   public:
    explicit Stack(size_t stackSize) : stack_() {
      stack_.reserve(stackSize);
    }

    void push(Value value) {
      stack_.push_back(value);
    }

    Value pop() {
      FLOW_ASSERT(stack_.size() > 0, "BUG: Cannot pop from empty stack.");
      Value v = stack_.back();
      stack_.pop_back();
      return v;
    }

    void discard(size_t n) {
      FLOW_ASSERT(n <= stack_.size(), "vm: Attempt to discard more items than available on stack.");
      n = std::min(n, stack_.size());
      stack_.resize(stack_.size() - n);
    }

    size_t size() const { return stack_.size(); }

    Value operator[](int relativeIndex) const {
      if (relativeIndex < 0) {
        FLOW_ASSERT(static_cast<size_t>(-relativeIndex - 1) < stack_.size(),
                    "vm: Attempt to load from stack beyond stack top");
        return stack_[stack_.size() + relativeIndex];
      } else {
        FLOW_ASSERT(static_cast<size_t>(relativeIndex) < stack_.size(),
                    "vm: Attempt to load from stack beyond stack top");
        return stack_[relativeIndex];
      }
    }

    Value& operator[](int relativeIndex) {
      if (relativeIndex < 0) {
        FLOW_ASSERT(static_cast<size_t>(-relativeIndex - 1) < stack_.size(),
                    "vm: Attempt to load from stack beyond stack top");
        return stack_[stack_.size() + relativeIndex];
      } else {
        FLOW_ASSERT(static_cast<size_t>(relativeIndex) < stack_.size(),
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

 public:
  Runner(const Handler* handler, void* userdata, TraceLogger logger);
  ~Runner();

  const Handler* handler() const noexcept { return handler_; }
  const Program* program() const noexcept { return program_; }
  void* userdata() const noexcept { return userdata_; }

  bool run();
  void suspend();
  bool resume();
  void rewind();

  /** Retrieves the last saved program execution offset. */
  size_t getInstructionPointer() const noexcept { return ip_; }

  /** Retrieves number of elements on stack. */
  size_t getStackPointer() const noexcept { return stack_.size(); }

  const util::RegExpContext* regexpContext() const noexcept { return &regexpContext_; }

  FlowString* newString(std::string value);

 private:
  //! retrieves a pointer to a an empty string constant.
  const FlowString* emptyString() const { return &*stringGarbage_.begin(); }

  FlowString* catString(const FlowString& a, const FlowString& b);

  const Stack& stack() const noexcept { return stack_; }
  Value stack(int si) const { return stack_[si]; }

  FlowNumber getNumber(int si) const { return static_cast<FlowNumber>(stack_[si]); }
  const FlowString& getString(int si) const { return *(FlowString*) stack_[si]; }
  const IPAddress& getIPAddress(int si) const { return *(IPAddress*) stack_[si]; }
  const Cidr& getCidr(int si) const { return *(Cidr*) stack_[si]; }
  const util::RegExp& getRegExp(int si) const { return *(util::RegExp*) stack_[si]; }

  const FlowString* getStringPtr(int si) const { return (FlowString*) stack_[si]; }
  const Cidr* getCidrPtr(int si) const { return (Cidr*) stack_[si]; }

  void push(Value value) { stack_.push(value); }
  Value pop() { return stack_.pop(); }
  void discard(size_t n) { stack_.discard(n); }
  void pushString(const FlowString* value) { push((Value) value); }

  bool loop();

  Runner(Runner&) = delete;
  Runner& operator=(Runner&) = delete;

 private:
  const Handler* handler_;
  TraceLogger traceLogger_;

  /**
   * We especially keep this ref to prevent ensure handler has
   * access to the program until the end, which is not garranteed
   * as it is only having a weak reference to the program (to avoid cycling
   * references).
   */
  const Program* program_;

  //! pointer to the currently evaluated HttpRequest/HttpResponse our case
  void* userdata_;

  util::RegExpContext regexpContext_;

  State state_;     //!< current VM state
  size_t ip_;       //!< last saved program execution offset

  size_t sp_;       //!< current stack depth (XXX used in debugging only)
  Stack stack_;     //!< runtime stack

  std::list<std::string> stringGarbage_;
};

}  // namespace xzero::flow
