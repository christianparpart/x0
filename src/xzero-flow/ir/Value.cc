// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero-flow/ir/Value.h>
#include <xzero-flow/ir/Instr.h>
#include <xzero-flow/ir/BasicBlock.h>
#include <xzero-flow/ir/IRHandler.h>
#include <xzero/logging.h>
#include <algorithm>
#include <assert.h>

namespace xzero::flow {

static unsigned long long valueCounter = 1;

Value::Value(const Value& v) : type_(v.type_), name_(), uses_() {
  char buf[256];
  snprintf(buf, sizeof(buf), "%s_%llu", v.name().c_str(), valueCounter);
  valueCounter++;
  name_ = buf;
}

Value::Value(FlowType ty, const std::string& name)
    : type_(ty), name_(name), uses_() {
  if (name_.empty()) {
    char buf[256];
    snprintf(buf, sizeof(buf), "unnamed%llu", valueCounter);
    valueCounter++;
    name_ = buf;
    // printf("default-create name: %s\n", name_.c_str());
  }
}

Value::~Value() {
  logTrace("Value($0).dtor", name());
  if (isUsed()) {
    logTrace("BUG! Value $0 is still in use by: $1",
             name(), StringUtil::join(uses_, ", ", &Value::name));
    for (auto* instr : uses_) {
      logTrace("In use by: $0 of block $1:", instr->name(),
          instr->getBasicBlock()->name());
      instr->dump();
      instr->getBasicBlock()->getHandler()->dump();
    }
  }
  assert(!isUsed() && "Value being destroyed is still in use.");
}

void Value::addUse(Instr* user) {
  uses_.push_back(user);
}

void Value::removeUse(Instr* user) {
  auto i = std::find(uses_.begin(), uses_.end(), user);

  assert(i != uses_.end());

  if (i != uses_.end()) {
    uses_.erase(i);
  }
}

void Value::replaceAllUsesWith(Value* newUse) {
  auto myUsers = uses_;

  for (Instr* user : myUsers) {
    user->replaceOperand(this, newUse);
  }
}

void Value::dump() {
  printf("Value '%s': %s\n", name_.c_str(), tos(type_).c_str());
}

}  // namespace xzero::flow
