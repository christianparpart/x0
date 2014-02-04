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

// {{{ Value
Value::Value(FlowType ty, const std::string& name) :
    type_(ty),
    name_(name)
{
}

Value::~Value()
{
}

void Value::dump()
{
    printf("Value '%s': %s\n", name_.c_str(), tos(type_).c_str());
}
// }}}
// {{{ Constant
void Constant::dump()
{
    printf("Constant %zu '%s': %s\n", id_, name().c_str(), tos(type()).c_str());
}
// }}}
// {{{ Instr
Instr::Instr(BasicBlock* parent, FlowType ty, const std::vector<Value*>& ops, const std::string& name) :
    Value(ty, name),
    parent_(parent),
    operands_(ops)
{
}

Instr::~Instr() {
}

VmInstr::VmInstr(BasicBlock* parent, FlowVM::Opcode opcode, const std::vector<Value*>& ops, const std::string& name) :
    Instr(parent, FlowVM::resultType(opcode), ops, name),
    opcode_(opcode)
{
}

void VmInstr::dump()
{
    printf("VmInstr %s ; %s", mnemonic(opcode_), name().c_str());

    for (size_t i = 0, e = operands().size(); i != e; ++i) {
        printf("\t operand #%zu: ", i);
        operands()[i]->dump();
    }
}

PhiNode::PhiNode(BasicBlock* parent, const std::vector<Value*>& ops, const std::string& name) :
    Instr(parent, ops[0]->type(), ops, name)
{
}

void PhiNode::dump()
{
    printf("%s = phi\n", name().c_str());

    for (size_t i = 0, e = operands().size(); i != e; ++i) {
        printf("\t operand #%zu: ", i);
        operands()[i]->dump();
    }
}

void CondBrInstr::dump()
{
    printf("condbr %s, %s, %s ; %s\n",
        operands()[0]->name().c_str(),
        operands()[1]->name().c_str(),
        operands()[2]->name().c_str(),
        name().c_str()
    );
}

void BrInstr::dump()
{
    printf("br %s ; %s\n", operands()[0]->name().c_str(), name().c_str());
}

// }}}
// {{{ MatchInstr
MatchInstr::MatchInstr(BasicBlock* parent, FlowVM::MatchClass op, const std::string& name) :
    Instr(parent, FlowType::Void, {}, name),
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
BasicBlock::BasicBlock(IRHandler* parent, const std::string& name) :
    Value(FlowType::Void, name),
    parent_(parent)
{
}

BasicBlock::~BasicBlock()
{
    for (auto instr: code_)
        delete instr;
}
// }}}
// {{{ IRHandler
IRHandler::IRHandler(IRProgram* parent, size_t id, const std::string& name) :
    Constant(FlowType::Handler, id, name),
    parent_(parent),
    entryPoint_(nullptr),
    blocks_()
{
}

IRHandler::~IRHandler()
{
}

void IRHandler::setEntryPoint(BasicBlock* bb)
{
    assert(bb->parent() == this);
    assert(entryPoint_ == nullptr && "QA: changing EP not allowed.");

    entryPoint_ = bb;
}
// }}}
// {{{ IRProgram
IRProgram::IRProgram() :
    numbers_(),
    strings_(),
    ipaddrs_(),
    cidrs_(),
    regexps_(),
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
// }}}
// {{{ IRBuilder
IRBuilder::IRBuilder() :
    program_(nullptr),
    handler_(nullptr),
    insertPoint_(nullptr)
{
}

IRBuilder::~IRBuilder()
{
}

// {{{ context management
void IRBuilder::setProgram(IRProgram* prog)
{
    program_ = prog;
    handler_ = nullptr;
    insertPoint_ = nullptr;
}

void IRBuilder::setHandler(IRHandler* hn)
{
    assert(hn->parent() == program_);

    handler_ = hn;
    insertPoint_ = nullptr;
}

BasicBlock* IRBuilder::createBlock(const std::string& name)
{
    return new BasicBlock(handler_, name);
}

