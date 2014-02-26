/* <IR.cpp>
 *
 * This file is part of the x0 web server project and is released under AGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2014 Christian Parpart <trapni@gmail.com>
 */

#include <x0/flow/IR.h>
#include <x0/flow/AST.h>
#include <x0/flow/InstructionVisitor.h>
#include <x0/DebugLogger.h> // XZERO_DEBUG
#include <utility>          // make_pair
#include <assert.h>
#include <math.h>

/*
 * GOAL:
 * ~~~~~
 *
 * - Optimization passes
 *   - same-BasicBlock merging
 *   - more intelligent register allocator
 *   - dead code elimination
 *   - jump threading
 *
 * TODOS:
 * - assert() on last instruction in current BB is not a terminator instr.
 */

namespace x0 {

using namespace FlowVM;

#define FLOW_DEBUG_IR 1

#if defined(FLOW_DEBUG_IR)
#	define TRACE(level, msg...) XZERO_DEBUG("IR", (level), msg)
#else
#	define TRACE(level, msg...) /*!*/
#endif

const char* cstr(UnaryOperator op)
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

const char* cstr(BinaryOperator op)
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

// {{{ Value
Value::Value(FlowType ty, const std::string& name) :
    type_(ty),
    name_(name),
    uses_()
{
    if (name_.empty()) {
        static unsigned long long i = 1;
        char buf[256];
        snprintf(buf, sizeof(buf), "unnamed%llu", i);
        i++;
        name_ = buf;
        //printf("default-create name: %s\n", name_.c_str());
    }
}

Value::~Value()
{
}

void Value::addUse(Instr* user)
{
    uses_.push_back(user);
}

void Value::dump()
{
    printf("Value '%s': %s\n", name_.c_str(), tos(type_).c_str());
}
// }}}

void IRVariable::dump()
{
    printf("%%%s; variable of type %s\n", name().c_str(), tos(type()).c_str());
}

// {{{ Constant
void Constant::dump()
{
    printf("Constant %zu '%s': %s\n", id_, name().c_str(), tos(type()).c_str());
}
template class X0_EXPORT ConstantValue<int64_t, FlowType::Number>;
template class X0_EXPORT ConstantValue<bool, FlowType::Boolean>;
template class X0_EXPORT ConstantValue<std::string, FlowType::String>;
template class X0_EXPORT ConstantValue<IPAddress, FlowType::IPAddress>;
template class X0_EXPORT ConstantValue<Cidr, FlowType::Cidr>;
template class X0_EXPORT ConstantValue<RegExp, FlowType::RegExp>;
// }}}
// {{{ Instr
Instr::Instr(FlowType ty, const std::vector<Value*>& ops, const std::string& name) :
    Value(ty, name),
    parent_(nullptr),
    operands_(ops)
{
    for (Value* op: operands_) {
        op->addUse(this);
    }
}

Instr::~Instr() {
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
            if (auto bf = dynamic_cast<IRBuiltinFunction*>(arg)) {
                printf("%s", bf->get().to_s().c_str());
                continue;
            }
        }
        printf("%%%s", arg->name().c_str());
    }
    printf("\n");
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
// {{{ BranchInstr
void BranchInstr::dump()
{
    dumpOne("br");
}

void BranchInstr::accept(InstructionVisitor& visitor)
{
    visitor.visit(*this);
}
// }}}
// {{{ other instructions
template<typename T, typename U>
std::vector<U> join(const T& a, const std::vector<U>& vec)
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

void CondBrInstr::accept(InstructionVisitor& visitor)
{
    visitor.visit(*this);
}

void BrInstr::accept(InstructionVisitor& visitor)
{
    visitor.visit(*this);
}

void RetInstr::accept(InstructionVisitor& visitor)
{
    visitor.visit(*this);
}

void MatchInstr::accept(InstructionVisitor& visitor)
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

void CondBrInstr::dump()
{
    dumpOne("CONDBR");
}

void BrInstr::dump()
{
    dumpOne("BR");
}

