/*
 * This file is part of the x0 web server project and is released under AGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2014 Christian Parpart <trapni@gmail.com>
 */

#include <x0/flow/ir/Instr.h>
#include <x0/flow/ir/ConstantValue.h>
#include <x0/flow/ir/IRBuiltinHandler.h>
#include <x0/flow/ir/IRBuiltinFunction.h>
#include <x0/flow/ir/BasicBlock.h>
#include <utility>
#include <assert.h>

namespace x0 {

using namespace FlowVM;

Instr::Instr(FlowType ty, const std::vector<Value*>& ops, const std::string& name) :
    Value(ty, name),
    parent_(nullptr),
    operands_(ops)
{
    for (Value* op: operands_) {
        op->addUse(this);
    }
}

Instr::~Instr()
{
}

size_t Instr::replaceOperand(Value* old, Value* replacement)
{
    size_t count = 0;

    printf("replaceOperand: %s\n", old->name().c_str());
    printf("          with: %s\n", replacement->name().c_str());

    for (size_t i = 0, e = operands_.size(); i != e; ++i) {
        if (operands_[i] == old) {
            printf("found at operand #%zu\n", i);
            operands_[i] = replacement;

            if (BasicBlock* oldBB = dynamic_cast<BasicBlock*>(old)) {
                printf("unlinking oldBB: %s\n", oldBB->name().c_str());
                parent()->unlinkSuccessor(oldBB);
            }

            if (BasicBlock* newBB = dynamic_cast<BasicBlock*>(replacement)) {
                printf("linking newBB: %s\n", newBB->name().c_str());
                parent()->linkSuccessor(newBB);
            }

            ++count;
        }
    }

    return count;
}

void Instr::dumpOne(const char* mnemonic)
{
    if (type() != FlowType::Void) {
        printf("\t%%%s = %s", name().c_str() ?: "?", mnemonic);
    } else {
        printf("\t%s", mnemonic);
    }

    for (size_t i = 0, e = operands_.size(); i != e; ++i) {
        printf(i ? ", " : " ");
        Value* arg = operands_[i];
        if (dynamic_cast<Constant*>(arg)) {
            if (auto i = dynamic_cast<ConstantInt*>(arg)) {
                printf("%li", i->get());
                continue;
            }
            if (auto s = dynamic_cast<ConstantString*>(arg)) {
                printf("\"%s\"", s->get().c_str());
                continue;
            }
            if (auto ip = dynamic_cast<ConstantIP*>(arg)) {
                printf("%s", ip->get().c_str());
                continue;
            }
            if (auto cidr = dynamic_cast<ConstantCidr*>(arg)) {
                printf("%s", cidr->get().str().c_str());
                continue;
            }
            if (auto re = dynamic_cast<ConstantRegExp*>(arg)) {
                printf("/%s/", re->get().pattern().c_str());
                continue;
            }
            if (auto bh = dynamic_cast<IRBuiltinHandler*>(arg)) {
                printf("%s", bh->get().to_s().c_str());
                continue;
            }
            if (auto bf = dynamic_cast<IRBuiltinFunction*>(arg)) {
                printf("%s", bf->get().to_s().c_str());
                continue;
            }
        }
        printf("%%%s", arg->name().c_str());
    }
    printf("\n");
}

} // namespace x0