void IRBuilder::setInsertPoint(BasicBlock* bb)
{
    assert(bb != nullptr);
    assert(bb->parent() == handler() && "insert point must belong to the current handler.");

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
    IRHandler* h = new IRHandler(program_, id, name);
    program_->handlers_.push_back(h);

    return h;
}
// }}}
// {{{ value management
/**
 * dynamically allocates an array of given element type and size.
 */
Instr* IRBuilder::createAlloca(FlowType ty, Value* arraySize, const std::string& name)
{
    return insert(new AllocaInstr(getInsertPoint(), ty, arraySize, name));
}

/**
 * initializes an array at given index.
 */
Instr* IRBuilder::createArraySet(Value* array, Value* index, Value* value, const std::string& name)
{
    return insert(new ArraySetInstr(getInsertPoint(), array, index, value, name));
}

/**
 * Loads given value
 */
Value* IRBuilder::createLoad(Value* value, const std::string& name)
{
    if (dynamic_cast<Constant*>(value))
        return value;

    if (dynamic_cast<Variable*>(value))
        return insert(new LoadInstr(getInsertPoint(), value, name));

    assert(!"Value must be of type Constant or Variable.");
    return nullptr;
}

/**
 * emits a STORE of value \p rhs to variable \p lhs.
 */
Instr* IRBuilder::createStore(Value* lhs, Value* rhs, const std::string& name)
{
    assert(lhs->type() == rhs->type() && "Type of lhs and rhs must be equal.");
    assert(dynamic_cast<Variable*>(lhs) && "lhs must be of type Variable.");

    return insert(new StoreInstr(getInsertPoint(), lhs, rhs, name));
}

Instr* IRBuilder::createPhi(const std::vector<Value*>& incomings, const std::string& name)
{
    return insert(new PhiNode(getInsertPoint(), incomings, name));
}
// }}}
// {{{ numerical ops
Value* IRBuilder::createNeg(Value* rhs, const std::string& name)
{
    assert(rhs->type() == FlowType::Number);

    if (auto a = dynamic_cast<ConstantInt*>(rhs))
        return get(-a->get());

    return insert(new VmInstr(getInsertPoint(), FlowVM::Opcode::NNEG, {rhs}, name));
}

Value* IRBuilder::createAdd(Value* lhs, Value* rhs, const std::string& name)
{
    assert(lhs->type() == rhs->type());
    assert(lhs->type() == FlowType::Number);

    // constant folding
    if (auto a = dynamic_cast<ConstantInt*>(lhs))
        if (auto b = dynamic_cast<ConstantInt*>(rhs))
            return get(a->get() + b->get());

    return insert(new VmInstr(getInsertPoint(), FlowVM::Opcode::NADD, {lhs, rhs}, name));
}

Value* IRBuilder::createSub(Value* lhs, Value* rhs, const std::string& name)
{
    assert(lhs->type() == rhs->type());
    assert(lhs->type() == FlowType::Number);

    // constant folding
    if (auto a = dynamic_cast<ConstantInt*>(lhs))
        if (auto b = dynamic_cast<ConstantInt*>(rhs))
            return get(a->get() - b->get());

    return insert(new VmInstr(getInsertPoint(), FlowVM::Opcode::NSUB, {lhs, rhs}, name));
}

Value* IRBuilder::createMul(Value* lhs, Value* rhs, const std::string& name)
{
    assert(lhs->type() == rhs->type());
    assert(lhs->type() == FlowType::Number);

    // constant folding
    if (auto a = dynamic_cast<ConstantInt*>(lhs))
        if (auto b = dynamic_cast<ConstantInt*>(rhs))
            return get(a->get() * b->get());

    return insert(new VmInstr(getInsertPoint(), FlowVM::Opcode::NMUL, {lhs, rhs}, name));
}

Value* IRBuilder::createDiv(Value* lhs, Value* rhs, const std::string& name)
{
    assert(lhs->type() == rhs->type());
    assert(lhs->type() == FlowType::Number);

    // constant folding
    if (auto a = dynamic_cast<ConstantInt*>(lhs))
        if (auto b = dynamic_cast<ConstantInt*>(rhs))
            return get(a->get() / b->get());

    return insert(new VmInstr(getInsertPoint(), FlowVM::Opcode::NMUL, {lhs, rhs}, name));
}

