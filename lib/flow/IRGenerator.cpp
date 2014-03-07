/* <IRGenerator.cpp>
 *
 * This file is part of the x0 web server project and is released under AGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2014 Christian Parpart <trapni@gmail.com>
 */

#include <x0/flow/IRGenerator.h>
#include <x0/flow/AST.h>
#include <x0/flow/ir/IRProgram.h>
#include <x0/flow/ir/IRHandler.h>
#include <x0/flow/ir/Instructions.h>
#include <x0/DebugLogger.h> // XZERO_DEBUG
#include <assert.h>
#include <math.h>

namespace x0 {

#define FLOW_DEBUG_IR 1

#if defined(FLOW_DEBUG_IR)
// {{{ trace
static size_t fnd = 0;
struct fntrace3 {
	std::string msg_;

	fntrace3(const char* msg) : msg_(msg)
	{
		size_t i = 0;
		char fmt[1024];

		for (i = 0; i < 2 * fnd; ) {
			fmt[i++] = ' ';
			fmt[i++] = ' ';
		}
		fmt[i++] = '-';
		fmt[i++] = '>';
		fmt[i++] = ' ';
		strcpy(fmt + i, msg_.c_str());

		XZERO_DEBUG("IRGenerator", 5, "%s", fmt);
		++fnd;
	}

