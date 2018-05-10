// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <flow/NativeCallback.h>
#include <xzero/net/IPAddress.h>
#include <xzero/net/Cidr.h>

namespace xzero::flow {

// constructs a handler callback
NativeCallback::NativeCallback(Runtime* runtime, const std::string& _name)
    : runtime_(runtime),
      isHandler_(true),
      verifier_(),
      function_(),
      signature_(),
      attributes_(0) {
  signature_.setName(_name);
  signature_.setReturnType(LiteralType::Boolean);
}

// constructs a function callback
NativeCallback::NativeCallback(Runtime* runtime, const std::string& _name,
                               LiteralType _returnType)
    : runtime_(runtime),
      isHandler_(false),
      verifier_(),
      function_(),
      signature_(),
      attributes_(0) {
  signature_.setName(_name);
  signature_.setReturnType(_returnType);
}

NativeCallback::~NativeCallback() {
}

bool NativeCallback::isHandler() const noexcept {
  return isHandler_;
}

bool NativeCallback::isFunction() const noexcept {
  return !isHandler_;
}

const std::string NativeCallback::name() const noexcept {
  return signature_.name();
}

const Signature& NativeCallback::signature() const noexcept {
  return signature_;
}

int NativeCallback::findParamByName(const std::string& name) const {
  for (int i = 0, e = names_.size(); i != e; ++i)
    if (names_[i] == name)
      return i;

  return -1;
}

NativeCallback& NativeCallback::setNoReturn() noexcept {
  attributes_ |= (unsigned) Attribute::NoReturn;
  return *this;
}

NativeCallback& NativeCallback::setReadOnly() noexcept {
  attributes_ |= (unsigned) Attribute::SideEffectFree;
  return *this;
}

NativeCallback& NativeCallback::setExperimental() noexcept {
  attributes_ |= (unsigned) Attribute::Experimental;
  return *this;
}

void NativeCallback::invoke(Params& args) const {
  function_(args);
}

}  // namespace xzero::flow