Value* IRBuilder::createRem(Value* lhs, Value* rhs, const std::string& name)
{
    assert(lhs->type() == rhs->type());
    assert(lhs->type() == FlowType::Number);

    // constant folding
    if (auto a = dynamic_cast<ConstantInt*>(lhs))
        if (auto b = dynamic_cast<ConstantInt*>(rhs))
            return get(a->get() % b->get());

    return insert(new VmInstr(getInsertPoint(), FlowVM::Opcode::NREM, {lhs, rhs}, name));
}

Value* IRBuilder::createShl(Value* lhs, Value* rhs, const std::string& name)
{
    assert(lhs->type() == rhs->type());
    assert(lhs->type() == FlowType::Number);

    // constant folding
    if (auto a = dynamic_cast<ConstantInt*>(lhs))
        if (auto b = dynamic_cast<ConstantInt*>(rhs))
            return get(a->get() << b->get());

    return insert(new VmInstr(getInsertPoint(), FlowVM::Opcode::NSHL, {lhs, rhs}, name));
}

Value* IRBuilder::createShr(Value* lhs, Value* rhs, const std::string& name)
{
    assert(lhs->type() == rhs->type());
    assert(lhs->type() == FlowType::Number);

    // constant folding
    if (auto a = dynamic_cast<ConstantInt*>(lhs))
        if (auto b = dynamic_cast<ConstantInt*>(rhs))
            return get(a->get() >> b->get());

    return insert(new VmInstr(getInsertPoint(), FlowVM::Opcode::NSHR, {lhs, rhs}, name));
}

Value* IRBuilder::createPow(Value* lhs, Value* rhs, const std::string& name)
{
    assert(lhs->type() == rhs->type());
    assert(lhs->type() == FlowType::Number);

    // constant folding
    if (auto a = dynamic_cast<ConstantInt*>(lhs))
        if (auto b = dynamic_cast<ConstantInt*>(rhs))
            return get(powl(a->get(), b->get()));

    return insert(new VmInstr(getInsertPoint(), FlowVM::Opcode::NPOW, {lhs, rhs}, name));
}

Value* IRBuilder::createAnd(Value* lhs, Value* rhs, const std::string& name)
{
    assert(lhs->type() == rhs->type());
    assert(lhs->type() == FlowType::Number);

    // constant folding
    if (auto a = dynamic_cast<ConstantInt*>(lhs))
        if (auto b = dynamic_cast<ConstantInt*>(rhs))
            return get(a->get() & b->get());

    return insert(new VmInstr(getInsertPoint(), FlowVM::Opcode::NAND, {lhs, rhs}, name));
}

Value* IRBuilder::createOr(Value* lhs, Value* rhs, const std::string& name)
{
    assert(lhs->type() == rhs->type());
    assert(lhs->type() == FlowType::Number);

    // constant folding
    if (auto a = dynamic_cast<ConstantInt*>(lhs))
        if (auto b = dynamic_cast<ConstantInt*>(rhs))
            return get(a->get() | b->get());

    return insert(new VmInstr(getInsertPoint(), FlowVM::Opcode::NOR, {lhs, rhs}, name));
}

Value* IRBuilder::createXor(Value* lhs, Value* rhs, const std::string& name)
{
    assert(lhs->type() == rhs->type());
    assert(lhs->type() == FlowType::Number);

    // constant folding
    if (auto a = dynamic_cast<ConstantInt*>(lhs))
        if (auto b = dynamic_cast<ConstantInt*>(rhs))
            return get(a->get() ^ b->get());

    return insert(new VmInstr(getInsertPoint(), FlowVM::Opcode::NXOR, {lhs, rhs}, name));
}

