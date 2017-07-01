// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero-flow/FlowType.h>
#include <xzero-flow/Api.h>

namespace xzero {
namespace flow {

enum class UnaryOperator {
  // numerical
  INeg,
  INot,
  // boolean
  BNot,
  // string
  SLen,
  SIsEmpty,
};

enum class BinaryOperator {
  // numerical
  IAdd,
  ISub,
  IMul,
  IDiv,
  IRem,
  IPow,
  IAnd,
  IOr,
  IXor,
  IShl,
  IShr,
  ICmpEQ,
  ICmpNE,
  ICmpLE,
  ICmpGE,
  ICmpLT,
  ICmpGT,
  // boolean
  BAnd,
  BOr,
  BXor,
  // string
  SAdd,
  SSubStr,
  SCmpEQ,
  SCmpNE,
  SCmpLE,
  SCmpGE,
  SCmpLT,
  SCmpGT,
  SCmpRE,
  SCmpBeg,
  SCmpEnd,
  SIn,
  // ip
  PCmpEQ,
  PCmpNE,
  PInCidr,
};

XZERO_FLOW_API const char* cstr(BinaryOperator op);
XZERO_FLOW_API const char* cstr(UnaryOperator op);

class NopInstr;
class AllocaInstr;
class StoreInstr;
class LoadInstr;
class CallInstr;
class HandlerCallInstr;
class PhiNode;
class CondBrInstr;
class BrInstr;
class RetInstr;
class MatchInstr;
class CastInstr;

template <const UnaryOperator Operator, const FlowType ResultType>
class UnaryInstr;

template <const BinaryOperator Operator, const FlowType ResultType>
class BinaryInstr;

// numeric
typedef UnaryInstr<UnaryOperator::INeg, FlowType::Number> INegInstr;
typedef UnaryInstr<UnaryOperator::INot, FlowType::Number> INotInstr;
typedef BinaryInstr<BinaryOperator::IAdd, FlowType::Number> IAddInstr;
typedef BinaryInstr<BinaryOperator::ISub, FlowType::Number> ISubInstr;
typedef BinaryInstr<BinaryOperator::IMul, FlowType::Number> IMulInstr;
typedef BinaryInstr<BinaryOperator::IDiv, FlowType::Number> IDivInstr;
typedef BinaryInstr<BinaryOperator::IRem, FlowType::Number> IRemInstr;
typedef BinaryInstr<BinaryOperator::IPow, FlowType::Number> IPowInstr;
typedef BinaryInstr<BinaryOperator::IAnd, FlowType::Number> IAndInstr;
typedef BinaryInstr<BinaryOperator::IOr, FlowType::Number> IOrInstr;
typedef BinaryInstr<BinaryOperator::IXor, FlowType::Number> IXorInstr;
typedef BinaryInstr<BinaryOperator::IShl, FlowType::Number> IShlInstr;
typedef BinaryInstr<BinaryOperator::IShr, FlowType::Number> IShrInstr;

typedef BinaryInstr<BinaryOperator::ICmpEQ, FlowType::Boolean> ICmpEQInstr;
typedef BinaryInstr<BinaryOperator::ICmpNE, FlowType::Boolean> ICmpNEInstr;
typedef BinaryInstr<BinaryOperator::ICmpLE, FlowType::Boolean> ICmpLEInstr;
typedef BinaryInstr<BinaryOperator::ICmpGE, FlowType::Boolean> ICmpGEInstr;
typedef BinaryInstr<BinaryOperator::ICmpLT, FlowType::Boolean> ICmpLTInstr;
typedef BinaryInstr<BinaryOperator::ICmpGT, FlowType::Boolean> ICmpGTInstr;

// binary
typedef UnaryInstr<UnaryOperator::BNot, FlowType::Boolean> BNotInstr;
typedef BinaryInstr<BinaryOperator::BAnd, FlowType::Boolean> BAndInstr;
typedef BinaryInstr<BinaryOperator::BOr, FlowType::Boolean> BOrInstr;
typedef BinaryInstr<BinaryOperator::BXor, FlowType::Boolean> BXorInstr;

// string
typedef UnaryInstr<UnaryOperator::SLen, FlowType::Number> SLenInstr;
typedef UnaryInstr<UnaryOperator::SIsEmpty, FlowType::Boolean> SIsEmptyInstr;
typedef BinaryInstr<BinaryOperator::SAdd, FlowType::String> SAddInstr;
typedef BinaryInstr<BinaryOperator::SSubStr, FlowType::String> SSubStrInstr;
typedef BinaryInstr<BinaryOperator::SCmpEQ, FlowType::Boolean> SCmpEQInstr;
typedef BinaryInstr<BinaryOperator::SCmpNE, FlowType::Boolean> SCmpNEInstr;
typedef BinaryInstr<BinaryOperator::SCmpLE, FlowType::Boolean> SCmpLEInstr;
typedef BinaryInstr<BinaryOperator::SCmpGE, FlowType::Boolean> SCmpGEInstr;
typedef BinaryInstr<BinaryOperator::SCmpLT, FlowType::Boolean> SCmpLTInstr;
typedef BinaryInstr<BinaryOperator::SCmpGT, FlowType::Boolean> SCmpGTInstr;
typedef BinaryInstr<BinaryOperator::SCmpRE, FlowType::Boolean> SCmpREInstr;
typedef BinaryInstr<BinaryOperator::SCmpBeg, FlowType::Boolean> SCmpBegInstr;
typedef BinaryInstr<BinaryOperator::SCmpEnd, FlowType::Boolean> SCmpEndInstr;
typedef BinaryInstr<BinaryOperator::SIn, FlowType::Boolean> SInInstr;

// ip
typedef BinaryInstr<BinaryOperator::PCmpEQ, FlowType::Boolean> PCmpEQInstr;
typedef BinaryInstr<BinaryOperator::PCmpNE, FlowType::Boolean> PCmpNEInstr;
typedef BinaryInstr<BinaryOperator::PInCidr, FlowType::Boolean> PInCidrInstr;

class XZERO_FLOW_API InstructionVisitor {
 public:
  virtual ~InstructionVisitor() {};

