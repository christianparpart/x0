// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero-flow/ir/InstructionVisitor.h>
#include <xzero-flow/ir/Instructions.h>

namespace xzero {
namespace flow {

bool IsSameInstruction::test(Instr* a, Instr* b) {
  IsSameInstruction check(a);
  b->accept(check);

  if (!check.result_)
    return false;

  if (!isSameOperands(a, b))
    return false;

  return true;
}

bool IsSameInstruction::isSameOperands(Instr* a, Instr* b) {
  if (a->operands().size() != b->operands().size())
    return false;

  for (size_t i = 0, e = a->operands().size(); i != e; ++i)
    if (a->operands()[i] != b->operands()[i])
      return false;

  return true;
}

IsSameInstruction::IsSameInstruction(Instr* a)
    : other_(a),
      result_(false) {
}

// {{{ impl
#define IS_SAME_INSTR_IMPL(type)            \
  void IsSameInstruction::visit(type&) {    \
    result_ = dynamic_cast<type*>(other_);  \
  }

IS_SAME_INSTR_IMPL(NopInstr);
IS_SAME_INSTR_IMPL(AllocaInstr);
IS_SAME_INSTR_IMPL(StoreInstr);
IS_SAME_INSTR_IMPL(LoadInstr);
IS_SAME_INSTR_IMPL(PhiNode);
IS_SAME_INSTR_IMPL(CallInstr);
IS_SAME_INSTR_IMPL(HandlerCallInstr);
IS_SAME_INSTR_IMPL(CondBrInstr);
IS_SAME_INSTR_IMPL(BrInstr);
IS_SAME_INSTR_IMPL(RetInstr);
IS_SAME_INSTR_IMPL(MatchInstr);
IS_SAME_INSTR_IMPL(CastInstr);
IS_SAME_INSTR_IMPL(INegInstr);
IS_SAME_INSTR_IMPL(INotInstr);
IS_SAME_INSTR_IMPL(IAddInstr);
IS_SAME_INSTR_IMPL(ISubInstr);
IS_SAME_INSTR_IMPL(IMulInstr);
IS_SAME_INSTR_IMPL(IDivInstr);
IS_SAME_INSTR_IMPL(IRemInstr);
IS_SAME_INSTR_IMPL(IPowInstr);
IS_SAME_INSTR_IMPL(IAndInstr);
IS_SAME_INSTR_IMPL(IOrInstr);
IS_SAME_INSTR_IMPL(IXorInstr);
IS_SAME_INSTR_IMPL(IShlInstr);
IS_SAME_INSTR_IMPL(IShrInstr);
IS_SAME_INSTR_IMPL(ICmpEQInstr);
IS_SAME_INSTR_IMPL(ICmpNEInstr);
IS_SAME_INSTR_IMPL(ICmpLEInstr);
IS_SAME_INSTR_IMPL(ICmpGEInstr);
IS_SAME_INSTR_IMPL(ICmpLTInstr);
IS_SAME_INSTR_IMPL(ICmpGTInstr);
IS_SAME_INSTR_IMPL(BNotInstr);
IS_SAME_INSTR_IMPL(BAndInstr);
IS_SAME_INSTR_IMPL(BOrInstr);
IS_SAME_INSTR_IMPL(BXorInstr);
IS_SAME_INSTR_IMPL(SLenInstr);
IS_SAME_INSTR_IMPL(SIsEmptyInstr);
IS_SAME_INSTR_IMPL(SAddInstr);
IS_SAME_INSTR_IMPL(SSubStrInstr);
IS_SAME_INSTR_IMPL(SCmpEQInstr);
IS_SAME_INSTR_IMPL(SCmpNEInstr);
IS_SAME_INSTR_IMPL(SCmpLEInstr);
IS_SAME_INSTR_IMPL(SCmpGEInstr);
IS_SAME_INSTR_IMPL(SCmpLTInstr);
IS_SAME_INSTR_IMPL(SCmpGTInstr);
IS_SAME_INSTR_IMPL(SCmpREInstr);
IS_SAME_INSTR_IMPL(SCmpBegInstr);
IS_SAME_INSTR_IMPL(SCmpEndInstr);
IS_SAME_INSTR_IMPL(SInInstr);
IS_SAME_INSTR_IMPL(PCmpEQInstr);
IS_SAME_INSTR_IMPL(PCmpNEInstr);
IS_SAME_INSTR_IMPL(PInCidrInstr);
// }}}

}  // namespace flow
}  // namespace xzero