Value* IRBuilder::createNCmpEQ(Value* lhs, Value* rhs, const std::string& name)
{
    assert(lhs->type() == rhs->type());
    assert(lhs->type() == FlowType::Number);

    // constant folding
    if (auto a = dynamic_cast<ConstantInt*>(lhs))
        if (auto b = dynamic_cast<ConstantInt*>(rhs))
            return get(a->get() == b->get());

    return insert(new VmInstr(getInsertPoint(), FlowVM::Opcode::NCMPEQ, {lhs, rhs}, name));
}

Value* IRBuilder::createNCmpNE(Value* lhs, Value* rhs, const std::string& name)
{
    assert(lhs->type() == rhs->type());
    assert(lhs->type() == FlowType::Number);

    // constant folding
    if (auto a = dynamic_cast<ConstantInt*>(lhs))
        if (auto b = dynamic_cast<ConstantInt*>(rhs))
            return get(a->get() != b->get());

    return insert(new VmInstr(getInsertPoint(), FlowVM::Opcode::NCMPNE, {lhs, rhs}, name));
}

Value* IRBuilder::createNCmpLE(Value* lhs, Value* rhs, const std::string& name)
{
    assert(lhs->type() == rhs->type());
    assert(lhs->type() == FlowType::Number);

    // constant folding
    if (auto a = dynamic_cast<ConstantInt*>(lhs))
        if (auto b = dynamic_cast<ConstantInt*>(rhs))
            return get(a->get() <= b->get());

    return insert(new VmInstr(getInsertPoint(), FlowVM::Opcode::NCMPLE, {lhs, rhs}, name));
}

Value* IRBuilder::createNCmpGE(Value* lhs, Value* rhs, const std::string& name)
{
    assert(lhs->type() == rhs->type());
    assert(lhs->type() == FlowType::Number);

    // constant folding
    if (auto a = dynamic_cast<ConstantInt*>(lhs))
        if (auto b = dynamic_cast<ConstantInt*>(rhs))
            return get(a->get() >= b->get());

    return insert(new VmInstr(getInsertPoint(), FlowVM::Opcode::NCMPGE, {lhs, rhs}, name));
}

Value* IRBuilder::createNCmpLT(Value* lhs, Value* rhs, const std::string& name)
{
    assert(lhs->type() == rhs->type());
    assert(lhs->type() == FlowType::Number);

    // constant folding
    if (auto a = dynamic_cast<ConstantInt*>(lhs))
        if (auto b = dynamic_cast<ConstantInt*>(rhs))
            return get(a->get() < b->get());

    return insert(new VmInstr(getInsertPoint(), FlowVM::Opcode::NCMPLT, {lhs, rhs}, name));
}

Value* IRBuilder::createNCmpGT(Value* lhs, Value* rhs, const std::string& name)
{
    assert(lhs->type() == rhs->type());
    assert(lhs->type() == FlowType::Number);

    // constant folding
    if (auto a = dynamic_cast<ConstantInt*>(lhs))
        if (auto b = dynamic_cast<ConstantInt*>(rhs))
            return get(a->get() > b->get());

    return insert(new VmInstr(getInsertPoint(), FlowVM::Opcode::NCMPGT, {lhs, rhs}, name));
}
// }}}
// {{{ string ops
Value* IRBuilder::createSAdd(Value* lhs, Value* rhs, const std::string& name)
{
    assert(lhs->type() == rhs->type());
    assert(lhs->type() == FlowType::String);

    // constant folding
    if (auto a = dynamic_cast<ConstantString*>(lhs))
        if (auto b = dynamic_cast<ConstantString*>(rhs))
            return get(a->get() + b->get());

    return insert(new VmInstr(getInsertPoint(), FlowVM::Opcode::SADD, {lhs, rhs}, name));
}

Value* IRBuilder::createSCmpEQ(Value* lhs, Value* rhs, const std::string& name)
{
    assert(lhs->type() == rhs->type());
    assert(lhs->type() == FlowType::String);

    // constant folding
    if (auto a = dynamic_cast<ConstantString*>(lhs))
        if (auto b = dynamic_cast<ConstantString*>(rhs))
            return get(a->get() == b->get());

    return insert(new VmInstr(getInsertPoint(), FlowVM::Opcode::SCMPEQ, {lhs, rhs}, name));
}

