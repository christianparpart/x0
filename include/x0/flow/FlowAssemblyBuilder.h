#pragma once

#include <x0/sysconfig.h>
#include <x0/flow/FlowType.h>
#include <x0/flow/AST.h>
#include <x0/flow/ASTVisitor.h>
#include <x0/flow/vm/Instruction.h>
#include <x0/flow/vm/Match.h>

#include <functional>
#include <cstdio>
#include <deque>
#include <vector>
#include <unordered_map>

namespace x0 {

namespace FlowVM {
    class Program;
    class Handler;
}

/**
 * @brief transforms a Flow AST into its assembly representation.
 *
 * @see FlowVM::Program
 */
class X0_API FlowAssemblyBuilder :
    public ASTVisitor
{
    class Scope;

public:
    FlowAssemblyBuilder();
    ~FlowAssemblyBuilder();

    static std::unique_ptr<FlowVM::Program> compile(Unit* unit);

private:
    bool prepare();
    Scope& scope() const { return *scope_; }

    // error handling
    void reportError(const std::string& message);
    template<typename... Args> void reportError(const std::string& fmt, Args&&...);

    // AST code generation
    virtual void accept(Variable& variable);
    virtual void accept(Handler& handler);
    virtual void accept(BuiltinFunction& symbol);
    virtual void accept(BuiltinHandler& symbol);
    virtual void accept(Unit& symbol);
    virtual void accept(UnaryExpr& expr);
    virtual void accept(BinaryExpr& expr);
    virtual void accept(FunctionCall& expr);
    virtual void accept(VariableExpr& expr);
    virtual void accept(HandlerRefExpr& expr);
    virtual void accept(StringExpr& expr);
    virtual void accept(NumberExpr& expr);
    virtual void accept(BoolExpr& expr);
    virtual void accept(RegExpExpr& expr);
    virtual void accept(IPAddressExpr& expr);
    virtual void accept(CidrExpr& cidr);
    virtual void accept(ExprStmt& stmt);
    virtual void accept(CompoundStmt& stmt);
    virtual void accept(CondStmt& stmt);
    virtual void accept(MatchStmt& stmt);
    virtual void accept(AssignStmt& stmt);
    virtual void accept(HandlerCall& stmt);

    Register allocate(size_t n = 1);
    Register codegen(Symbol* symbol);
    Register codegen(Expr* expression);
    void codegen(Stmt* expression);

    size_t emit(FlowVM::Opcode opc) { return emit(FlowVM::makeInstruction(opc)); }
    size_t emit(FlowVM::Opcode opc, FlowVM::Operand op1) { return emit(FlowVM::makeInstruction(opc, op1)); }
    size_t emit(FlowVM::Opcode opc, FlowVM::Operand op1, FlowVM::Operand op2) { return emit(FlowVM::makeInstruction(opc, op1, op2)); }
    size_t emit(FlowVM::Opcode opc, FlowVM::Operand op1, FlowVM::Operand op2, FlowVM::Operand op3) { return emit(FlowVM::makeInstruction(opc, op1, op2, op3)); }
    size_t emit(FlowVM::Instruction instr);

    Register literal(FlowNumber value);
    Register literal(const FlowString& value);
    Register literal(const IPAddress& value);
    Register literal(const RegExpExpr* re);
    size_t handlerRef(Handler* handler);
    Register nativeHandler(BuiltinHandler* handler);
    Register nativeFunction(BuiltinFunction* function);
    void codegenInline(Handler* handler);
    void codegenBuiltin(Callable* callee, const ParamList& args);

private:
    Scope* scope_;
    Unit* unit_;                            //!< input unit to be compiled

    // program output
    std::vector<FlowNumber> numbers_;
    std::vector<FlowString> strings_;
    std::vector<IPAddress> ipaddrs_;
    std::vector<std::string> regularExpressions_;               // XXX to be a pre-compiled handled during runtime
    std::vector<FlowVM::MatchDef> matches_;
    std::vector<std::pair<std::string, std::string>> modules_;
    std::vector<std::string> nativeHandlerSignatures_;
    std::vector<std::string> nativeFunctionSignatures_;

    std::vector<std::pair<std::string, std::vector<FlowVM::Instruction>>> handlers_;
    FlowVM::Handler* handler_;                  //!< current handler 
    size_t handlerId_;                          //!< current handler's ID
    std::vector<FlowVM::Instruction> code_;     //!< current handler's code 
    std::unique_ptr<FlowVM::Program> program_;  //!< current program
    Register registerCount_;                    //!< number of currently allocated registers
    Register result_;                           //!< current code generation's result register
    std::vector<std::string> errors_;           //!< list of raised errors during code generation.
};

// {{{ class FlowAssemblyBuilder::Scope
class FlowAssemblyBuilder::Scope
{
private:
    std::unordered_map<Symbol*, Register> scope_;

public:
    Scope() {}
    ~Scope() {}

    void clear() {
        scope_.clear();
    }

    Register lookup(Symbol* symbol) const {
        auto i = scope_.find(symbol);
        return i != scope_.end() ? i->second : Register(0);
    }

    void insert(Symbol* symbol, Register value) {
        scope_[symbol] = value;
    }

    void remove(Symbol* symbol) {
        auto i = scope_.find(symbol);
        if (i != scope_.end()) {
            scope_.erase(i);
        }
    }
};
// }}}

//{{{ FlowAssemblyBuilder inlines
template<typename... Args>
inline void FlowAssemblyBuilder::reportError(const std::string& fmt, Args&&... args)
{
    char buf[1024];
    ssize_t n = snprintf(buf, sizeof(buf), fmt.c_str(), std::forward<Args>(args)...);

    if (n > 0) {
        reportError(buf);
    }
}
//}}}

} // namespace x0