	~fntrace3() {
		--fnd;

		size_t i = 0;
		char fmt[1024];

		for (i = 0; i < 2 * fnd; ) {
			fmt[i++] = ' ';
			fmt[i++] = ' ';
		}
		fmt[i++] = '<';
		fmt[i++] = '-';
		fmt[i++] = ' ';
		strcpy(fmt + i, msg_.c_str());

		XZERO_DEBUG("IRGenerator", 5, "%s", fmt);
	}
};
// }}}
#	define FNTRACE() fntrace3 _(__PRETTY_FUNCTION__)
#	define TRACE(level, msg...) XZERO_DEBUG("IRGenerator", (level), msg)
#else
#	define FNTRACE() /*!*/
#	define TRACE(level, msg...) /*!*/
#endif

IRGenerator::IRGenerator() :
    IRBuilder(),
    ASTVisitor(),
    scope_(new Scope())
{
}

IRGenerator::~IRGenerator()
{
    delete scope_;
}

IRProgram* IRGenerator::generate(Unit* unit)
{
    IRGenerator ir;
    ir.codegen(unit);

    return ir.program();
}

Value* IRGenerator::codegen(Expr* expr)
{
    expr->visit(*this);
    return result_;
}

Value* IRGenerator::codegen(Stmt* stmt)
{
    if (stmt) {
        stmt->visit(*this);
    } else {
        result_ = nullptr;
    }
    return result_;
}

Value* IRGenerator::codegen(Symbol* sym)
{
    sym->visit(*this);
    return result_;
}

void IRGenerator::accept(Unit& unit)
{
    FNTRACE();

    setProgram(new IRProgram());

    for (const auto sym: *unit.scope()) {
        codegen(sym);
    }
}

void IRGenerator::accept(Variable& variable)
{
    FNTRACE();

    Value* initializer = codegen(variable.initializer());
    if (!initializer)
        return;

    AllocaInstr* var = createAlloca(initializer->type(), get(1), variable.name());
    scope().update(&variable, var);

    createStore(var, initializer);
    result_ = var;
}

void IRGenerator::accept(Handler& handlerSym)
{
    FNTRACE();

    setHandler(getHandler(handlerSym.name()));
    setInsertPoint(createBlock("EntryPoint"));

    codegenInline(handlerSym);

    createRet(get(false));

    handler()->verify();
}

void IRGenerator::codegenInline(Handler& handlerSym)
{
    // emit local variable declarations
    if (handlerSym.scope()) {
        for (Symbol* symbol: *handlerSym.scope()) {
            codegen(symbol);
        }
    }

    // emit body
    codegen(handlerSym.body());
}

void IRGenerator::accept(BuiltinFunction& builtin)
{
    FNTRACE();

    result_ = getBuiltinFunction(builtin.signature());
}

void IRGenerator::accept(BuiltinHandler& builtin)
{
    FNTRACE();

    result_ = getBuiltinHandler(builtin.signature());
}

void IRGenerator::accept(UnaryExpr& expr)
{
    FNTRACE();

    static const std::unordered_map<
        int /*FlowVM::Opcode*/,
        Value* (IRGenerator::*)(Value*, const std::string&)
    > ops = {
        { FlowVM::Opcode::I2S, &IRGenerator::createI2S },
        { FlowVM::Opcode::P2S, &IRGenerator::createP2S },
        { FlowVM::Opcode::C2S, &IRGenerator::createC2S },
        { FlowVM::Opcode::R2S, &IRGenerator::createR2S },
        { FlowVM::Opcode::S2I, &IRGenerator::createS2I },
        { FlowVM::Opcode::NNEG, &IRGenerator::createNeg },
    };

    Value* rhs = codegen(expr.subExpr());

    auto i = ops.find(expr.op());
    if (i != ops.end()) {
        result_ = (this->*i->second)(rhs, "");
    } else {
        assert(!"Unsupported unary expression in IRGenerator.");
        result_ = nullptr;
    }
}

void IRGenerator::accept(BinaryExpr& expr)
{
    FNTRACE();

    static const std::unordered_map<
        int /*FlowVM::Opcode*/,
        Value* (IRGenerator::*)(Value*, Value*, const std::string&)
    > ops = {
        // numerical
        { FlowVM::Opcode::NADD, &IRGenerator::createAdd },
        { FlowVM::Opcode::NSUB, &IRGenerator::createSub },
        { FlowVM::Opcode::NMUL, &IRGenerator::createMul },
        { FlowVM::Opcode::NDIV, &IRGenerator::createDiv },
        { FlowVM::Opcode::NREM, &IRGenerator::createRem },
        { FlowVM::Opcode::NSHL, &IRGenerator::createShl },
        { FlowVM::Opcode::NSHR, &IRGenerator::createShr },
        { FlowVM::Opcode::NPOW, &IRGenerator::createPow },
        { FlowVM::Opcode::NAND, &IRGenerator::createAnd },
        { FlowVM::Opcode::NOR,  &IRGenerator::createOr },
        { FlowVM::Opcode::NXOR, &IRGenerator::createXor },
        { FlowVM::Opcode::NCMPEQ, &IRGenerator::createNCmpEQ },
        { FlowVM::Opcode::NCMPNE, &IRGenerator::createNCmpNE },
        { FlowVM::Opcode::NCMPLE, &IRGenerator::createNCmpLE },
        { FlowVM::Opcode::NCMPGE, &IRGenerator::createNCmpGE },
        { FlowVM::Opcode::NCMPLT, &IRGenerator::createNCmpLT },
        { FlowVM::Opcode::NCMPGT, &IRGenerator::createNCmpGT },

        // string
        { FlowVM::Opcode::SADD, &IRGenerator::createSAdd },
        { FlowVM::Opcode::SCMPEQ, &IRGenerator::createSCmpEQ },
        { FlowVM::Opcode::SCMPNE, &IRGenerator::createSCmpNE },
        { FlowVM::Opcode::SCMPLE, &IRGenerator::createSCmpLE },
        { FlowVM::Opcode::SCMPGE, &IRGenerator::createSCmpGE },
        { FlowVM::Opcode::SCMPLT, &IRGenerator::createSCmpLT },
        { FlowVM::Opcode::SCMPGT, &IRGenerator::createSCmpGT },
        { FlowVM::Opcode::SCMPBEG, &IRGenerator::createSCmpEB },
        { FlowVM::Opcode::SCMPEND, &IRGenerator::createSCmpEE },
        { FlowVM::Opcode::SCONTAINS, &IRGenerator::createSIn },

        // regex
        { FlowVM::Opcode::SREGMATCH, &IRGenerator::createSCmpRE },

        // ip
        { FlowVM::Opcode::PCMPEQ, &IRGenerator::createPCmpEQ },
        { FlowVM::Opcode::PCMPNE, &IRGenerator::createPCmpNE },
        { FlowVM::Opcode::PINCIDR, &IRGenerator::createPInCidr },
    };

    if (expr.op() == FlowVM::Opcode::BOR) {
        // (lhs || rhs)
        //
        //   L = lhs();
        //   if (L) goto end;
        //   R = rhs();
        //   L = R;
        // end:
        //   result = L;

        BasicBlock* borLeft = createBlock("bor.left");
        BasicBlock* borRight = createBlock("bor.right");
        BasicBlock* borCont = createBlock("bor.cont");

        AllocaInstr* result = createAlloca(FlowType::Boolean, get(1), "bor");
        Value* lhs = codegen(expr.leftExpr());
        createCondBr(lhs, borLeft, borRight);

        setInsertPoint(borLeft);
        createStore(result, lhs, "bor.left");
        createBr(borCont);

        setInsertPoint(borRight);
        Value* rhs = codegen(expr.rightExpr());
        createStore(result, rhs, "bor.right");
        createBr(borCont);

        setInsertPoint(borCont);

        result_ = result;

        return;
    }

    Value* lhs = codegen(expr.leftExpr());
    Value* rhs = codegen(expr.rightExpr());

    auto i = ops.find(expr.op());
    if (i != ops.end()) {
        result_ = (this->*i->second)(lhs, rhs, "");
    } else {
        fprintf(stderr, "BUG: Binary operation `%s` not implemented.\n", mnemonic(expr.op()));
        assert(!"Unimplemented");
        result_ = nullptr;
    }
}

void IRGenerator::accept(CallExpr& call)
{
    FNTRACE();

    std::vector<Value*> args;
    for (Expr* arg: call.args().values()) {
        if (Value* v = codegen(arg)) {
            args.push_back(v);
        } else {
            return;
        }
    }

    if (call.callee()->isFunction()) {
        Value* callee = codegen(call.callee());
        // builtin function
        result_ = createCallFunction(static_cast<IRBuiltinFunction*>(callee), args);
    } else if (call.callee()->isBuiltin()) {
        Value* callee = codegen(call.callee());
        // builtin handler
        result_ = createInvokeHandler(static_cast<IRBuiltinHandler*>(callee), args);
    } else {
        // source handler
        codegenInline(*static_cast<Handler*>(call.callee()));
        result_ = nullptr;
    }
}

void IRGenerator::accept(VariableExpr& expr)
{
    FNTRACE();

    // loads the value of the given variable

    if (auto var = scope().lookup(expr.variable())) {
        result_ = createLoad(var);
    } else {
        result_ = nullptr;
    }
}

void IRGenerator::accept(HandlerRefExpr& literal)
{
    FNTRACE();

    // lodas a handler reference (handler ID) to a handler, possibly generating the code for this handler.

    result_ = codegen(literal.handler());
}

void IRGenerator::accept(StringExpr& literal)
{
    FNTRACE();

    // loads a string literal

    result_ = get(literal.value());
}

void IRGenerator::accept(NumberExpr& literal)
{
    FNTRACE();

    // loads a number literal

    result_ = get(literal.value());
}

void IRGenerator::accept(BoolExpr& literal)
{
    FNTRACE();

    // loads a boolean literal

    result_ = get(int64_t(literal.value()));
}

void IRGenerator::accept(RegExpExpr& literal)
{
    FNTRACE();

    // loads a regex literal by reference ID to the const table

    result_ = get(literal.value());
}

void IRGenerator::accept(IPAddressExpr& literal)
{
    FNTRACE();

    // loads an ip address by reference ID to the const table

    result_ = get(literal.value());
}

void IRGenerator::accept(CidrExpr& literal)
{
    FNTRACE();

    // loads a CIDR network by reference ID to the const table

    result_ = get(literal.value());
}

void IRGenerator::accept(ArrayExpr& arrayExpr)
{
    FNTRACE();

    Value* array = createAlloca(arrayExpr.getType(), get(1 + arrayExpr.values().size()));

    // store array size at array[0]
    createStore(array, get(0), get(arrayExpr.values().size()));

    // store array values at array[1..N]
    for (size_t i = 0, e = arrayExpr.values().size(); i != e; ++i) {
        Value* element = codegen(arrayExpr.values()[i].get());
        createStore(array, get(i + 1), element);
    }

    result_ = array;
}

void IRGenerator::accept(ExprStmt& stmt)
{
    FNTRACE();

    codegen(stmt.expression());
}

void IRGenerator::accept(CompoundStmt& compound)
{
    FNTRACE();

    for (const auto& stmt: compound) {
        codegen(stmt.get());
    }
}

void IRGenerator::accept(CondStmt& stmt)
{
    FNTRACE();

    BasicBlock* trueBlock = createBlock("trueBlock");
    BasicBlock* falseBlock = createBlock("falseBlock");
    BasicBlock* contBlock = createBlock("contBlock");

    Value* cond = codegen(stmt.condition());
    createCondBr(cond, trueBlock, falseBlock);

    setInsertPoint(trueBlock);
    codegen(stmt.thenStmt());
    createBr(contBlock);

    setInsertPoint(falseBlock);
    codegen(stmt.elseStmt());
    createBr(contBlock);

    setInsertPoint(contBlock);
}

Constant* IRGenerator::getConstant(Expr* expr)
{
    if (auto e = dynamic_cast<StringExpr*>(expr))
        return get(e->value());
    else if (auto e = dynamic_cast<RegExpExpr*>(expr))
        return get(e->value());
    else {
        reportError("FIXME: Invalid (unsupported) literal type <%s> in match case.",
                tos(expr->getType()).c_str());
        return nullptr;
    }
}

void IRGenerator::accept(MatchStmt& stmt)
{
    FNTRACE();

    Value* cond = codegen(stmt.condition());
    BasicBlock* contBlock = createBlock("match.cont");
    MatchInstr* matchInstr = createMatch(stmt.op(), cond);

    for (const MatchCase& one: stmt.cases()) {
        Constant* label = getConstant(one.first.get());

        BasicBlock* bb = createBlock("match.case");
        setInsertPoint(bb);
        codegen(one.second.get());
        createBr(contBlock);

        matchInstr->addCase(label, bb);
    }

    if (stmt.elseStmt()) {
        BasicBlock* elseBlock = createBlock("match.else");
        setInsertPoint(elseBlock);
        codegen(stmt.elseStmt());
        createBr(contBlock);

        matchInstr->setElseBlock(elseBlock);
    }

    setInsertPoint(contBlock);
}

void IRGenerator::accept(AssignStmt& stmt)
{
    FNTRACE();

    Value* lhs = scope().lookup(stmt.variable());
    Value* rhs = codegen(stmt.expression());
    assert(lhs->type() == rhs->type() && "Type of lhs and rhs must be equal.");

    result_ = createStore(lhs, rhs, "assignment");
}

void IRGenerator::reportError(const std::string& message)
{
    fprintf(stderr, "%s\n", message.c_str());
}

} // namespace x0
