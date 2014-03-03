/*
 * This file is part of the x0 web server project and is released under AGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2014 Christian Parpart <trapni@gmail.com>
 */

#include <x0/flow/ir/BasicBlock.h>
#include <x0/flow/ir/Instructions.h>
#include <x0/flow/ir/InstructionVisitor.h>
#include <x0/flow/ir/IRBuiltinHandler.h>
#include <x0/flow/ir/IRBuiltinFunction.h>
#include <utility>                              // make_pair
#include <assert.h>

namespace x0 {

using namespace FlowVM;

const char* cstr(UnaryOperator op) // {{{
{
    static const char* ops[] = {
        // numerical
        [(size_t)UnaryOperator::INeg] = "ineg",
        [(size_t)UnaryOperator::INot] = "inot",
        // binary
        [(size_t)UnaryOperator::BNot] = "bnot",
        // string
        [(size_t)UnaryOperator::SLen] = "slen",
        [(size_t)UnaryOperator::SIsEmpty] = "sisempty",
    };

    return ops[(size_t)op];
}
// }}}
const char* cstr(BinaryOperator op) // {{{
{
    static const char* ops[] = {
        // numerical
        [(size_t)BinaryOperator::IAdd] = "iadd",
        [(size_t)BinaryOperator::ISub] = "isub",
        [(size_t)BinaryOperator::IMul] = "imul",
        [(size_t)BinaryOperator::IDiv] = "idiv",
        [(size_t)BinaryOperator::IRem] = "irem",
        [(size_t)BinaryOperator::IPow] = "ipow",
        [(size_t)BinaryOperator::IAnd] = "iand",
        [(size_t)BinaryOperator::IOr] = "ior",
        [(size_t)BinaryOperator::IXor] = "ixor",
        [(size_t)BinaryOperator::IShl] = "ishl",
        [(size_t)BinaryOperator::IShr] = "ishr",
        [(size_t)BinaryOperator::ICmpEQ] = "icmpeq",
        [(size_t)BinaryOperator::ICmpNE] = "icmpne",
        [(size_t)BinaryOperator::ICmpLE] = "icmple",
        [(size_t)BinaryOperator::ICmpGE] = "icmpge",
        [(size_t)BinaryOperator::ICmpLT] = "icmplt",
        [(size_t)BinaryOperator::ICmpGT] = "icmpgt",
        // boolean
        [(size_t)BinaryOperator::BAnd] = "band",
        [(size_t)BinaryOperator::BOr] = "bor",
        [(size_t)BinaryOperator::BXor] = "bxor",
        // string
        [(size_t)BinaryOperator::SAdd] = "sadd",
        [(size_t)BinaryOperator::SSubStr] = "ssubstr",
        [(size_t)BinaryOperator::SCmpEQ] = "scmpeq",
        [(size_t)BinaryOperator::SCmpNE] = "scmpne",
        [(size_t)BinaryOperator::SCmpLE] = "scmple",
        [(size_t)BinaryOperator::SCmpGE] = "scmpge",
        [(size_t)BinaryOperator::SCmpLT] = "scmplt",
        [(size_t)BinaryOperator::SCmpGT] = "scmpgt",
        [(size_t)BinaryOperator::SCmpRE] = "scmpre",
        [(size_t)BinaryOperator::SCmpBeg] = "scmpbeg",
        [(size_t)BinaryOperator::SCmpEnd] = "scmpend",
        [(size_t)BinaryOperator::SIn] = "sin",
    };

    return ops[(size_t)op];
}
// }}}
// {{{ CastInstr
void CastInstr::dump()
{
    dumpOne(tos(type()).c_str());
}

void CastInstr::accept(InstructionVisitor& v)
{
    v.visit(*this);
}
// }}}
// {{{ CondBrInstr
CondBrInstr::CondBrInstr(Value* cond, BasicBlock* trueBlock, BasicBlock* falseBlock, const std::string& name) :
    BranchInstr({cond, trueBlock, falseBlock}, name)
{
}

void CondBrInstr::dump()
{
    dumpOne("condbr");
}

void CondBrInstr::accept(InstructionVisitor& visitor)
{
    visitor.visit(*this);
}
// }}}
// {{{ BrInstr
BrInstr::BrInstr(BasicBlock* targetBlock) :
    BranchInstr({targetBlock})
{
}

void BrInstr::dump()
{
    dumpOne("br");
}

void BrInstr::accept(InstructionVisitor& visitor)
{
    visitor.visit(*this);
}
// }}}
// {{{ MatchInstr
MatchInstr::MatchInstr(MatchClass op, Value* cond, const std::string& name) :
    BranchInstr({cond}, name),
    op_(op),
    cases_(),
    elseBlock_(nullptr)
{
}

void MatchInstr::addCase(Constant* label, BasicBlock* code)
{
    parent()->linkSuccessor(code);

    cases_.push_back(std::make_pair(label, code));
}

void MatchInstr::setElseBlock(BasicBlock* code)
{
    assert(elseBlock_ == nullptr);

    parent()->linkSuccessor(code);

    elseBlock_ = code;
}

void MatchInstr::dump()
{
    switch (op()) {
        case MatchClass::Same:
            dumpOne("match.same");
            break;
        case MatchClass::Head:
            dumpOne("match.head");
            break;
        case MatchClass::Tail:
            dumpOne("match.tail");
            break;
        case MatchClass::RegExp:
            dumpOne("match.re");
            break;
    }
}

void MatchInstr::accept(InstructionVisitor& visitor)
{
    visitor.visit(*this);
}
// }}}
// {{{ RetInstr
RetInstr::RetInstr(Value* result, const std::string& name) :
    BranchInstr({result}, name)
{
}

void RetInstr::dump()
{
    dumpOne("ret");
}

void RetInstr::accept(InstructionVisitor& visitor)
{
    visitor.visit(*this);
}
// }}}
// {{{ other instructions
template<typename T, typename U>
inline std::vector<U> join(const T& a, const std::vector<U>& vec)
{
    std::vector<U> res;

    res.push_back(a);
    for (const U& v: vec)
        res.push_back(v);

    return std::move(res);
}

CallInstr::CallInstr(IRBuiltinFunction* callee, const std::vector<Value*>& args, const std::string& name) :
    Instr(callee->signature().returnType(), join(callee, args), name)
{
}

HandlerCallInstr::HandlerCallInstr(IRBuiltinHandler* callee, const std::vector<Value*>& args) :
    Instr(FlowType::Boolean, join(callee, args), "")
{
}

void HandlerCallInstr::dump()
{
    dumpOne("hcall");
}

void HandlerCallInstr::accept(InstructionVisitor& visitor)
{
    visitor.visit(*this);
}

void AllocaInstr::accept(InstructionVisitor& visitor)
{
    visitor.visit(*this);
}

void ArraySetInstr::accept(InstructionVisitor& visitor)
{
    visitor.visit(*this);
}

void StoreInstr::accept(InstructionVisitor& visitor)
{
    visitor.visit(*this);
}

void LoadInstr::accept(InstructionVisitor& visitor)
{
    visitor.visit(*this);
}

void CallInstr::accept(InstructionVisitor& visitor)
{
    visitor.visit(*this);
}

void PhiNode::accept(InstructionVisitor& visitor)
{
    visitor.visit(*this);
}

PhiNode::PhiNode(const std::vector<Value*>& ops, const std::string& name) :
    Instr(ops[0]->type(), ops, name)
{
}

void PhiNode::dump()
{
    dumpOne("phi");
}

void AllocaInstr::dump()
{
    dumpOne("alloca");
}

void ArraySetInstr::dump()
{
    dumpOne("ARRAYSET");
}

void LoadInstr::dump()
{
    dumpOne("load");
}

void StoreInstr::dump()
{
    dumpOne("store");
}

void CallInstr::dump()
{
    dumpOne("CALL");
}

// }}}

} // namespace x0
