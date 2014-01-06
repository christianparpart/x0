/* <IR.cpp>
 *
 * This file is part of the x0 web server project and is released under AGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2014 Christian Parpart <trapni@gmail.com>
 */

#include <x0/flow/IR.h>
#include <x0/flow/AST.h>
#include <utility> // make_pair
#include <assert.h>

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
 * Useful references:
 * ~~~~~~~~~~~~~~~~~~
 *
 * - http://en.wikipedia.org/wiki/Static_single_assignment_form
 * - http://en.wikipedia.org/wiki/Basic_block
 * - http://en.wikipedia.org/wiki/Use-define_chain
 * - http://en.wikipedia.org/wiki/Compiler_optimization
 * - http://llvm.org/doxygen/classllvm_1_1Value.html
 *
 * CLASS HIERARCHY:
 * ~~~~~~~~~~~~~~~~
 *
 * - Value
 *   - Constant
 *     - ConstantBase<T>
 *       - ConstantInt <int64_t>
 *       - ConstantStr <string>
 *       - ConstantIP <IPAddress>
 *       - ConstantCidr <Cidr>
 *       - ConstantRegExp <RegExp>
 *     - IRHandler
 *   - Instr
 *     - VmInstr
 *     - PhiNode
 *   - BasicBlock
 * - IRProgram
 * - IRBuilder
 */

namespace x0 {

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

void Constant::dump()
{
    printf("Constant %zu '%s': %s\n", id_, name().c_str(), tos(type()).c_str());
}

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
    for (auto& value: numbers_) delete value.second;
    for (auto& value: strings_) delete value.second;
    for (auto& value: ipaddrs_) delete value.second;
    for (auto& value: cidrs_) delete value.second;
    for (auto& value: regexps_) delete value.second;
    for (auto& value: handlers_) delete value.second;
}

void IRProgram::dump()
{
    printf("; IRProgram\n");
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

void IRBuilder::setProgram(IRProgram* prog)
{
    program_ = prog;
}

void IRBuilder::setHandler(IRHandler* hn)
{
    assert(hn->parent() == program_);

    handler_ = hn;
}

BasicBlock* IRBuilder::createBlock(const std::string& name)
{
    return new BasicBlock(handler_, name);
}

void IRBuilder::setInsertPoint(BasicBlock* bb)
{
    assert(bb != nullptr);

    insertPoint_ = bb;
    bb->parent_ = handler_;
}

Instr* IRBuilder::insert(Instr* instr)
{
    assert(instr != nullptr);
    assert(insertPoint_ != nullptr);
    assert(instr->parent() == nullptr);

    instr->setParent(insertPoint_);
    insertPoint_->code_.push_back(instr);
    insertPoint_->setType(instr->type());

    return instr;
}

IRHandler* IRBuilder::getHandler(const std::string& name)
{
    for (auto& item: program_->handlers_) {
        if (item.first == name)
            return item.second;
    }

    size_t id = program_->handlers_.size();
    IRHandler* h = new IRHandler(program_, id, name);
    program_->handlers_.push_back(std::make_pair(name, h));

    return h;
}

Value* IRBuilder::get(int64_t literal)
{
    return nullptr; // TODO
}

Value* IRBuilder::get(const std::string& literal)
{
    return nullptr; // TODO
}

Value* IRBuilder::get(const RegExp& literal)
{
    return nullptr; // TODO
}

// {{{ value management
Instr* IRBuilder::createAlloca(FlowType ty, size_t arraySize, const std::string& name)
{
    return nullptr; // TODO
}

Instr* IRBuilder::createStore(Value* lhs, Value* rhs, const std::string& name)
{
    return nullptr; // TODO
}
// }}}

Value* IRBuilder::createAdd(Value* lhs, Value* rhs, const std::string& name)
{
    assert(lhs->type() == rhs->type());

    switch (lhs->type()) {
        case FlowType::Number:
            return insert(new VmInstr(insertPoint_, FlowVM::Opcode::NADD, {lhs, rhs}, name));
        case FlowType::String:
            return insert(new VmInstr(insertPoint_, FlowVM::Opcode::SADD, {lhs, rhs}, name));
        default:
            assert(!"Unsupported type");
            return nullptr;
    }
}

Instr* IRBuilder::createRet(Value* result, const std::string& name)
{
    return nullptr; // TODO
}

Instr* IRBuilder::createBr(BasicBlock* bb)
{
    return insert(new VmInstr(insertPoint_, FlowVM::Opcode::JMP, {}, ""));
}

Instr* IRBuilder::createCondBr(Value* condValue, BasicBlock* trueBlock, BasicBlock* falseBlock, const std::string& name)
{
    return insert(new CondBrInstr(insertPoint_, condValue, trueBlock, falseBlock, name));
}
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

void IRGenerator::accept(FunctionCall& expr)
{
    // TODO
}

void IRGenerator::accept(VariableExpr& expr)
{
    // TODO
}

void IRGenerator::accept(HandlerRefExpr& literal)
{
    // TODO
}

void IRGenerator::accept(StringExpr& literal)
{
    result_ = get(literal.value());
}

void IRGenerator::accept(NumberExpr& literal)
{
    result_ = get(literal.value());
}

void IRGenerator::accept(BoolExpr& literal)
{
    result_ = get(int64_t(literal.value()));
}

void IRGenerator::accept(RegExpExpr& literal)
{
    // TODO
}

void IRGenerator::accept(IPAddressExpr& literal)
{
    // TODO
}

void IRGenerator::accept(CidrExpr& literal)
{
    // TODO
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

void IRGenerator::accept(HandlerCall& stmt)
{
}

void IRGenerator::reportError(const std::string& message)
{
    fprintf(stderr, "%s\n", message.c_str());
}
// }}}

} // namespace x0