  virtual void visit(NopInstr& instr) = 0;

  // storage
  virtual void visit(AllocaInstr& instr) = 0;
  virtual void visit(StoreInstr& instr) = 0;
  virtual void visit(LoadInstr& instr) = 0;
  virtual void visit(PhiNode& instr) = 0;

  // calls
  virtual void visit(CallInstr& instr) = 0;
  virtual void visit(HandlerCallInstr& instr) = 0;

  // terminator
  virtual void visit(CondBrInstr& instr) = 0;
  virtual void visit(BrInstr& instr) = 0;
  virtual void visit(RetInstr& instr) = 0;
  virtual void visit(MatchInstr& instr) = 0;

  // type cast
  virtual void visit(CastInstr& instr) = 0;

  // numeric
  virtual void visit(INegInstr& instr) = 0;
  virtual void visit(INotInstr& instr) = 0;
  virtual void visit(IAddInstr& instr) = 0;
  virtual void visit(ISubInstr& instr) = 0;
  virtual void visit(IMulInstr& instr) = 0;
  virtual void visit(IDivInstr& instr) = 0;
  virtual void visit(IRemInstr& instr) = 0;
  virtual void visit(IPowInstr& instr) = 0;
  virtual void visit(IAndInstr& instr) = 0;
  virtual void visit(IOrInstr& instr) = 0;
  virtual void visit(IXorInstr& instr) = 0;
  virtual void visit(IShlInstr& instr) = 0;
  virtual void visit(IShrInstr& instr) = 0;
  virtual void visit(ICmpEQInstr& instr) = 0;
  virtual void visit(ICmpNEInstr& instr) = 0;
  virtual void visit(ICmpLEInstr& instr) = 0;
  virtual void visit(ICmpGEInstr& instr) = 0;
  virtual void visit(ICmpLTInstr& instr) = 0;
  virtual void visit(ICmpGTInstr& instr) = 0;

  // boolean
  virtual void visit(BNotInstr& instr) = 0;
  virtual void visit(BAndInstr& instr) = 0;
  virtual void visit(BOrInstr& instr) = 0;
  virtual void visit(BXorInstr& instr) = 0;