Value* IRBuilder::createSCmpNE(Value* lhs, Value* rhs, const std::string& name)
{
    assert(lhs->type() == rhs->type());
    assert(lhs->type() == FlowType::String);

    // constant folding
    if (auto a = dynamic_cast<ConstantString*>(lhs))
        if (auto b = dynamic_cast<ConstantString*>(rhs))
            return get(a->get() != b->get());

    return insert(new VmInstr(getInsertPoint(), FlowVM::Opcode::SCMPNE, {lhs, rhs}, name));
}

Value* IRBuilder::createSCmpLE(Value* lhs, Value* rhs, const std::string& name)
{
    assert(lhs->type() == rhs->type());
    assert(lhs->type() == FlowType::String);

    // constant folding
    if (auto a = dynamic_cast<ConstantString*>(lhs))
        if (auto b = dynamic_cast<ConstantString*>(rhs))
            return get(a->get() <= b->get());

    return insert(new VmInstr(getInsertPoint(), FlowVM::Opcode::SCMPLE, {lhs, rhs}, name));
}

Value* IRBuilder::createSCmpGE(Value* lhs, Value* rhs, const std::string& name)
{
    assert(lhs->type() == rhs->type());
    assert(lhs->type() == FlowType::String);

    // constant folding
    if (auto a = dynamic_cast<ConstantString*>(lhs))
        if (auto b = dynamic_cast<ConstantString*>(rhs))
            return get(a->get() >= b->get());

    return insert(new VmInstr(getInsertPoint(), FlowVM::Opcode::SCMPGE, {lhs, rhs}, name));
}

Value* IRBuilder::createSCmpLT(Value* lhs, Value* rhs, const std::string& name)
{
    assert(lhs->type() == rhs->type());
    assert(lhs->type() == FlowType::String);

    // constant folding
    if (auto a = dynamic_cast<ConstantString*>(lhs))
        if (auto b = dynamic_cast<ConstantString*>(rhs))
            return get(a->get() < b->get());

    return insert(new VmInstr(getInsertPoint(), FlowVM::Opcode::SCMPLT, {lhs, rhs}, name));
}

Value* IRBuilder::createSCmpGT(Value* lhs, Value* rhs, const std::string& name)
{
    assert(lhs->type() == rhs->type());
    assert(lhs->type() == FlowType::String);

    // constant folding
    if (auto a = dynamic_cast<ConstantString*>(lhs))
        if (auto b = dynamic_cast<ConstantString*>(rhs))
            return get(a->get() > b->get());

    return insert(new VmInstr(getInsertPoint(), FlowVM::Opcode::SCMPGT, {lhs, rhs}, name));
}

/**
 * Compare string \p lhs against regexp \p rhs.
 */
Value* IRBuilder::createSCmpRE(Value* lhs, Value* rhs, const std::string& name)
{
    assert(lhs->type() == FlowType::String);
    assert(rhs->type() == FlowType::RegExp);

    // XXX don't perform constant folding on (string =~ regexp) as this operation yields side affects to: regex.group(I)S

    return insert(new VmInstr(getInsertPoint(), FlowVM::Opcode::SREGMATCH, {lhs, rhs}, name));
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

    return insert(new VmInstr(getInsertPoint(), FlowVM::Opcode::SCMPBEG, {lhs, rhs}, name));
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

    return insert(new VmInstr(getInsertPoint(), FlowVM::Opcode::SCMPEND, {lhs, rhs}, name));
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

    return insert(new VmInstr(getInsertPoint(), op, {rhs}, name));
}
// }}}
// {{{ call creators
Instr* IRBuilder::createCallFunction(Value* function, const std::vector<Value*>& args, const std::string& name)
{
    return insert(new VmInstr(getInsertPoint(), FlowVM::Opcode::CALL, args, name));
}

