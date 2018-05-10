// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <flow/ir/BasicBlock.h>
#include <flow/ir/IRHandler.h>
#include <flow/ir/Instr.h>
#include <flow/ir/Value.h>
#include <flow/util/strings.h>
#include <flow/util/assert.h>

#include <fmt/format.h>
#include <algorithm>
#include <cassert>

namespace xzero::flow {

static unsigned long long valueCounter = 1;

Value::Value(const Value& v) : type_(v.type_), name_(), uses_() {
  char buf[256];
  snprintf(buf, sizeof(buf), "%s_%llu", v.name().c_str(), valueCounter);
  valueCounter++;
  name_ = buf;
}

Value::Value(LiteralType ty, const std::string& name)
    : type_(ty), name_(name), uses_() {
  if (name_.empty()) {
    name_ = fmt::format("unnamed{}", valueCounter);
    valueCounter++;
    // printf("default-create name: %s\n", name_.c_str());
  }
}

Value::~Value() {
  FLOW_ASSERT(!isUsed(), fmt::format(
              "Value being destroyed is still in use by: {}.",
              join(uses_, ", ", &Instr::name)));
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