void RetInstr::dump()
{
    dumpOne("RET");
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

void MatchInstr::dump()
{
    dumpOne("MATCH");
}
void CallInstr::dump()
{
    dumpOne("CALL");
}

// }}}
// {{{ MatchInstr
MatchInstr::MatchInstr(MatchClass op, Value* cond, const std::string& name) :
    Instr(FlowType::Void, {}, name),
    op_(op),
    cases_(),
    elseBlock_(nullptr)
{
    assert(cond != nullptr);
    operands().push_back(cond);
}

void MatchInstr::addCase(Constant* label, BasicBlock* code)
{
    parent()->link(code);

    cases_.push_back(std::make_pair(label, code));
}

void MatchInstr::setElseBlock(BasicBlock* code)
{
    assert(elseBlock_ == nullptr);

    parent()->link(code);

    elseBlock_ = code;
}
// }}}
// {{{ BasicBlock
BasicBlock::BasicBlock(const std::string& name) :
    Value(FlowType::Void, name),
    parent_(nullptr),
    code_(),
    predecessors_(),
    successors_()
{
}

BasicBlock::~BasicBlock()
{
    for (auto instr: code_) {
        delete instr;
    }
}

void BasicBlock::dump()
{
    printf("%%%s:\n", name().c_str());
    for (size_t i = 0, e = code_.size(); i != e; ++i) {
        code_[i]->dump();
    }
    printf("\n");
}

void BasicBlock::link(BasicBlock* successor)
{
    assert(successor != nullptr);

    successors().push_back(successor);
    successor->predecessors().push_back(this);
}

void BasicBlock::unlink(BasicBlock* successor)
{
    assert(!"TODO");
}

std::vector<BasicBlock*> BasicBlock::dominators()
{
    std::vector<BasicBlock*> result;
    collectIDom(result);
    result.push_back(this);
    return result;
}

std::vector<BasicBlock*> BasicBlock::immediateDominators()
{
    std::vector<BasicBlock*> result;
    collectIDom(result);
    return result;
}

void BasicBlock::collectIDom(std::vector<BasicBlock*>& output)
{
    for (BasicBlock* p: predecessors()) {
        p->collectIDom(output);
    }
}
// }}}
// {{{ IRHandler
IRHandler::IRHandler(size_t id, const std::string& name) :
    Constant(FlowType::Handler, id, name),
    parent_(nullptr),
    entryPoint_(nullptr),
    blocks_()
{
}

IRHandler::~IRHandler()
{
}

BasicBlock* IRHandler::setEntryPoint(BasicBlock* bb)
{
    assert(bb->parent() == this);
    assert(entryPoint_ == nullptr && "QA: changing EP not allowed.");

    entryPoint_ = bb;

    return bb;
}

void IRHandler::dump()
{
    printf(".handler %s ; entryPoint = %%%s\n", name().c_str(), entryPoint_->name().c_str());

    for (BasicBlock* bb: blocks_)
        bb->dump();

    printf("\n");
}
// }}}
// {{{ IRProgram
IRProgram::IRProgram() :
    imports_(),
    numbers_(),
    strings_(),
    ipaddrs_(),
    cidrs_(),
    regexps_(),
    builtinFunctions_(),
    builtinHandlers_(),
    handlers_()
{
}

IRProgram::~IRProgram()
{
    for (auto& value: numbers_) delete value;
    for (auto& value: strings_) delete value;
    for (auto& value: ipaddrs_) delete value;
    for (auto& value: cidrs_) delete value;
    for (auto& value: regexps_) delete value;
    for (auto& value: handlers_) delete value;
}

void IRProgram::dump()
{
    printf("; IRProgram\n");

    for (auto handler: handlers_)
        handler->dump();
}

template<typename T, typename U>
T* IRProgram::get(std::vector<T*>& table, const U& literal)
{
    for (size_t i = 0, e = table.size(); i != e; ++i)
        if (table[i]->get() == literal)
            return table[i];

    T* value = new T(table.size(), literal);
    table.push_back(value);
    return value;
}

template ConstantInt* IRProgram::get<ConstantInt, int64_t>(std::vector<ConstantInt*>&, const int64_t&);
template ConstantString* IRProgram::get<ConstantString, std::string>(std::vector<ConstantString*>&, const std::string&);
template ConstantIP* IRProgram::get<ConstantIP, IPAddress>(std::vector<ConstantIP*>&, const IPAddress&);
template ConstantCidr* IRProgram::get<ConstantCidr, Cidr>(std::vector<ConstantCidr*>&, const Cidr&);
template ConstantRegExp* IRProgram::get<ConstantRegExp, RegExp>(std::vector<ConstantRegExp*>&, const RegExp&);
template IRBuiltinHandler* IRProgram::get<IRBuiltinHandler, Signature>(std::vector<IRBuiltinHandler*>&, const Signature&);
template IRBuiltinFunction* IRProgram::get<IRBuiltinFunction, Signature>(std::vector<IRBuiltinFunction*>&, const Signature&);
// }}}
// {{{ IRBuilder
IRBuilder::IRBuilder() :
    program_(nullptr),
    handler_(nullptr),
    insertPoint_(nullptr),
    nameStore_()
{
}

IRBuilder::~IRBuilder()
{
}

// {{{ name management
std::string IRBuilder::makeName(const std::string& name)
{
    const std::string theName = name.empty() ? "tmp" : name;

    auto i = nameStore_.find(theName);
    if (i == nameStore_.end()) {
        nameStore_[theName] = 0;
        return theName;
    }

    unsigned long id = id = ++i->second;

    char buf[512];
    snprintf(buf, sizeof(buf), "%s%lu", theName.c_str(), id);
    return buf;
}
// }}}
// {{{ context management
void IRBuilder::setProgram(IRProgram* prog)
{
    program_ = prog;
    handler_ = nullptr;
    insertPoint_ = nullptr;
}

IRHandler* IRBuilder::setHandler(IRHandler* hn)
{
    assert(hn->parent() == program_);

    handler_ = hn;
    insertPoint_ = nullptr;

    return hn;
}

BasicBlock* IRBuilder::createBlock(const std::string& name)
{
    std::string n = makeName(name);

    TRACE(1, "createBlock() %s", n.c_str());
    BasicBlock* bb = new BasicBlock(n);

    bb->setParent(handler_);
    handler_->blocks_.push_back(bb);

    return bb;
}

void IRBuilder::setInsertPoint(BasicBlock* bb)
{
    assert(bb != nullptr);
    assert(bb->parent() == handler() && "insert point must belong to the current handler.");

    TRACE(1, "setInsertPoint() %s", bb->name().c_str());

    insertPoint_ = bb;
    bb->parent_ = handler_;
}

Instr* IRBuilder::insert(Instr* instr)
{
    assert(instr != nullptr);
    assert(getInsertPoint() != nullptr);
    assert(instr->parent() == nullptr);

    instr->setParent(getInsertPoint());
    getInsertPoint()->code_.push_back(instr);

    // XXX the resulting type of a basic block always equals the one of its last inserted instruction
    getInsertPoint()->setType(instr->type());

    return instr;
}
// }}}
// {{{ handler pool
IRHandler* IRBuilder::getHandler(const std::string& name)
{
    for (auto& item: program_->handlers_) {
        if (item->name() == name)
            return item;
    }

    size_t id = program_->handlers_.size();
    IRHandler* h = new IRHandler(id, name);

    h->setParent(program_);
    program_->handlers_.push_back(h);

    return h;
}
// }}}
// {{{ value management
/**
 * dynamically allocates an array of given element type and size.
 */
AllocaInstr* IRBuilder::createAlloca(FlowType ty, Value* arraySize, const std::string& name)
{
    return static_cast<AllocaInstr*>(insert(new AllocaInstr(ty, arraySize, makeName(name))));
}

/**
 * initializes an array at given index.
 */
Instr* IRBuilder::createArraySet(Value* array, Value* index, Value* value, const std::string& name)
{
    return insert(new ArraySetInstr(array, index, value, makeName(name)));
}

/**
 * Loads given value
 */
Value* IRBuilder::createLoad(Value* value, const std::string& name)
{
    if (dynamic_cast<Constant*>(value))
        return value;

    //if (dynamic_cast<Variable*>(value))
        return insert(new LoadInstr(value, makeName(name)));

    assert(!"Value must be of type Constant or Variable.");
    return nullptr;
}

/**
 * emits a STORE of value \p rhs to variable \p lhs.
 */
Instr* IRBuilder::createStore(Value* lhs, Value* rhs, const std::string& name)
{
    assert(dynamic_cast<AllocaInstr*>(lhs) && "lhs must be of type AllocaInstr in order to STORE to.");
    //assert(lhs->type() == rhs->type() && "Type of lhs and rhs must be equal.");
    //assert(dynamic_cast<IRVariable*>(lhs) && "lhs must be of type Variable.");

    return insert(new StoreInstr(lhs, rhs, makeName(name)));
}

Instr* IRBuilder::createPhi(const std::vector<Value*>& incomings, const std::string& name)
{
    return insert(new PhiNode(incomings, makeName(name)));
}
// }}}
// {{{ numerical ops
Value* IRBuilder::createNeg(Value* rhs, const std::string& name)
{
    assert(rhs->type() == FlowType::Number);

    if (auto a = dynamic_cast<ConstantInt*>(rhs))
        return get(-a->get());

    return insert(new INegInstr(rhs, makeName(name)));
}

Value* IRBuilder::createAdd(Value* lhs, Value* rhs, const std::string& name)
{
    assert(lhs->type() == rhs->type());
    assert(lhs->type() == FlowType::Number);

    if (auto a = dynamic_cast<ConstantInt*>(lhs))
        if (auto b = dynamic_cast<ConstantInt*>(rhs))
            return get(a->get() + b->get());

    return insert(new IAddInstr(lhs, rhs, makeName(name)));
}

Value* IRBuilder::createSub(Value* lhs, Value* rhs, const std::string& name)
{
    assert(lhs->type() == rhs->type());
    assert(lhs->type() == FlowType::Number);

    if (auto a = dynamic_cast<ConstantInt*>(lhs))
        if (auto b = dynamic_cast<ConstantInt*>(rhs))
            return get(a->get() - b->get());

    return insert(new ISubInstr(lhs, rhs, makeName(name)));
}

Value* IRBuilder::createMul(Value* lhs, Value* rhs, const std::string& name)
{
    assert(lhs->type() == rhs->type());
    assert(lhs->type() == FlowType::Number);

    if (auto a = dynamic_cast<ConstantInt*>(lhs))
        if (auto b = dynamic_cast<ConstantInt*>(rhs))
            return get(a->get() * b->get());

    return insert(new IMulInstr(lhs, rhs, makeName(name)));
}

Value* IRBuilder::createDiv(Value* lhs, Value* rhs, const std::string& name)
{
    assert(lhs->type() == rhs->type());
    assert(lhs->type() == FlowType::Number);

    if (auto a = dynamic_cast<ConstantInt*>(lhs))
        if (auto b = dynamic_cast<ConstantInt*>(rhs))
            return get(a->get() / b->get());

    return insert(new IDivInstr(lhs, rhs, makeName(name)));
}

Value* IRBuilder::createRem(Value* lhs, Value* rhs, const std::string& name)
{
    assert(lhs->type() == rhs->type());
    assert(lhs->type() == FlowType::Number);

    if (auto a = dynamic_cast<ConstantInt*>(lhs))
        if (auto b = dynamic_cast<ConstantInt*>(rhs))
            return get(a->get() % b->get());

    return insert(new IRemInstr(lhs, rhs, makeName(name)));
}

Value* IRBuilder::createShl(Value* lhs, Value* rhs, const std::string& name)
{
    assert(lhs->type() == rhs->type());
    assert(lhs->type() == FlowType::Number);

    if (auto a = dynamic_cast<ConstantInt*>(lhs))
        if (auto b = dynamic_cast<ConstantInt*>(rhs))
            return get(a->get() << b->get());

    return insert(new IShlInstr(lhs, rhs, makeName(name)));
}

Value* IRBuilder::createShr(Value* lhs, Value* rhs, const std::string& name)
{
    assert(lhs->type() == rhs->type());
    assert(lhs->type() == FlowType::Number);

    if (auto a = dynamic_cast<ConstantInt*>(lhs))
        if (auto b = dynamic_cast<ConstantInt*>(rhs))
            return get(a->get() >> b->get());

    return insert(new IShrInstr(lhs, rhs, makeName(name)));
}

Value* IRBuilder::createPow(Value* lhs, Value* rhs, const std::string& name)
{
    assert(lhs->type() == rhs->type());
    assert(lhs->type() == FlowType::Number);

    if (auto a = dynamic_cast<ConstantInt*>(lhs))
        if (auto b = dynamic_cast<ConstantInt*>(rhs))
            return get(powl(a->get(), b->get()));

    return insert(new IPowInstr(lhs, rhs, makeName(name)));
}

Value* IRBuilder::createAnd(Value* lhs, Value* rhs, const std::string& name)
{
    assert(lhs->type() == rhs->type());
    assert(lhs->type() == FlowType::Number);

    if (auto a = dynamic_cast<ConstantInt*>(lhs))
        if (auto b = dynamic_cast<ConstantInt*>(rhs))
            return get(a->get() & b->get());

    return insert(new IAndInstr(lhs, rhs, makeName(name)));
}

Value* IRBuilder::createOr(Value* lhs, Value* rhs, const std::string& name)
{
    assert(lhs->type() == rhs->type());
    assert(lhs->type() == FlowType::Number);

    if (auto a = dynamic_cast<ConstantInt*>(lhs))
        if (auto b = dynamic_cast<ConstantInt*>(rhs))
            return get(a->get() | b->get());

    return insert(new IOrInstr(lhs, rhs, makeName(name)));
}

Value* IRBuilder::createXor(Value* lhs, Value* rhs, const std::string& name)
{
    assert(lhs->type() == rhs->type());
    assert(lhs->type() == FlowType::Number);

    if (auto a = dynamic_cast<ConstantInt*>(lhs))
        if (auto b = dynamic_cast<ConstantInt*>(rhs))
            return get(a->get() ^ b->get());

    return insert(new IXorInstr(lhs, rhs, makeName(name)));
}

Value* IRBuilder::createNCmpEQ(Value* lhs, Value* rhs, const std::string& name)
{
    assert(lhs->type() == rhs->type());
    assert(lhs->type() == FlowType::Number);

    if (auto a = dynamic_cast<ConstantInt*>(lhs))
        if (auto b = dynamic_cast<ConstantInt*>(rhs))
            return get(a->get() == b->get());

    return insert(new ICmpEQInstr(lhs, rhs, makeName(name)));
}

Value* IRBuilder::createNCmpNE(Value* lhs, Value* rhs, const std::string& name)
{
    assert(lhs->type() == rhs->type());
    assert(lhs->type() == FlowType::Number);

    if (auto a = dynamic_cast<ConstantInt*>(lhs))
        if (auto b = dynamic_cast<ConstantInt*>(rhs))
            return get(a->get() != b->get());

    return insert(new ICmpNEInstr(lhs, rhs, makeName(name)));
}

Value* IRBuilder::createNCmpLE(Value* lhs, Value* rhs, const std::string& name)
{
    assert(lhs->type() == rhs->type());
    assert(lhs->type() == FlowType::Number);

    if (auto a = dynamic_cast<ConstantInt*>(lhs))
        if (auto b = dynamic_cast<ConstantInt*>(rhs))
            return get(a->get() <= b->get());

    return insert(new ICmpLEInstr(lhs, rhs, makeName(name)));
}

Value* IRBuilder::createNCmpGE(Value* lhs, Value* rhs, const std::string& name)
{
    assert(lhs->type() == rhs->type());
    assert(lhs->type() == FlowType::Number);

    if (auto a = dynamic_cast<ConstantInt*>(lhs))
        if (auto b = dynamic_cast<ConstantInt*>(rhs))
            return get(a->get() >= b->get());

    return insert(new ICmpGEInstr(lhs, rhs, makeName(name)));
}

Value* IRBuilder::createNCmpLT(Value* lhs, Value* rhs, const std::string& name)
{
    assert(lhs->type() == rhs->type());
    assert(lhs->type() == FlowType::Number);

    if (auto a = dynamic_cast<ConstantInt*>(lhs))
        if (auto b = dynamic_cast<ConstantInt*>(rhs))
            return get(a->get() < b->get());

    return insert(new ICmpLTInstr(lhs, rhs, makeName(name)));
}

Value* IRBuilder::createNCmpGT(Value* lhs, Value* rhs, const std::string& name)
{
    assert(lhs->type() == rhs->type());
    assert(lhs->type() == FlowType::Number);

    if (auto a = dynamic_cast<ConstantInt*>(lhs))
        if (auto b = dynamic_cast<ConstantInt*>(rhs))
            return get(a->get() > b->get());

    return insert(new ICmpGTInstr(lhs, rhs, makeName(name)));
}
// }}}
// {{{ string ops
Value* IRBuilder::createSAdd(Value* lhs, Value* rhs, const std::string& name)
{
    assert(lhs->type() == rhs->type());
    assert(lhs->type() == FlowType::String);

    if (auto a = dynamic_cast<ConstantString*>(lhs)) {
        if (auto b = dynamic_cast<ConstantString*>(rhs)) {
            return get(a->get() + b->get());
        }

        if (a->get().empty()) {
            return rhs;
        }
    } else if (auto b = dynamic_cast<ConstantString*>(rhs)) {
        if (b->get().empty()) {
            return rhs;
        }
    }

    return insert(new SAddInstr(lhs, rhs, makeName(name)));
}

Value* IRBuilder::createSCmpEQ(Value* lhs, Value* rhs, const std::string& name)
{
    assert(lhs->type() == rhs->type());
    assert(lhs->type() == FlowType::String);

    if (auto a = dynamic_cast<ConstantString*>(lhs))
        if (auto b = dynamic_cast<ConstantString*>(rhs))
            return get(a->get() == b->get());

    return insert(new SCmpEQInstr(lhs, rhs, makeName(name)));
}

Value* IRBuilder::createSCmpNE(Value* lhs, Value* rhs, const std::string& name)
{
    assert(lhs->type() == rhs->type());
    assert(lhs->type() == FlowType::String);

    if (auto a = dynamic_cast<ConstantString*>(lhs))
        if (auto b = dynamic_cast<ConstantString*>(rhs))
            return get(a->get() != b->get());

    return insert(new SCmpNEInstr(lhs, rhs, makeName(name)));
}

Value* IRBuilder::createSCmpLE(Value* lhs, Value* rhs, const std::string& name)
{
    assert(lhs->type() == rhs->type());
    assert(lhs->type() == FlowType::String);

    if (auto a = dynamic_cast<ConstantString*>(lhs))
        if (auto b = dynamic_cast<ConstantString*>(rhs))
            return get(a->get() <= b->get());

    return insert(new SCmpLEInstr(lhs, rhs, makeName(name)));
}

Value* IRBuilder::createSCmpGE(Value* lhs, Value* rhs, const std::string& name)
{
    assert(lhs->type() == rhs->type());
    assert(lhs->type() == FlowType::String);

    if (auto a = dynamic_cast<ConstantString*>(lhs))
        if (auto b = dynamic_cast<ConstantString*>(rhs))
            return get(a->get() >= b->get());

    return insert(new SCmpGEInstr(lhs, rhs, makeName(name)));
}

Value* IRBuilder::createSCmpLT(Value* lhs, Value* rhs, const std::string& name)
{
    assert(lhs->type() == rhs->type());
    assert(lhs->type() == FlowType::String);

    if (auto a = dynamic_cast<ConstantString*>(lhs))
        if (auto b = dynamic_cast<ConstantString*>(rhs))
            return get(a->get() < b->get());

    return insert(new SCmpLTInstr(lhs, rhs, makeName(name)));
}

Value* IRBuilder::createSCmpGT(Value* lhs, Value* rhs, const std::string& name)
{
    assert(lhs->type() == rhs->type());
    assert(lhs->type() == FlowType::String);

    if (auto a = dynamic_cast<ConstantString*>(lhs))
        if (auto b = dynamic_cast<ConstantString*>(rhs))
            return get(a->get() > b->get());

    return insert(new SCmpGTInstr(lhs, rhs, makeName(name)));
}

/**
 * Compare string \p lhs against regexp \p rhs.
 */
Value* IRBuilder::createSCmpRE(Value* lhs, Value* rhs, const std::string& name)
{
    assert(lhs->type() == FlowType::String);
    assert(rhs->type() == FlowType::RegExp);

    // XXX don't perform constant folding on (string =~ regexp) as this operation yields side affects to: regex.group(I)S

    return insert(new SCmpREInstr(lhs, rhs, makeName(name)));
}

/**
 * Tests if string \p lhs begins with string \p rhs.
 *
 * @param lhs test string
 * @param rhs sub string needle
 * @param name Name of the given operations result value.
 *
 * @return boolean result.
 */
Value* IRBuilder::createSCmpEB(Value* lhs, Value* rhs, const std::string& name)
{
    if (auto a = dynamic_cast<ConstantString*>(lhs))
        if (auto b = dynamic_cast<ConstantString*>(rhs))
            return get(BufferRef(a->get()).begins(b->get()));

    return insert(new SCmpBegInstr(lhs, rhs, makeName(name)));
}

/**
 * Tests if string \p lhs ends with string \p rhs.
 *
 * @param lhs test string
 * @param rhs sub string needle
 * @param name Name of the given operations result value.
 *
 * @return boolean result.
 */
Value* IRBuilder::createSCmpEE(Value* lhs, Value* rhs, const std::string& name)
{
    if (auto a = dynamic_cast<ConstantString*>(lhs))
        if (auto b = dynamic_cast<ConstantString*>(rhs))
            return get(BufferRef(a->get()).ends(b->get()));

    return insert(new SCmpEndInstr(lhs, rhs, makeName(name)));
}
// }}}
// {{{ cast ops
Value* IRBuilder::createB2S(Value* rhs, const std::string& name)
{
    assert(rhs->type() == FlowType::Boolean);

    if (auto a = dynamic_cast<ConstantBoolean*>(rhs))
        return get(a->get() ? "true" : "false");

    return insert(new CastInstr(FlowType::String, rhs, makeName(name)));
}

Value* IRBuilder::createI2S(Value* rhs, const std::string& name)
{
    assert(rhs->type() == FlowType::Number);

    if (auto i = dynamic_cast<ConstantInt*>(rhs)) {
        char buf[64];
        snprintf(buf, sizeof(buf), "%li", i->get());
        return get(buf);
    }

    return insert(new CastInstr(FlowType::String, rhs, makeName(name)));
}

Value* IRBuilder::createP2S(Value* rhs, const std::string& name)
{
    assert(rhs->type() == FlowType::IPAddress);

    if (auto ip = dynamic_cast<ConstantIP*>(rhs))
        return get(ip->get().str());

    return insert(new CastInstr(FlowType::String, rhs, makeName(name)));
}

Value* IRBuilder::createC2S(Value* rhs, const std::string& name)
{
    assert(rhs->type() == FlowType::Cidr);

    if (auto ip = dynamic_cast<ConstantCidr*>(rhs))
        return get(ip->get().str());

    return insert(new CastInstr(FlowType::String, rhs, makeName(name)));
}

Value* IRBuilder::createR2S(Value* rhs, const std::string& name)
{
    assert(rhs->type() == FlowType::RegExp);

    if (auto ip = dynamic_cast<ConstantRegExp*>(rhs))
        return get(ip->get().pattern());

    return insert(new CastInstr(FlowType::String, rhs, makeName(name)));
}

Value* IRBuilder::createS2I(Value* rhs, const std::string& name)
{
    assert(rhs->type() == FlowType::String);

    if (auto ip = dynamic_cast<ConstantString*>(rhs))
        return get(stoi(ip->get()));

    return insert(new CastInstr(FlowType::Number, rhs, makeName(name)));
}
// }}}
// {{{ call creators
Instr* IRBuilder::createCallFunction(IRBuiltinFunction* callee, const std::vector<Value*>& args, const std::string& name)
{
    return insert(new CallInstr(callee, args, makeName(name)));
}

Instr* IRBuilder::createInvokeHandler(IRBuiltinHandler* callee, const std::vector<Value*>& args)
{
    assert(!"TODO");
    return nullptr; // TODO insert(new HandlerCallInstr(args, makeName(name)));
}
// }}}
// {{{ exit point creators
Instr* IRBuilder::createRet(Value* result, const std::string& name)
{
    return insert(new RetInstr(result, makeName(name)));
}

Instr* IRBuilder::createBr(BasicBlock* target)
{
    getInsertPoint()->link(target);

    return insert(new BrInstr({target}, ""));
}

Instr* IRBuilder::createCondBr(Value* condValue, BasicBlock* trueBlock, BasicBlock* falseBlock, const std::string& name)
{
    getInsertPoint()->link(falseBlock);
    getInsertPoint()->link(trueBlock);

    return insert(new CondBrInstr(condValue, trueBlock, falseBlock, makeName(name)));
}

MatchInstr* IRBuilder::createMatch(FlowVM::MatchClass opc, Value* cond, const std::string& name)
{
    return static_cast<MatchInstr*>(insert(new MatchInstr(opc, cond, makeName(name))));
}

Value* IRBuilder::createMatchSame(Value* cond, const std::string& name)
{
    return createMatch(MatchClass::Same, cond, name);
}

Value* IRBuilder::createMatchHead(Value* cond, const std::string& name)
{
    return createMatch(MatchClass::Head, cond, name);
}

Value* IRBuilder::createMatchTail(Value* cond, const std::string& name)
{
    return createMatch(MatchClass::Tail, cond, name);
}

Value* IRBuilder::createMatchRegExp(Value* cond, const std::string& name)
{
    return createMatch(MatchClass::RegExp, cond, makeName(name));
}
// }}}
// }}}

} // namespace x0
