// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero-flow/Api.h>
#include <xzero-flow/ir/Value.h>
#include <xzero-flow/ir/InstructionVisitor.h>
#include <xzero-flow/vm/Instruction.h>
#include <xzero-flow/vm/MatchClass.h>
#include <xzero-flow/vm/Signature.h>
#include <xzero/net/IPAddress.h>
#include <xzero/net/Cidr.h>
#include <xzero/RegExp.h>

#include <string>
#include <vector>
#include <list>

namespace xzero {
namespace flow {

class Instr;
class BasicBlock;
class IRHandler;
class IRProgram;
class IRBuilder;

class InstructionVisitor;

/**
 * Base class for native instructions.
 *
 * An instruction is derived from base class \c Value because its result can be
 * used as an operand for other instructions.
 *
 * @see IRBuilder
 * @see BasicBlock
 * @see IRHandler
 */
class XZERO_FLOW_API Instr : public Value {
 protected:
  Instr(const Instr& v);

 public:
  Instr(FlowType ty, const std::vector<Value*>& ops = {},
        const std::string& name = "");
  ~Instr();

  /**
   * Retrieves parent basic block this instruction is part of.
   */
  BasicBlock* parent() const { return parent_; }

  /**
   * Read-only access to operands.
   */
  const std::vector<Value*>& operands() const { return operands_; }

  /**
   * Retrieves n'th operand at given \p index.
   */
  Value* operand(size_t index) const { return operands_[index]; }

  /**
   * Adds given operand \p value to the end of the operand list.
   */
  void addOperand(Value* value);

  /**
   * Sets operand at index \p i to given \p value.
   *
   * This operation will potentially replace the value that has been at index \p
   *i before,
   * properly unlinking it from any uses or successor/predecessor links.
   */
  Value* setOperand(size_t i, Value* value);

  /**
   * Replaces \p old operand with \p replacement.
   *
   * @param old value to replace
   * @param replacement new value to put at the offset of \p old.
   *
   * @returns number of actual performed replacements.
   */
  size_t replaceOperand(Value* old, Value* replacement);

  /**
   * Clears out all operands.
   *
   * @see addOperand()
   */
  void clearOperands();

  /**
   * Replaces this instruction with the given @p newInstr.
   */
  void replace(Instr* newInstr);

  /**
   * Clones given instruction.
   *
   * This will not clone any of its operands but reference them.
   */
  virtual Instr* clone() = 0;

  /**
   * Generic extension interface.
   *
   * @param v extension to pass this instruction to.
   *
   * @see InstructionVisitor
   */
  virtual void accept(InstructionVisitor& v) = 0;

 protected:
  void dumpOne(const char* mnemonic);

  void setParent(BasicBlock* bb) { parent_ = bb; }

  friend class BasicBlock;

 private:
  BasicBlock* parent_;
  std::vector<Value*> operands_;
};

}  // namespace flow
}  // namespace xzero
