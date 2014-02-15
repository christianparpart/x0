/* <IR.cpp>
 *
 * This file is part of the x0 web server project and is released under AGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2014 Christian Parpart <trapni@gmail.com>
 */

#include <x0/flow/IR.h>
#include <x0/flow/AST.h>
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
 */

namespace x0 {

#define FLOW_DEBUG_IR 1

#if defined(FLOW_DEBUG_IR)
#	define TRACE(level, msg...) XZERO_DEBUG("IR", (level), msg)
#else
#	define TRACE(level, msg...) /*!*/
#endif

// {{{ Value
Value::Value(FlowType ty, const std::string& name) :
    type_(ty),
    name_(name)
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

VmInstr::VmInstr(FlowVM::Opcode opcode, const std::vector<Value*>& ops, const std::string& name) :
    Instr(FlowVM::resultType(opcode), ops, name),
    opcode_(opcode)
{
}

void VmInstr::dump()
{
    dumpOne(mnemonic(opcode_));
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
MatchInstr::MatchInstr(FlowVM::MatchClass op, const std::string& name) :
    Instr(FlowType::Void, {}, name),
    op_(op),
    elseBlock_(nullptr)
{
    operands().push_back(nullptr); // condition placeholder
}

void MatchInstr::setCondition(Value* condition)
{
    operands()[0] = condition;
}

void MatchInstr::addCase(Value* label, BasicBlock* code)
{
    cases_.push_back(std::make_pair(label, code));
}
// }}}
// {{{ BasicBlock
BasicBlock::BasicBlock(const std::string& name) :
    Value(FlowType::Void, name),
    parent_(nullptr)
{
}

BasicBlock::~BasicBlock()
{
    for (auto instr: code_)
        delete instr;
}

void BasicBlock::dump()
{
    printf("%%%s:\n", name().c_str());
    for (size_t i = 0, e = code_.size(); i != e; ++i) {
        code_[i]->dump();
    }
    printf("\n");
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
    numbers_(),
    strings_(),
    ipaddrs_(),
    cidrs_(),
    regexps_(),
    builtinFunctions_(),
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
template IRBuiltinFunction* IRProgram::get<IRBuiltinFunction, FlowVM::Signature>(std::vector<IRBuiltinFunction*>&, const FlowVM::Signature&);
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

    return insert(new VmInstr(FlowVM::Opcode::NNEG, {rhs}, makeName(name)));
}

Value* IRBuilder::createAdd(Value* lhs, Value* rhs, const std::string& name)
{
    assert(lhs->type() == rhs->type());
    assert(lhs->type() == FlowType::Number);

    // constant folding
    if (auto a = dynamic_cast<ConstantInt*>(lhs))
        if (auto b = dynamic_cast<ConstantInt*>(rhs))
            return get(a->get() + b->get());

    return insert(new VmInstr(FlowVM::Opcode::NADD, {lhs, rhs}, makeName(name)));
}

Value* IRBuilder::createSub(Value* lhs, Value* rhs, const std::string& name)
{
    assert(lhs->type() == rhs->type());
    assert(lhs->type() == FlowType::Number);

    // constant folding
    if (auto a = dynamic_cast<ConstantInt*>(lhs))
        if (auto b = dynamic_cast<ConstantInt*>(rhs))
            return get(a->get() - b->get());

    return insert(new VmInstr(FlowVM::Opcode::NSUB, {lhs, rhs}, makeName(name)));
}

Value* IRBuilder::createMul(Value* lhs, Value* rhs, const std::string& name)
{
    assert(lhs->type() == rhs->type());
    assert(lhs->type() == FlowType::Number);

    // constant folding
    if (auto a = dynamic_cast<ConstantInt*>(lhs))
        if (auto b = dynamic_cast<ConstantInt*>(rhs))
            return get(a->get() * b->get());

    return insert(new VmInstr(FlowVM::Opcode::NMUL, {lhs, rhs}, makeName(name)));
}

Value* IRBuilder::createDiv(Value* lhs, Value* rhs, const std::string& name)
{
    assert(lhs->type() == rhs->type());
    assert(lhs->type() == FlowType::Number);

    // constant folding
    if (auto a = dynamic_cast<ConstantInt*>(lhs))
        if (auto b = dynamic_cast<ConstantInt*>(rhs))
            return get(a->get() / b->get());

    return insert(new VmInstr(FlowVM::Opcode::NMUL, {lhs, rhs}, makeName(name)));
}

Value* IRBuilder::createRem(Value* lhs, Value* rhs, const std::string& name)
{
    assert(lhs->type() == rhs->type());
    assert(lhs->type() == FlowType::Number);

    // constant folding
    if (auto a = dynamic_cast<ConstantInt*>(lhs))
        if (auto b = dynamic_cast<ConstantInt*>(rhs))
            return get(a->get() % b->get());

    return insert(new VmInstr(FlowVM::Opcode::NREM, {lhs, rhs}, makeName(name)));
}

Value* IRBuilder::createShl(Value* lhs, Value* rhs, const std::string& name)
{
    assert(lhs->type() == rhs->type());
    assert(lhs->type() == FlowType::Number);

    // constant folding
    if (auto a = dynamic_cast<ConstantInt*>(lhs))
        if (auto b = dynamic_cast<ConstantInt*>(rhs))
            return get(a->get() << b->get());

    return insert(new VmInstr(FlowVM::Opcode::NSHL, {lhs, rhs}, makeName(name)));
}

Value* IRBuilder::createShr(Value* lhs, Value* rhs, const std::string& name)
{
    assert(lhs->type() == rhs->type());
    assert(lhs->type() == FlowType::Number);

    // constant folding
    if (auto a = dynamic_cast<ConstantInt*>(lhs))
        if (auto b = dynamic_cast<ConstantInt*>(rhs))
            return get(a->get() >> b->get());

    return insert(new VmInstr(FlowVM::Opcode::NSHR, {lhs, rhs}, makeName(name)));
}

Value* IRBuilder::createPow(Value* lhs, Value* rhs, const std::string& name)
{
    assert(lhs->type() == rhs->type());
    assert(lhs->type() == FlowType::Number);

    // constant folding
    if (auto a = dynamic_cast<ConstantInt*>(lhs))
        if (auto b = dynamic_cast<ConstantInt*>(rhs))
            return get(powl(a->get(), b->get()));

    return insert(new VmInstr(FlowVM::Opcode::NPOW, {lhs, rhs}, makeName(name)));
}

Value* IRBuilder::createAnd(Value* lhs, Value* rhs, const std::string& name)
{
    assert(lhs->type() == rhs->type());
    assert(lhs->type() == FlowType::Number);

    // constant folding
    if (auto a = dynamic_cast<ConstantInt*>(lhs))
        if (auto b = dynamic_cast<ConstantInt*>(rhs))
            return get(a->get() & b->get());

    return insert(new VmInstr(FlowVM::Opcode::NAND, {lhs, rhs}, makeName(name)));
}

Value* IRBuilder::createOr(Value* lhs, Value* rhs, const std::string& name)
{
    assert(lhs->type() == rhs->type());
    assert(lhs->type() == FlowType::Number);

    // constant folding
    if (auto a = dynamic_cast<ConstantInt*>(lhs))
        if (auto b = dynamic_cast<ConstantInt*>(rhs))
            return get(a->get() | b->get());

    return insert(new VmInstr(FlowVM::Opcode::NOR, {lhs, rhs}, makeName(name)));
}

Value* IRBuilder::createXor(Value* lhs, Value* rhs, const std::string& name)
{
    assert(lhs->type() == rhs->type());
    assert(lhs->type() == FlowType::Number);

    // constant folding
    if (auto a = dynamic_cast<ConstantInt*>(lhs))
        if (auto b = dynamic_cast<ConstantInt*>(rhs))
            return get(a->get() ^ b->get());

    return insert(new VmInstr(FlowVM::Opcode::NXOR, {lhs, rhs}, makeName(name)));
}

Value* IRBuilder::createNCmpEQ(Value* lhs, Value* rhs, const std::string& name)
{
    assert(lhs->type() == rhs->type());
    assert(lhs->type() == FlowType::Number);

    // constant folding
    if (auto a = dynamic_cast<ConstantInt*>(lhs))
        if (auto b = dynamic_cast<ConstantInt*>(rhs))
            return get(a->get() == b->get());

    return insert(new VmInstr(FlowVM::Opcode::NCMPEQ, {lhs, rhs}, makeName(name)));
}

Value* IRBuilder::createNCmpNE(Value* lhs, Value* rhs, const std::string& name)
{
    assert(lhs->type() == rhs->type());
    assert(lhs->type() == FlowType::Number);

    // constant folding
    if (auto a = dynamic_cast<ConstantInt*>(lhs))
        if (auto b = dynamic_cast<ConstantInt*>(rhs))
            return get(a->get() != b->get());

    return insert(new VmInstr(FlowVM::Opcode::NCMPNE, {lhs, rhs}, makeName(name)));
}

Value* IRBuilder::createNCmpLE(Value* lhs, Value* rhs, const std::string& name)
{
    assert(lhs->type() == rhs->type());
    assert(lhs->type() == FlowType::Number);

    // constant folding
    if (auto a = dynamic_cast<ConstantInt*>(lhs))
        if (auto b = dynamic_cast<ConstantInt*>(rhs))
            return get(a->get() <= b->get());

    return insert(new VmInstr(FlowVM::Opcode::NCMPLE, {lhs, rhs}, makeName(name)));
}

Value* IRBuilder::createNCmpGE(Value* lhs, Value* rhs, const std::string& name)
{
    assert(lhs->type() == rhs->type());
    assert(lhs->type() == FlowType::Number);

    // constant folding
    if (auto a = dynamic_cast<ConstantInt*>(lhs))
        if (auto b = dynamic_cast<ConstantInt*>(rhs))
            return get(a->get() >= b->get());

    return insert(new VmInstr(FlowVM::Opcode::NCMPGE, {lhs, rhs}, makeName(name)));
}

Value* IRBuilder::createNCmpLT(Value* lhs, Value* rhs, const std::string& name)
{
    assert(lhs->type() == rhs->type());
    assert(lhs->type() == FlowType::Number);

    // constant folding
    if (auto a = dynamic_cast<ConstantInt*>(lhs))
        if (auto b = dynamic_cast<ConstantInt*>(rhs))
            return get(a->get() < b->get());

    return insert(new VmInstr(FlowVM::Opcode::NCMPLT, {lhs, rhs}, makeName(name)));
}

Value* IRBuilder::createNCmpGT(Value* lhs, Value* rhs, const std::string& name)
{
    assert(lhs->type() == rhs->type());
    assert(lhs->type() == FlowType::Number);

    // constant folding
    if (auto a = dynamic_cast<ConstantInt*>(lhs))
        if (auto b = dynamic_cast<ConstantInt*>(rhs))
            return get(a->get() > b->get());

    return insert(new VmInstr(FlowVM::Opcode::NCMPGT, {lhs, rhs}, makeName(name)));
}
// }}}
// {{{ string ops
Value* IRBuilder::createSAdd(Value* lhs, Value* rhs, const std::string& name)
{
    assert(lhs->type() == rhs->type());
    assert(lhs->type() == FlowType::String);

    // constant folding
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

    return insert(new VmInstr(FlowVM::Opcode::SADD, {lhs, rhs}, makeName(name)));
}

Value* IRBuilder::createSCmpEQ(Value* lhs, Value* rhs, const std::string& name)
{
    assert(lhs->type() == rhs->type());
    assert(lhs->type() == FlowType::String);

    // constant folding
    if (auto a = dynamic_cast<ConstantString*>(lhs))
        if (auto b = dynamic_cast<ConstantString*>(rhs))
            return get(a->get() == b->get());

    return insert(new VmInstr(FlowVM::Opcode::SCMPEQ, {lhs, rhs}, makeName(name)));
}

Value* IRBuilder::createSCmpNE(Value* lhs, Value* rhs, const std::string& name)
{
    assert(lhs->type() == rhs->type());
    assert(lhs->type() == FlowType::String);

    // constant folding
    if (auto a = dynamic_cast<ConstantString*>(lhs))
        if (auto b = dynamic_cast<ConstantString*>(rhs))
            return get(a->get() != b->get());

    return insert(new VmInstr(FlowVM::Opcode::SCMPNE, {lhs, rhs}, makeName(name)));
}

Value* IRBuilder::createSCmpLE(Value* lhs, Value* rhs, const std::string& name)
{
    assert(lhs->type() == rhs->type());
    assert(lhs->type() == FlowType::String);

    // constant folding
    if (auto a = dynamic_cast<ConstantString*>(lhs))
        if (auto b = dynamic_cast<ConstantString*>(rhs))
            return get(a->get() <= b->get());

    return insert(new VmInstr(FlowVM::Opcode::SCMPLE, {lhs, rhs}, makeName(name)));
}

Value* IRBuilder::createSCmpGE(Value* lhs, Value* rhs, const std::string& name)
{
    assert(lhs->type() == rhs->type());
    assert(lhs->type() == FlowType::String);

    // constant folding
    if (auto a = dynamic_cast<ConstantString*>(lhs))
        if (auto b = dynamic_cast<ConstantString*>(rhs))
            return get(a->get() >= b->get());

    return insert(new VmInstr(FlowVM::Opcode::SCMPGE, {lhs, rhs}, makeName(name)));
}

Value* IRBuilder::createSCmpLT(Value* lhs, Value* rhs, const std::string& name)
{
    assert(lhs->type() == rhs->type());
    assert(lhs->type() == FlowType::String);

    // constant folding
    if (auto a = dynamic_cast<ConstantString*>(lhs))
        if (auto b = dynamic_cast<ConstantString*>(rhs))
            return get(a->get() < b->get());

    return insert(new VmInstr(FlowVM::Opcode::SCMPLT, {lhs, rhs}, makeName(name)));
}

Value* IRBuilder::createSCmpGT(Value* lhs, Value* rhs, const std::string& name)
{
    assert(lhs->type() == rhs->type());
    assert(lhs->type() == FlowType::String);

    // constant folding
    if (auto a = dynamic_cast<ConstantString*>(lhs))
        if (auto b = dynamic_cast<ConstantString*>(rhs))
            return get(a->get() > b->get());

    return insert(new VmInstr(FlowVM::Opcode::SCMPGT, {lhs, rhs}, makeName(name)));
}

/**
 * Compare string \p lhs against regexp \p rhs.
 */
Value* IRBuilder::createSCmpRE(Value* lhs, Value* rhs, const std::string& name)
{
    assert(lhs->type() == FlowType::String);
    assert(rhs->type() == FlowType::RegExp);

    // XXX don't perform constant folding on (string =~ regexp) as this operation yields side affects to: regex.group(I)S

    return insert(new VmInstr(FlowVM::Opcode::SREGMATCH, {lhs, rhs}, makeName(name)));
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
    // constant folding
    if (auto a = dynamic_cast<ConstantString*>(lhs))
        if (auto b = dynamic_cast<ConstantString*>(rhs))
            return get(BufferRef(a->get()).begins(b->get()));

    return insert(new VmInstr(FlowVM::Opcode::SCMPBEG, {lhs, rhs}, makeName(name)));
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
    // constant folding
    if (auto a = dynamic_cast<ConstantString*>(lhs))
        if (auto b = dynamic_cast<ConstantString*>(rhs))
            return get(BufferRef(a->get()).ends(b->get()));

    return insert(new VmInstr(FlowVM::Opcode::SCMPEND, {lhs, rhs}, makeName(name)));
}
// }}}
// {{{ cast ops
/**
 * Converts given value to target type.
 *
 * @param ty target type to convert to
 * @param rhs source value to convert from
 *
 * @return \c nullptr if invalid conversion, a converted value otherwise.
 */
Value* IRBuilder::createConvert(FlowType ty, Value* rhs, const std::string& name)
{
    using FlowVM::Opcode;

    assert(rhs != nullptr);
    assert(ty == FlowType::Number
        || ty == FlowType::Boolean
        || ty == FlowType::String
        || ty == FlowType::IPAddress
        || ty == FlowType::Cidr
        || ty == FlowType::RegExp
        );

    static const std::unordered_map<FlowType, std::unordered_map<FlowType, Opcode>> ops = {
        {FlowType::Number, {
            {FlowType::Number, Opcode::NOP},
            {FlowType::Boolean, Opcode::NCMPZ},
            {FlowType::String, Opcode::I2S},
        }},
        {FlowType::Boolean, {
            {FlowType::Boolean, Opcode::NOP},
            {FlowType::Number, Opcode::NOP},
            {FlowType::String, Opcode::I2S},
        }},
        {FlowType::String, {
            {FlowType::String, Opcode::NOP},
            {FlowType::Number, Opcode::S2I},
        }},
        {FlowType::IPAddress, {
            {FlowType::IPAddress, Opcode::NOP},
            {FlowType::String, Opcode::P2S},
        }},
        {FlowType::Cidr, {
            {FlowType::Cidr, Opcode::NOP},
            {FlowType::String, Opcode::C2S},
        }},
        {FlowType::RegExp, {
            {FlowType::RegExp, Opcode::NOP},
            {FlowType::String, Opcode::R2S},
        }},
    };

    if (ty == rhs->type())
        return rhs;

    // source type
    auto a = ops.find(rhs->type());
    if (a == ops.end())
        return nullptr;

    // target type
    auto b = a->second.find(ty);
    if (b == a->second.end())
        return nullptr;

    auto op = b->second;

    // constant folding
    if (dynamic_cast<Constant*>(rhs)) {
        switch (op) {
            case Opcode::NCMPZ: // int -> bool
                return get(static_cast<ConstantInt*>(rhs)->get() != 0);
            case Opcode::I2S:   // int,bool -> str
                if (auto a = dynamic_cast<ConstantBoolean*>(rhs)) {
                    return get(a->get() ? "true" : "false");
                } else {
                    auto b = static_cast<ConstantInt*>(rhs)->get();
                    char buf[64];
                    snprintf(buf, sizeof(buf), "%li", b);
                    return get(buf);
                }
            case Opcode::S2I:   // str -> bool
                return get(static_cast<ConstantString*>(rhs)->get().size() != 0);
            case Opcode::P2S:   // ip -> str
                return get(static_cast<ConstantIP*>(rhs)->get().str());
            case Opcode::C2S:   // cidr -> str
                return get(static_cast<ConstantCidr*>(rhs)->get().str());
            case Opcode::R2S:   // regex -> str
                return get(static_cast<ConstantRegExp*>(rhs)->get().pattern());
            case Opcode::NOP:
                // no need to cast into the same type
                return rhs;
            default:
                XZERO_DEBUG("IR", 1, "Unexpected cast op: %i", op);
                break;
        }
    }

    return insert(new VmInstr(op, {rhs}, makeName(name)));
}

Value* IRBuilder::createB2S(Value* rhs, const std::string& name)
{
    assert(rhs->type() == FlowType::Boolean);

    if (auto a = dynamic_cast<ConstantBoolean*>(rhs)) {
        return get(a->get() ? "true" : "false");
    }

    return insert(new VmInstr(FlowVM::Opcode::I2S, {rhs}, makeName(name)));
}

Value* IRBuilder::createI2S(Value* rhs, const std::string& name)
{
    assert(rhs->type() == FlowType::Number);

    if (auto i = dynamic_cast<ConstantInt*>(rhs)) {
        char buf[64];
        snprintf(buf, sizeof(buf), "%li", i->get());
        return get(buf);
    }

    return insert(new VmInstr(FlowVM::Opcode::I2S, {rhs}, makeName(name)));
}

Value* IRBuilder::createP2S(Value* rhs, const std::string& name)
{
    assert(rhs->type() == FlowType::IPAddress);

    if (auto ip = dynamic_cast<ConstantIP*>(rhs)) {
        return get(ip->get().str());
    }

    return insert(new VmInstr(FlowVM::Opcode::P2S, {rhs}, makeName(name)));
}

Value* IRBuilder::createC2S(Value* rhs, const std::string& name)
{
    assert(rhs->type() == FlowType::Cidr);

    if (auto ip = dynamic_cast<ConstantCidr*>(rhs)) {
        return get(ip->get().str());
    }

    return createConvert(FlowType::String, rhs, makeName(name));
}

Value* IRBuilder::createR2S(Value* rhs, const std::string& name)
{
    assert(rhs->type() == FlowType::RegExp);

    if (auto ip = dynamic_cast<ConstantRegExp*>(rhs)) {
        return get(ip->get().pattern());
    }

    return createConvert(FlowType::String, rhs, makeName(name));
}

Value* IRBuilder::createS2I(Value* rhs, const std::string& name)
{
    assert(rhs->type() == FlowType::String);

    if (auto ip = dynamic_cast<ConstantString*>(rhs)) {
        return get(stoi(ip->get()));
    }

    return createConvert(FlowType::String, rhs, makeName(name));
}
// }}}
// {{{ call creators
Instr* IRBuilder::createCallFunction(IRBuiltinFunction* callee, const std::vector<Value*>& args, const std::string& name)
{
    return insert(new CallInstr(callee, args, makeName(name)));
}

Instr* IRBuilder::createInvokeHandler(const std::vector<Value*>& args, const std::string& name)
{
    return insert(new VmInstr(FlowVM::Opcode::HANDLER, args, makeName(name)));
}
// }}}
// {{{ exit point creators
Instr* IRBuilder::createRet(Value* result, const std::string& name)
{
    return insert(new RetInstr(result, makeName(name)));
}

Instr* IRBuilder::createBr(BasicBlock* target)
{
    return insert(new BrInstr({target}, ""));
}

Instr* IRBuilder::createCondBr(Value* condValue, BasicBlock* trueBlock, BasicBlock* falseBlock, const std::string& name)
{
    return insert(new CondBrInstr(condValue, trueBlock, falseBlock, makeName(name)));
}

Value* IRBuilder::createMatchSame(Value* cond, size_t matchId, const std::string& name)
{
    return insert(new VmInstr(FlowVM::Opcode::SMATCHEQ, {get(matchId)}, makeName(name)));
}

Value* IRBuilder::createMatchHead(Value* cond, size_t matchId, const std::string& name)
{
    return insert(new VmInstr(FlowVM::Opcode::SMATCHBEG, {get(matchId)}, makeName(name)));
}

Value* IRBuilder::createMatchTail(Value* cond, size_t matchId, const std::string& name)
{
    return insert(new VmInstr(FlowVM::Opcode::SMATCHEND, {get(matchId)}, makeName(name)));
}

Value* IRBuilder::createMatchRegExp(Value* cond, size_t matchId, const std::string& name)
{
    return insert(new VmInstr(FlowVM::Opcode::SMATCHR, {get(matchId)}, makeName(name)));
}
// }}}
// }}}

} // namespace x0
