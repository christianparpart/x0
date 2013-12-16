#include <x0/flow2/FlowAssemblyBuilder.h>
#include <x0/flow2/vm/Program.h>
#include <cassert>

namespace x0 {

using FlowVM::Opcode;
using FlowVM::makeInstruction;

FlowAssemblyBuilder::FlowAssemblyBuilder() :
    scope_(new Scope()),
    unit_(nullptr),
    numbers_(),
    strings_(),
    modules_(),
    nativeHandlerSignatures_(),
    nativeFunctionSignatures_(),
    handlers_(),
    handler_(nullptr),
    code_(),
    program_(),
    registerCount_(1),
    result_(0),
    errors_()
{
}

FlowAssemblyBuilder::~FlowAssemblyBuilder()
{
}

std::unique_ptr<FlowVM::Program> FlowAssemblyBuilder::compile(Unit* unit)
{
    FlowAssemblyBuilder b;

    b.visit(*unit);

    return std::move(b.program_);
}

void FlowAssemblyBuilder::reportError(const std::string& message)
{
    fprintf(stderr, "[Error] %s\n", message.c_str());
    errors_.push_back(message);
}

void FlowAssemblyBuilder::visit(Variable& variable)
{
    // declares & initializes a variable
    result_ = codegen(variable.initializer());
    scope().insert(&variable, result_);
}

void FlowAssemblyBuilder::codegenInline(Handler* handler)
{
    for (Symbol* symbol: *handler->scope()) {
        codegen(symbol);
    }

    codegen(handler->body());
}

void FlowAssemblyBuilder::visit(Handler& handler)
{
    if (handler.isForwardDeclared()) {
        reportError("Implicitely forward declared handler \"%s\" is missing implementation.", handler.name().c_str());

        return;
    }

    registerCount_ = 1;

    codegenInline(&handler);
    emit(Opcode::EXIT, 0);

    for (auto& hand: handlers_) {
        if (hand.first == handler.name()) {
            // implement forward-referenced handler
            hand.second = std::move(code_);
            return;
        }
    }
    // implement handler
    handlers_.push_back(std::make_pair(handler.name(), std::move(code_)));
}

void FlowAssemblyBuilder::visit(BuiltinFunction& symbol)
{
    //assert(!"TODO: builtin function");
}

void FlowAssemblyBuilder::visit(BuiltinHandler& symbol)
{
    //assert(!"TODO: builtin handler");
}

void FlowAssemblyBuilder::visit(Unit& unit)
{
    for (Symbol* symbol: *unit.scope()) {
        codegen(symbol);

        if (!errors_.empty()) {
            return;
        }
    }

    program_ = std::unique_ptr<FlowVM::Program>(new FlowVM::Program(
        numbers_,
        strings_,
        regularExpressions_,
        unit.imports(),
        nativeHandlerSignatures_,
        nativeFunctionSignatures_
    ));

    for (auto handler: handlers_) {
        program_->createHandler(handler.first, handler.second);
    }
}

void FlowAssemblyBuilder::visit(UnaryExpr& expr)
{
    Register subExpr = codegen(expr.subExpr());
    result_ = allocate();
    emit(expr.op(), result_, subExpr);
}

void FlowAssemblyBuilder::visit(BinaryExpr& expr)
{
    Register lhs = codegen(expr.leftExpr());
    Register rhs = codegen(expr.rightExpr());
    result_ = allocate();

    emit(expr.op(), result_, lhs, rhs);
}

void FlowAssemblyBuilder::visit(FunctionCallExpr& call)
{
    int argc = call.args().size() + 1;
    Register rbase = allocate(argc);

    for (int i = 1; i < argc; ++i) {
        Register tmp = codegen(call.args()[i - 1]);
        emit(Opcode::MOV, rbase + i, tmp);
    }

    Register nativeId = nativeFunction(static_cast<BuiltinFunction*>(call.callee()));
    emit(Opcode::CALL, nativeId, argc, rbase);
    result_ = rbase;
}

void FlowAssemblyBuilder::visit(VariableExpr& expr)
{
    result_ = scope().lookup(expr.variable());
}

void FlowAssemblyBuilder::visit(HandlerRefExpr& expr)
{
    size_t hrefId = handlerRef(expr.handler());
    result_ = allocate();
    emit(Opcode::IMOV, result_, hrefId);
}

void FlowAssemblyBuilder::visit(StringExpr& expr)
{
    result_ = allocate();

    emit(Opcode::SCONST, result_, literal(expr.value()));
}

void FlowAssemblyBuilder::visit(NumberExpr& expr)
{
    result_ = allocate();

    if (expr.value() >= -32768 && expr.value() <= 32768) {
        emit(Opcode::IMOV, result_, expr.value());
    } else {
        emit(Opcode::NCONST, result_, literal(expr.value()));
    }
}

Register FlowAssemblyBuilder::literal(FlowNumber value)
{
    for (size_t i = 0, e = numbers_.size(); i != e; ++i)
        if (numbers_[i] == value)
            return i;

    numbers_.push_back(value);
    return numbers_.size() - 1;
}

Register FlowAssemblyBuilder::literal(const FlowString& value)
{
    for (size_t i = 0, e = strings_.size(); i != e; ++i)
        if (strings_[i] == value)
            return i;

    strings_.push_back(value);
    return strings_.size() - 1;
}

/**
 * Retrieves the program's handler ID for given handler, possibly forward-declaring given handler if not (yet) found.
 */
size_t FlowAssemblyBuilder::handlerRef(Handler* handler)
{
    for (size_t i = 0, e = handlers_.size(); i != e; ++i)
        if (handlers_[i].first == handler->name())
            return i;

    handlers_.push_back(std::make_pair(handler->name(), std::vector<FlowVM::Instruction>()));
    return handlers_.size() - 1;
}

Register FlowAssemblyBuilder::nativeHandler(BuiltinHandler* handler)
{
    std::string signature = handler->signature().to_s();

    for (size_t i = 0, e = nativeHandlerSignatures_.size(); i != e; ++i)
        if (nativeHandlerSignatures_[i] == signature)
            return i;

    nativeHandlerSignatures_.push_back(signature);
    return nativeHandlerSignatures_.size() - 1;
}

Register FlowAssemblyBuilder::nativeFunction(BuiltinFunction* function)
{
    std::string signature = function->signature().to_s();

    for (size_t i = 0, e = nativeFunctionSignatures_.size(); i != e; ++i)
        if (nativeFunctionSignatures_[i] == signature)
            return i;

    nativeFunctionSignatures_.push_back(signature);
    return nativeFunctionSignatures_.size() - 1;
}

void FlowAssemblyBuilder::visit(BoolExpr& expr)
{
    result_ = allocate();
    emit(Opcode::IMOV, result_, expr.value());
}

void FlowAssemblyBuilder::visit(RegExpExpr& expr)
{
    printf("TODO: regex expr\n");
}

void FlowAssemblyBuilder::visit(IPAddressExpr& expr)
{
    printf("TODO: ipaddr expr\n");
}

void FlowAssemblyBuilder::visit(CidrExpr& cidr)
{
    printf("TODO: cidr expr\n");
}

void FlowAssemblyBuilder::visit(ExprStmt& stmt)
{
    printf("TODO: expr stmt\n");
}

void FlowAssemblyBuilder::visit(CompoundStmt& compound)
{
    for (const auto& stmt: compound) {
        codegen(stmt.get());
    }
}

void FlowAssemblyBuilder::visit(CondStmt& stmt)
{
    Register expr = codegen(stmt.condition());
    emit(Opcode::CONDBR, expr, code_.size() + 2);
    size_t elseBlock = emit(Opcode::JMP, 0);

    codegen(stmt.thenStmt());
    size_t end = emit(Opcode::JMP);

    size_t elseBlockStart = code_.size();
    codegen(stmt.elseStmt());
    size_t elseBlockEnd = code_.size();

    code_[elseBlock] = makeInstruction(Opcode::JMP, elseBlockStart);
    code_[end] = makeInstruction(Opcode::JMP, elseBlockEnd);
}

void FlowAssemblyBuilder::visit(AssignStmt& assign)
{
    Register lhs = scope().lookup(assign.variable());
    Register rhs = codegen(assign.expression());

    emit(Opcode::MOV, lhs, rhs);
}

void FlowAssemblyBuilder::visit(CallStmt& call)
{
    if (!call.callee()->isBuiltin()) {
        assert(call.callee()->isHandler());
        size_t mark = registerCount_;
        codegenInline(static_cast<Handler*>(call.callee()));
        registerCount_ = mark; // unwind
        return;
    }

    // builtin handler/function call
    int argc = call.args().size() + 1;
    Register rbase = allocate(argc);

    for (int i = 1; i < argc; ++i) {
        Register tmp = codegen(call.args()[i - 1]);
        emit(Opcode::MOV, rbase + i, tmp);
    }

    if (call.callee()->isHandler()) {
        Register nativeId = nativeHandler(static_cast<BuiltinHandler*>(call.callee()));
        emit(Opcode::HANDLER, nativeId, argc, rbase);
    } else {
        Register nativeId = nativeFunction(static_cast<BuiltinFunction*>(call.callee()));
        emit(Opcode::CALL, nativeId, argc, rbase);
        result_ = rbase;
    }
}

Register FlowAssemblyBuilder::allocate(size_t n)
{
    Register base = registerCount_;
    registerCount_ += n;
    return base;
}

size_t FlowAssemblyBuilder::emit(FlowVM::Instruction instr)
{
    code_.push_back(instr);
    return code_.size() - 1;
}

Register FlowAssemblyBuilder::codegen(Symbol* symbol)
{
    symbol->accept(*this);
    return result_;
}

Register FlowAssemblyBuilder::codegen(Expr* expression)
{
    expression->accept(*this);
    return result_;
}

void FlowAssemblyBuilder::codegen(Stmt* stmt)
{
    stmt->accept(*this);
}

} // namespace x0
