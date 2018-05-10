// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <flow/LiteralType.h>

#include <string>
#include <vector>

namespace xzero::flow {

class Instr;

/**
 * Defines an immutable IR value.
 */
class Value {
 protected:
  Value(const Value& v);

 public:
  Value(LiteralType ty, const std::string& name);
  virtual ~Value();

  LiteralType type() const { return type_; }
  void setType(LiteralType ty) { type_ = ty; }

  const std::string& name() const { return name_; }
  void setName(const std::string& n) { name_ = n; }

  /**
   * adds @p user to the list of instructions that are "using" this value.
   */
  void addUse(Instr* user);

  /**
   * removes @p user from the list of instructions that determines the list of
   * instructions that are using this value.
   */
  void removeUse(Instr* user);

  /**
   * Determines whether or not this value is being used by at least one other
   * instruction.
   */
  bool isUsed() const { return !uses_.empty(); }

  /**
   * Retrieves a range instructions that are *using* this value.
   */
  const std::vector<Instr*>& uses() const { return uses_; }

  /**
   * Replaces all uses of \c this value as operand with value \p newUse instead.
   *
   * @param newUse the new value to be used.
   */
  void replaceAllUsesWith(Value* newUse);

  virtual void dump();

 private:
  LiteralType type_;
  std::string name_;

  std::vector<Instr*> uses_;  //! list of instructions that <b>use</b> this value.
};

}  // namespace xzero::flow