Instr* IRBuilder::createInvokeHandler(Value* handler, const std::vector<Value*>& args, const std::string& name)
{
    return insert(new VmInstr(getInsertPoint(), FlowVM::Opcode::HANDLER, args, name));
}
// }}}
// {{{ exit point creators
Instr* IRBuilder::createRet(Value* result, const std::string& name)
{
    return insert(new RetInstr(getInsertPoint(), name));
}

Instr* IRBuilder::createBr(BasicBlock* target)
{
    return insert(new BrInstr(getInsertPoint(), {target}, ""));
}

Instr* IRBuilder::createCondBr(Value* condValue, BasicBlock* trueBlock, BasicBlock* falseBlock, const std::string& name)
{
    return insert(new CondBrInstr(getInsertPoint(), condValue, trueBlock, falseBlock, name));
}

Value* IRBuilder::createMatchSame(Value* cond, size_t matchId, const std::string& name)
{
    return insert(new VmInstr(getInsertPoint(), FlowVM::Opcode::SMATCHEQ, {get(matchId)}, name));
}

Value* IRBuilder::createMatchHead(Value* cond, size_t matchId, const std::string& name)
{
    return insert(new VmInstr(getInsertPoint(), FlowVM::Opcode::SMATCHBEG, {get(matchId)}, name));
}

Value* IRBuilder::createMatchTail(Value* cond, size_t matchId, const std::string& name)
{
    return insert(new VmInstr(getInsertPoint(), FlowVM::Opcode::SMATCHEND, {get(matchId)}, name));
}

Value* IRBuilder::createMatchRegExp(Value* cond, size_t matchId, const std::string& name)
{
    return insert(new VmInstr(getInsertPoint(), FlowVM::Opcode::SMATCHR, {get(matchId)}, name));
}
// }}}
// }}}
// {{{ IRGenerator
IRGenerator::IRGenerator() :
    IRBuilder(),
    ASTVisitor()
{
}

IRGenerator::~IRGenerator()
{
}

IRProgram* IRGenerator::generate(Unit* unit)
{
    IRGenerator ir;
    ir.generate(unit);

    return ir.program();
}

Value* IRGenerator::generate(Expr* expr)
{
    expr->visit(*this);
    return result_;
}

Value* IRGenerator::generate(Stmt* stmt)
{
    stmt->visit(*this);
    return result_;
}

Value* IRGenerator::generate(Symbol* sym)
{
    sym->visit(*this);
    return result_;
}

void IRGenerator::accept(Unit& unit)
{
    setProgram(new IRProgram());

    for (const auto sym: *unit.scope()) {
        generate(sym);
    }
}

void IRGenerator::accept(Variable& variable)
{
    // TODO declare given variable in current handler
}

void IRGenerator::accept(Handler& handler)
{
    setHandler(getHandler(handler.name()));
    setInsertPoint(createBlock("EntryPoint"));
}

void IRGenerator::accept(BuiltinFunction& symbol)
{
}

void IRGenerator::accept(BuiltinHandler& symbol)
{
}

void IRGenerator::accept(UnaryExpr& expr)
{
    Value* rhs = generate(expr.subExpr());
    result_ = insert(new VmInstr(getInsertPoint(), expr.op(), {rhs}));
}

void IRGenerator::accept(BinaryExpr& expr)
{
    Value* lhs = generate(expr.leftExpr());
    Value* rhs = generate(expr.rightExpr());
    result_ = insert(new VmInstr(getInsertPoint(), expr.op(), {lhs, rhs}));
}

void IRGenerator::accept(CallExpr& call)
{
    std::vector<Value*> args;
    for (Expr* arg: call.args().values()) {
        if (Value* v = generate(arg)) {
            args.push_back(v);
        } else {
            return;
        }
    }

    Value* f = generate(call.callee());

    if (call.callee()->isFunction()) {
        // builtin function
        result_ = createCallFunction(f, args);
    } else if (call.callee()->isBuiltin()) {
        // builtin handler
        result_ = createInvokeHandler(f, args);
    } else {
        // source handler
        result_ = nullptr; // TODO: inline source handler
    }
}

void IRGenerator::accept(VariableExpr& expr)
{
    // loads the value of the given variable

    result_ = generate(expr.variable());
}