  // string
  virtual void visit(SLenInstr& instr) = 0;
  virtual void visit(SIsEmptyInstr& instr) = 0;
  virtual void visit(SAddInstr& instr) = 0;
  virtual void visit(SSubStrInstr& instr) = 0;
  virtual void visit(SCmpEQInstr& instr) = 0;
  virtual void visit(SCmpNEInstr& instr) = 0;
  virtual void visit(SCmpLEInstr& instr) = 0;
  virtual void visit(SCmpGEInstr& instr) = 0;
  virtual void visit(SCmpLTInstr& instr) = 0;
  virtual void visit(SCmpGTInstr& instr) = 0;
  virtual void visit(SCmpREInstr& instr) = 0;
  virtual void visit(SCmpBegInstr& instr) = 0;
  virtual void visit(SCmpEndInstr& instr) = 0;
  virtual void visit(SInInstr& instr) = 0;

  // ip
  virtual void visit(PCmpEQInstr& instr) = 0;
  virtual void visit(PCmpNEInstr& instr) = 0;
  virtual void visit(PInCidrInstr& instr) = 0;
};

class Instr;

class IsSameInstruction : public InstructionVisitor {
 public:
  static bool test(Instr* a, Instr* b);
  static bool isSameOperands(Instr* a, Instr* b);

 private:
  Instr* other_;
  bool result_;

 protected:
  explicit IsSameInstruction(Instr* a);

  void visit(NopInstr& instr) override;

  // storage
  void visit(AllocaInstr& instr) override;
  void visit(StoreInstr& instr) override;
  void visit(LoadInstr& instr) override;
  void visit(PhiNode& instr) override;

  // calls
  void visit(CallInstr& instr) override;
  void visit(HandlerCallInstr& instr) override;

  // terminator
  void visit(CondBrInstr& instr) override;
  void visit(BrInstr& instr) override;
  void visit(RetInstr& instr) override;
  void visit(MatchInstr& instr) override;

  // type cast
  void visit(CastInstr& instr) override;

  // numeric
  void visit(INegInstr& instr) override;
  void visit(INotInstr& instr) override;
  void visit(IAddInstr& instr) override;
  void visit(ISubInstr& instr) override;
  void visit(IMulInstr& instr) override;
  void visit(IDivInstr& instr) override;
  void visit(IRemInstr& instr) override;
  void visit(IPowInstr& instr) override;
  void visit(IAndInstr& instr) override;
  void visit(IOrInstr& instr) override;
  void visit(IXorInstr& instr) override;
  void visit(IShlInstr& instr) override;
  void visit(IShrInstr& instr) override;
  void visit(ICmpEQInstr& instr) override;
  void visit(ICmpNEInstr& instr) override;
  void visit(ICmpLEInstr& instr) override;
  void visit(ICmpGEInstr& instr) override;
  void visit(ICmpLTInstr& instr) override;
  void visit(ICmpGTInstr& instr) override;

  // boolean
  void visit(BNotInstr& instr) override;
  void visit(BAndInstr& instr) override;
  void visit(BOrInstr& instr) override;
  void visit(BXorInstr& instr) override;

  // string
  void visit(SLenInstr& instr) override;
  void visit(SIsEmptyInstr& instr) override;
  void visit(SAddInstr& instr) override;
  void visit(SSubStrInstr& instr) override;
  void visit(SCmpEQInstr& instr) override;
  void visit(SCmpNEInstr& instr) override;
  void visit(SCmpLEInstr& instr) override;
  void visit(SCmpGEInstr& instr) override;
  void visit(SCmpLTInstr& instr) override;
  void visit(SCmpGTInstr& instr) override;
  void visit(SCmpREInstr& instr) override;
  void visit(SCmpBegInstr& instr) override;
  void visit(SCmpEndInstr& instr) override;
  void visit(SInInstr& instr) override;

  // ip
  void visit(PCmpEQInstr& instr) override;
  void visit(PCmpNEInstr& instr) override;
  void visit(PInCidrInstr& instr) override;
};

}  // namespace flow
}  // namespace xzero
