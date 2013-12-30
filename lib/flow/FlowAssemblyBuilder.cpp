#include <x0/flow/FlowAssemblyBuilder.h>
#include <x0/flow/vm/Program.h>
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

    b.accept(*unit);

    return std::move(b.program_);
}

void FlowAssemblyBuilder::reportError(const std::string& message)
{
    fprintf(stderr, "[Error] %s\n", message.c_str());
    errors_.push_back(message);
}

void FlowAssemblyBuilder::accept(Variable& variable)
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

void FlowAssemblyBuilder::accept(Handler& handler)
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

void FlowAssemblyBuilder::accept(BuiltinFunction& symbol)
{
    //assert(!"TODO: builtin function");
}

void FlowAssemblyBuilder::accept(BuiltinHandler& symbol)
{
    //assert(!"TODO: builtin handler");
}

void FlowAssemblyBuilder::accept(Unit& unit)
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
        ipaddrs_,
        regularExpressions_,
        unit.imports(),
        nativeHandlerSignatures_,
        nativeFunctionSignatures_
    ));

    for (auto handler: handlers_) {
        program_->createHandler(handler.first, handler.second);
    }
}

void FlowAssemblyBuilder::accept(UnaryExpr& expr)
{
    Register subExpr = codegen(expr.subExpr());
    result_ = allocate();
    emit(expr.op(), result_, subExpr);
}

void FlowAssemblyBuilder::accept(BinaryExpr& expr)
{
    Register lhs = codegen(expr.leftExpr());
    Register rhs = codegen(expr.rightExpr());
    result_ = allocate();

    emit(expr.op(), result_, lhs, rhs);
}

void FlowAssemblyBuilder::accept(FunctionCall& call)
{
    codegenBuiltin(call.callee(), call.args());
}

void FlowAssemblyBuilder::accept(VariableExpr& expr)
{
    result_ = scope().lookup(expr.variable());
}

void FlowAssemblyBuilder::accept(HandlerRefExpr& expr)
{
    size_t hrefId = handlerRef(expr.handler());
    result_ = allocate();
    emit(Opcode::IMOV, result_, hrefId);
}

void FlowAssemblyBuilder::accept(StringExpr& expr)
{
    result_ = allocate();

    emit(Opcode::SCONST, result_, literal(expr.value()));
}

void FlowAssemblyBuilder::accept(NumberExpr& expr)
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

Register FlowAssemblyBuilder::literal(const IPAddress& value)
{
    for (size_t i = 0, e = ipaddrs_.size(); i != e; ++i)
        if (ipaddrs_[i] == value)
            return i;

    ipaddrs_.push_back(value);
    return ipaddrs_.size() - 1;
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

void FlowAssemblyBuilder::accept(BoolExpr& expr)
{
    result_ = allocate();
    emit(Opcode::IMOV, result_, expr.value());
}

void FlowAssemblyBuilder::accept(RegExpExpr& expr)
{
    printf("TODO: regex expr\n");
}

void FlowAssemblyBuilder::accept(IPAddressExpr& expr)
{
    result_ = allocate();
    emit(Opcode::PCONST, result_, literal(expr.value()));
}

void FlowAssemblyBuilder::accept(CidrExpr& cidr)
{
    printf("TODO: cidr expr\n");
}

void FlowAssemblyBuilder::accept(ExprStmt& stmt)
{
    codegen(stmt.expression());
}

void FlowAssemblyBuilder::accept(CompoundStmt& compound)
{
    for (const auto& stmt: compound) {
        codegen(stmt.get());
    }
}

void FlowAssemblyBuilder::accept(CondStmt& stmt)
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

void FlowAssemblyBuilder::accept(MatchStmt& stmt)
{
    printf("TODO: (CG) MatchStmt\n");
}

void FlowAssemblyBuilder::accept(AssignStmt& assign)
{
    Register lhs = scope().lookup(assign.variable());
    Register rhs = codegen(assign.expression());

    emit(Opcode::MOV, lhs, rhs);
}

void FlowAssemblyBuilder::accept(HandlerCall& call)
{
    if (call.callee()->isBuiltin()) {
        // call to builtin handler/function
        codegenBuiltin(call.callee(), call.args());
    } else {
        assert(call.callee()->isHandler());
        size_t mark = registerCount_;
        codegenInline(static_cast<Handler*>(call.callee()));
        registerCount_ = mark; // unwind
    }
}

void FlowAssemblyBuilder::codegenBuiltin(Callable* callee, const ParamList& args)
{
    int argc = args.size() + 1;
    Register rbase = allocate(argc);

    // emit call args
    for (int i = 1; i < argc; ++i) {
        Register tmp = codegen(args.values()[i - 1]);
        emit(Opcode::MOV, rbase + i, tmp);
    }

    // emit call
    if (callee->isHandler()) {
        Register nativeId = nativeHandler(static_cast<BuiltinHandler*>(callee));
        emit(Opcode::HANDLER, nativeId, argc, rbase);
    } else {
        Register nativeId = nativeFunction(static_cast<BuiltinFunction*>(callee));
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
    symbol->visit(*this);
    return result_;
}

Register FlowAssemblyBuilder::codegen(Expr* expression)
{
    expression->visit(*this);
    return result_;
}

void FlowAssemblyBuilder::codegen(Stmt* stmt)
{
    stmt->visit(*this);
}

} // namespace x0
