// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <flow/ir/Instr.h>
#include <flow/ir/ConstantValue.h>
#include <flow/MatchClass.h>

#include <string>
#include <vector>
#include <list>

namespace xzero::flow {

class Instr;
class BasicBlock;
class IRProgram;
class IRBuilder;
class IRBuiltinHandler;
class IRBuiltinFunction;

class NopInstr : public Instr {
 public:
  NopInstr() : Instr(LiteralType::Void, {}, "nop") {}

  void dump() override;
  std::unique_ptr<Instr> clone() override;
  void accept(InstructionVisitor& v) override;
};

/**
 * Allocates an array of given type and elements.
 */
class AllocaInstr : public Instr {
 private:
  static LiteralType computeType(LiteralType elementType, Value* size) {  // {{{
    if (auto n = dynamic_cast<ConstantInt*>(size)) {
      if (n->get() == 1) return elementType;
    }

    switch (elementType) {
      case LiteralType::Number:
        return LiteralType::IntArray;
      case LiteralType::String:
        return LiteralType::StringArray;
      default:
        return LiteralType::Void;
    }
  }  // }}}

 public:
  AllocaInstr(LiteralType ty, Value* n, const std::string& name)
      : Instr(ty, {n}, name) {}

  LiteralType elementType() const {
    switch (type()) {
      case LiteralType::StringArray:
        return LiteralType::String;
      case LiteralType::IntArray:
        return LiteralType::Number;
      default:
        return LiteralType::Void;
    }
  }

  Value* arraySize() const { return operands()[0]; }

  void dump() override;
  std::unique_ptr<Instr> clone() override;
  void accept(InstructionVisitor& v) override;
};

class StoreInstr : public Instr {
 public:
  StoreInstr(Value* variable, ConstantInt* index, Value* source,
             const std::string& name)
      : Instr(LiteralType::Void, {variable, index, source}, name) {}

  Value* variable() const { return operand(0); }
  ConstantInt* index() const { return static_cast<ConstantInt*>(operand(1)); }
  Value* source() const { return operand(2); }

  void dump() override;
  std::unique_ptr<Instr> clone() override;
  void accept(InstructionVisitor& v) override;
};

class LoadInstr : public Instr {
 public:
  LoadInstr(Value* variable, const std::string& name)
      : Instr(variable->type(), {variable}, name) {}

  Value* variable() const { return operand(0); }

  void dump() override;
  std::unique_ptr<Instr> clone() override;
  void accept(InstructionVisitor& v) override;
};

class CallInstr : public Instr {
 public:
  CallInstr(const std::vector<Value*>& args, const std::string& name);
  CallInstr(IRBuiltinFunction* callee, const std::vector<Value*>& args,
            const std::string& name);

  IRBuiltinFunction* callee() const { return (IRBuiltinFunction*)operand(0); }

  void dump() override;
  std::unique_ptr<Instr> clone() override;
  void accept(InstructionVisitor& v) override;
};

class HandlerCallInstr : public Instr {
 public:
  explicit HandlerCallInstr(const std::vector<Value*>& args);
  HandlerCallInstr(IRBuiltinHandler* callee, const std::vector<Value*>& args);

  IRBuiltinHandler* callee() const { return (IRBuiltinHandler*)operand(0); }

  void dump() override;
  std::unique_ptr<Instr> clone() override;
  void accept(InstructionVisitor& v) override;
};

class CastInstr : public Instr {
 public:
  CastInstr(LiteralType resultType, Value* op, const std::string& name)
      : Instr(resultType, {op}, name) {}

  Value* source() const { return operand(0); }

  void dump() override;
  std::unique_ptr<Instr> clone() override;
  void accept(InstructionVisitor& v) override;
};

template <const UnaryOperator Operator, const LiteralType ResultType>
class UnaryInstr : public Instr {
 public:
  UnaryInstr(Value* op, const std::string& name)
      : Instr(ResultType, {op}, name), operator_(Operator) {}

  UnaryOperator op() const { return operator_; }

  void dump() override { dumpOne(cstr(operator_)); }

  std::unique_ptr<Instr> clone() override {
    return std::make_unique<UnaryInstr<Operator, ResultType>>(operand(0), name());
  }