void IRGenerator::accept(HandlerRefExpr& literal)
{
    // lodas a handler reference (handler ID) to a handler, possibly generating the code for this handler.

    result_ = generate(literal.handler());
}

void IRGenerator::accept(StringExpr& literal)
{
    // loads a string literal

    result_ = get(literal.value());
}

void IRGenerator::accept(NumberExpr& literal)
{
    // loads a number literal

    result_ = get(literal.value());
}

void IRGenerator::accept(BoolExpr& literal)
{
    // loads a boolean literal

    result_ = get(int64_t(literal.value()));
}

void IRGenerator::accept(RegExpExpr& literal)
{
    // loads a regex literal by reference ID to the const table

    result_ = get(literal.value());
}

void IRGenerator::accept(IPAddressExpr& literal)
{
    // loads an ip address by reference ID to the const table

    result_ = get(literal.value());
}

void IRGenerator::accept(CidrExpr& literal)
{
    // loads a CIDR network by reference ID to the const table

    result_ = get(literal.value());
}

void IRGenerator::accept(ArrayExpr& arrayExpr)
{
    // loads a new array of given elements

    Value* array = createAlloca(arrayExpr.getType(), get(arrayExpr.values().size()));

    for (size_t i = 0, e = arrayExpr.values().size(); i != e; ++i) {
        Value* element = generate(arrayExpr.values()[i].get());
        createArraySet(array, get(i), element);
    }

    result_ = array;
}

void IRGenerator::accept(ExprStmt& stmt)
{
    generate(stmt.expression());
}

void IRGenerator::accept(CompoundStmt& compound)
{
    for (const auto& stmt: compound) {
        generate(stmt.get());
    }
}

void IRGenerator::accept(CondStmt& stmt)
{
    BasicBlock* trueBlock = createBlock("trueBlock");
    BasicBlock* falseBlock = createBlock("falseBlock");
    BasicBlock* contBlock = createBlock("contBlock");

    Value* cond = generate(stmt.condition());
    createCondBr(cond, trueBlock, falseBlock, "if.cond");

    setInsertPoint(trueBlock);
    generate(stmt.thenStmt());
    createBr(contBlock);

    setInsertPoint(falseBlock);
    generate(stmt.elseStmt());
    createBr(contBlock);

    setInsertPoint(contBlock);
}

void IRGenerator::accept(MatchStmt& stmt)
{
    // TODO

    BasicBlock* contBlock = createBlock("match.cont");
    MatchInstr* matchInstr = new MatchInstr(getInsertPoint(), stmt.op());

    Value* cond = generate(stmt.condition());
    matchInstr->setCondition(cond);

    for (const MatchCase& one: stmt.cases()) {
        Value* label;
        if (auto e = dynamic_cast<StringExpr*>(one.first.get()))
            label = get(e->value());
        else if (auto e = dynamic_cast<RegExpExpr*>(one.first.get()))
            label = get(e->value());
        else {
            reportError("FIXME: Invalid (unsupported) literal type <%s> in match case.",
                    tos(one.first->getType()).c_str());
            result_ = nullptr;
            return;
        }

        BasicBlock* bb = createBlock("match.case");
        setInsertPoint(bb);
        generate(one.second.get());
        createBr(contBlock);

        matchInstr->addCase(label, bb);
    }

    if (stmt.elseStmt()) {
        BasicBlock* elseBlock = createBlock("match.else");
        setInsertPoint(elseBlock);
        generate(stmt.elseStmt());
        createBr(contBlock);

        matchInstr->setElseBlock(elseBlock);
    }

    setInsertPoint(contBlock);
}

void IRGenerator::accept(AssignStmt& stmt)
{
    //auto lhs = scope().lookup(assign.variable());
    Value* lhs = generate(stmt.variable());
    Value* rhs = generate(stmt.expression());

    result_ = createStore(lhs, rhs, stmt.variable()->name());
}

void IRGenerator::reportError(const std::string& message)
{
    fprintf(stderr, "%s\n", message.c_str());
}
// }}}

} // namespace x0
