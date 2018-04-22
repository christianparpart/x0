// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/defines.h>
#include <xzero-flow/LiteralType.h>
#include <xzero-flow/vm/Handler.h>
#include <xzero-flow/vm/Program.h>
#include <xzero-flow/vm/Runner.h>
#include <xzero/net/IPAddress.h>
#include <xzero/net/Cidr.h>
#include <memory>

namespace xzero::flow {

class Params {
 public:
  using Value = Runner::Value;

  Params(Runner* caller, int argc)
      : caller_(caller), argc_(argc), argv_(argc + 1) {}

  void setArg(int argi, Value value) { argv_[argi] = value; }

  Runner* caller() const { return caller_; }

  void setResult(bool value) { argv_[0] = value; }
  void setResult(FlowNumber value) { argv_[0] = (Value) value; }
  void setResult(const Handler* handler) { argv_[0] = caller_->program()->indexOf(handler); }
  void setResult(const char* cstr) { argv_[0] = (Value) caller_->newString(cstr); }
  void setResult(const std::string& str) { argv_[0] = (Value) caller_->newString(str.data(), str.size()); }
  void setResult(const FlowString* str) { argv_[0] = (Value) str; }
  void setResult(const IPAddress* ip) { argv_[0] = (Value) ip; }
  void setResult(const Cidr* cidr) { argv_[0] = (Value) cidr; }

  int size() const { return argc_; }
  int count() const { return argc_; }

  Value at(size_t i) const { return argv_[i]; }
  Value operator[](size_t i) const { return argv_[i]; }
  Value& operator[](size_t i) { return argv_[i]; }

  bool getBool(size_t offset) const { return at(offset); }
  FlowNumber getInt(size_t offset) const { return at(offset); }
  const FlowString& getString(size_t offset) const { return *(FlowString*)at(offset); }
  Handler* getHandler(size_t offset) const { return caller_->program()->handler(static_cast<size_t>(at(offset))); }
  const IPAddress& getIPAddress(size_t offset) const { return *(IPAddress*)at(offset); }
  const Cidr& getCidr(size_t offset) const { return *(Cidr*)at(offset); }

  const FlowIntArray& getIntArray(size_t offset) const { return *(FlowIntArray*)at(offset); }
  const FlowStringArray& getStringArray(size_t offset) const { return *(FlowStringArray*)at(offset); }
  const FlowIPAddrArray& getIPAddressArray(size_t offset) const { return *(FlowIPAddrArray*)at(offset); }
  const FlowCidrArray& getCidrArray(size_t offset) const { return *(FlowCidrArray*)at(offset); }

  class iterator {  // {{{
   private:
    Params* params_;
    size_t current_;

   public:
    iterator(Params* p, size_t init) : params_(p), current_(init) {}
    iterator(const iterator& v) = default;

    size_t offset() const { return current_; }
    Value get() const { return params_->at(current_); }

    Value& operator*() { return params_->argv_[current_]; }
    const Value& operator*() const { return params_->argv_[current_]; }

    iterator& operator++() {
      if (current_ != static_cast<decltype(current_)>(params_->argc_)) {
        ++current_;
      }
      return *this;
    }

    bool operator==(const iterator& other) const {
      return current_ == other.current_;
    }

    bool operator!=(const iterator& other) const {
      return current_ != other.current_;
    }
  };  // }}}

  iterator begin() { return iterator(this, std::min(1, argc_)); }
  iterator end() { return iterator(this, argc_); }

 private:
  Runner* caller_;
  int argc_;
  std::vector<Value> argv_;
};

}  // namespace xzero::flow
