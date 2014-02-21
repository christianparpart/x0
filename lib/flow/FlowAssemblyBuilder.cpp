#include <x0/flow/FlowAssemblyBuilder.h>
#include <x0/flow/vm/Program.h>
#include <x0/flow/vm/Match.h>
#include <cassert>

namespace x0 {

using FlowVM::Opcode;
using FlowVM::makeInstruction;

FlowAssemblyBuilder::FlowAssemblyBuilder() :
    scope_(new Scope()),
    unit_(nullptr),
    numbers_(),
    strings_(),
    regularExpressions_(),
    matches_(),
    modules_(),
    nativeHandlerSignatures_(),
    nativeFunctionSignatures_(),
    handlers_(),
    handlerId_(0),
    code_(),
    program_(),
    registerCount_(1),
    result_(0),
    errors_()
{
}

FlowAssemblyBuilder::~FlowAssemblyBuilder()
{
    delete scope_;
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

    // explicitely forward-declare handler, so we can use its ID internally.
    handlerId_ = handlerRef(&handler);

    registerCount_ = 1;

    codegenInline(&handler);
    emit(Opcode::EXIT, 0);

    handlers_[handlerId_].second = std::move(code_);
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
        cidrs_,
        regularExpressions_,
        matches_,
        unit.imports(),
        nativeHandlerSignatures_,
        nativeFunctionSignatures_,
        handlers_
    ));
}

void FlowAssemblyBuilder::accept(UnaryExpr& expr)
{
    Register subExpr = codegen(expr.subExpr());
    result_ = allocate();
    emit(expr.op(), result_, subExpr);
}

void FlowAssemblyBuilder::accept(BinaryExpr& expr)
{
    switch (expr.op()) {
        case Opcode::BOR: { // boolean OR
            // (lhs || rhs)
            //
            //   L = lhs();
            //   if (L) goto end;
            //   R = rhs();
            //   L = R;
            // end:
            //   result = L;
            //
            Register lhs = codegen(expr.leftExpr());
            size_t contJump = emit(Opcode::JN, lhs, 0/*XXX*/);

            Register rhs = codegen(expr.rightExpr());
            emit(Opcode::MOV, lhs, rhs);

            code_[contJump] = makeInstruction(Opcode::JN, lhs, code_.size());

            result_ = lhs;
            break;
        }
        default: {
            Register lhs = codegen(expr.leftExpr());
            Register rhs = codegen(expr.rightExpr());
            result_ = allocate();
            emit(expr.op(), result_, lhs, rhs);
            break;
        }
    }
}

void FlowAssemblyBuilder::accept(CallExpr& call)
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

Register FlowAssemblyBuilder::literal(const std::string& value)
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

Register FlowAssemblyBuilder::literal(const Cidr& value)
{
    for (size_t i = 0, e = cidrs_.size(); i != e; ++i)
        if (cidrs_[i] == value)
            return i;

    cidrs_.push_back(value);
    return cidrs_.size() - 1;
}

Register FlowAssemblyBuilder::literal(const RegExp& re)
{
    for (size_t i = 0, e = regularExpressions_.size(); i != e; ++i)
        if (regularExpressions_[i] == re.c_str())
            return i;

    regularExpressions_.push_back(re.c_str());
    return regularExpressions_.size() - 1;
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
//    result_ = allocate();
//    emit(Opcode::IMOV, result_, literal(expr.value()));
    result_ = literal(expr.value());
}

void FlowAssemblyBuilder::accept(IPAddressExpr& expr)
{
    result_ = allocate();
    emit(Opcode::PCONST, result_, literal(expr.value()));
}

void FlowAssemblyBuilder::accept(CidrExpr& cidr)
{
    result_ = allocate();
    emit(Opcode::CCONST, result_, literal(cidr.value()));
}

void FlowAssemblyBuilder::accept(ArrayExpr& arrayExpr)
{
    Register array = allocate();
    switch (arrayExpr.getType()) {
        case FlowType::StringArray:
            emit(Opcode::ASNEW, array, arrayExpr.values().size());
            for (size_t i = 0, e = arrayExpr.values().size(); i != e; ++i) {
                Register value = codegen(arrayExpr.values()[i].get());
                emit(Opcode::ASINIT, array, i, value);
            }
            break;
        case FlowType::IntArray:
            emit(Opcode::ANNEW, array, arrayExpr.values().size());
            for (size_t i = 0, e = arrayExpr.values().size(); i != e; ++i) {
                Register value = codegen(arrayExpr.values()[i].get());
                emit(Opcode::ANINIT, array, i, value);
            }
            break;
        default:
            return;
    }

    result_ = array;
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
    emit(Opcode::JN, expr, code_.size() + 2);
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
    FlowVM::MatchDef def;
    def.handlerId = handlerId_;
    def.op = stmt.op();
    def.elsePC = 0; // is properly initialized later

    Register cond = codegen(stmt.condition());
    size_t matchId = matches_.size();

    Opcode opc;
    switch (stmt.op()) {
        case FlowVM::MatchClass::Same:
            opc = Opcode::SMATCHEQ;
            break;
        case FlowVM::MatchClass::Head:
            opc = Opcode::SMATCHBEG;
            break;
        case FlowVM::MatchClass::Tail:
            opc = Opcode::SMATCHEND;
            break;
        case FlowVM::MatchClass::RegExp:
            opc = Opcode::SMATCHR;
            break;
        default:
            assert(!"FIXME: unsupported Match class");
            return;
    }
    emit(opc, cond, matchId);

    std::vector<size_t> exitJumps;

    for (const MatchCase& one: stmt.cases()) {
        Register label;
        if (auto e = dynamic_cast<StringExpr*>(one.first.get()))
            label = literal(e->value());
        else if (auto e = dynamic_cast<RegExpExpr*>(one.first.get()))
            label = literal(e->value());
        else {
            reportError("FIXME: Invalid (unsupported) literal type <%s> in match case.",
                    tos(one.first->getType()).c_str());
            return;
        }

        Stmt* code = one.second.get();

        def.cases.push_back(FlowVM::MatchCaseDef(label, code_.size()));
        codegen(code);
        exitJumps.push_back(emit(Opcode::JMP, 0));
    }

    def.elsePC = code_.size();
    if (stmt.elseStmt()) {
        codegen(stmt.elseStmt());
    }

    auto exitLabel = code_.size();

    for (const auto& jump: exitJumps) {
        code_[jump] = makeInstruction(Opcode::JMP, exitLabel);
    }

    matches_.push_back(def);
}

void FlowAssemblyBuilder::accept(AssignStmt& assign)
{
    Register lhs = scope().lookup(assign.variable());
    Register rhs = codegen(assign.expression());

    emit(Opcode::MOV, lhs, rhs);
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
    if (stmt) {
        stmt->visit(*this);
    }
}

} // namespace x0