  void accept(InstructionVisitor& v) override { v.visit(*this); }

 private:
  UnaryOperator operator_;
};

template <const BinaryOperator Operator, const LiteralType ResultType>
class BinaryInstr : public Instr {
 public:
  BinaryInstr(Value* lhs, Value* rhs, const std::string& name)
      : Instr(ResultType, {lhs, rhs}, name), operator_(Operator) {}

  BinaryOperator op() const { return operator_; }

  void dump() override { dumpOne(cstr(operator_)); }

  std::unique_ptr<Instr> clone() override {
    return std::make_unique<BinaryInstr<Operator, ResultType>>(
        operand(0), operand(1), name());
  }

  void accept(InstructionVisitor& v) override { v.visit(*this); }

 private:
  BinaryOperator operator_;
};

/**
 * Creates a PHI (phoney) instruction.
 *
 * Creates a synthetic instruction that purely informs the target register
 *allocator
 * to allocate the very same register for all given operands,
 * which is then used across all their basic blocks.
 */
class PhiNode : public Instr {
 public:
  PhiNode(const std::vector<Value*>& ops, const std::string& name);

  void dump() override;
  std::unique_ptr<Instr> clone() override;
  void accept(InstructionVisitor& v) override;
};

class TerminateInstr : public Instr {
 protected:
  TerminateInstr(const TerminateInstr& v) : Instr(v) {}

 public:
  TerminateInstr(const std::vector<Value*>& ops)
      : Instr(LiteralType::Void, ops, "") {}
};

/**
 * Conditional branch instruction.
 *
 * Creates a terminate instruction that transfers control to one of the two
 * given alternate basic blocks, depending on the given input condition.
 */
class CondBrInstr : public TerminateInstr {
 public:
  /**
   * Initializes the object.
   *
   * @param cond input condition that (if true) causes \p trueBlock to be jumped
   *to, \p falseBlock otherwise.
   * @param trueBlock basic block to run if input condition evaluated to true.
   * @param falseBlock basic block to run if input condition evaluated to false.
   */
  CondBrInstr(Value* cond, BasicBlock* trueBlock, BasicBlock* falseBlock);

  Value* condition() const { return operands()[0]; }
  BasicBlock* trueBlock() const { return (BasicBlock*)operands()[1]; }
  BasicBlock* falseBlock() const { return (BasicBlock*)operands()[2]; }

  void dump() override;
  std::unique_ptr<Instr> clone() override;
  void accept(InstructionVisitor& v) override;
};

/**
 * Unconditional jump instruction.
 */
class BrInstr : public TerminateInstr {
 public:
  explicit BrInstr(BasicBlock* targetBlock);

  BasicBlock* targetBlock() const { return (BasicBlock*)operands()[0]; }

  void dump() override;
  std::unique_ptr<Instr> clone() override;
  void accept(InstructionVisitor& v) override;
};

/**
 * handler-return instruction.
 */
class RetInstr : public TerminateInstr {
 public:
  RetInstr(Value* result);

  void dump() override;
  std::unique_ptr<Instr> clone() override;
  void accept(InstructionVisitor& v) override;
};

/**
 * Match instruction, implementing the Flow match-keyword.
 *
 * <li>operand[0] - condition</li>
 * <li>operand[1] - default block</li>
 * <li>operand[2n+2] - case label</li>
 * <li>operand[2n+3] - case block</li>
 */
class MatchInstr : public TerminateInstr {
 public:
  MatchInstr(const MatchInstr&);
  MatchInstr(MatchClass op, Value* cond);

  MatchClass op() const { return op_; }

  Value* condition() const { return operand(0); }

  void addCase(Constant* label, BasicBlock* code);
  std::vector<std::pair<Constant*, BasicBlock*>> cases() const;

  BasicBlock* elseBlock() const;
  void setElseBlock(BasicBlock* code);

  void dump() override;
  std::unique_ptr<Instr> clone() override;
  void accept(InstructionVisitor& v) override;

 private:
  MatchClass op_;
  std::vector<std::pair<Constant*, BasicBlock*>> cases_;
};

}  // namespace xzero::flow
