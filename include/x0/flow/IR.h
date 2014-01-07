/* <IR.h>
 *
 * This file is part of the x0 web server project and is released under AGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2014 Christian Parpart <trapni@gmail.com>
 */

#pragma once

#include <x0/Api.h>
#include <x0/flow/vm/Instruction.h>
#include <x0/flow/vm/MatchClass.h>
#include <x0/flow/ASTVisitor.h>
#include <x0/IPAddress.h>
#include <x0/Cidr.h>
#include <x0/RegExp.h>

#include <string>
#include <unordered_map>
#include <vector>
#include <list>

namespace x0 {

class Value;
class Instr;
class BasicBlock;
class IRHandler;
class IRProgram;
class IRBuilder;

/**
 * Defines an immutable IR value.
 */
class X0_API Value {
public:
    Value(FlowType ty, const std::string& name = "");
    virtual ~Value();

    FlowType type() const { return type_; }
    void setType(FlowType ty) { type_ = ty; }

    const std::string& name() const { return name_; }
    void setName(const std::string& n) { name_ = n; }

    virtual void dump();

private:
    FlowType type_;
    std::string name_;
};

class X0_API Constant : public Value {
public:
    Constant(FlowType ty, size_t id, const std::string& name = "") :
        Value(ty, name), id_(id) {}

    size_t id() const { return id_; }

    virtual void dump();

private:
    int64_t id_;
};

class X0_API BasicBlock : public Value {
public:
    explicit BasicBlock(IRHandler* parent, const std::string& name = "");
    ~BasicBlock();

    IRHandler* parent() const { return parent_; }
    void setParent(IRHandler* handler) { parent_ = handler; }

private:
    IRHandler* parent_;
    std::vector<Instr*> code_;

    friend class IRBuilder;
};

// NCMPEQ, NADD, SMATCHR, JMP, EXIT, ...
class X0_API Instr : public Value {
public:
    Instr(BasicBlock* parent, FlowType ty, const std::vector<Value*>& ops = {}, const std::string& name = "");
    ~Instr();

    BasicBlock* parent() const { return parent_; }
    void setParent(BasicBlock* bb) { parent_ = bb; }

    const std::vector<Value*>& operands() const { return operands_; }
    std::vector<Value*>& operands() { return operands_; }

private:
    BasicBlock* parent_;
    std::vector<Value*> operands_;
};

class X0_API VmInstr : public Instr {
public:
    VmInstr(BasicBlock* parent, FlowVM::Opcode opcode, const std::vector<Value*>& ops = {}, const std::string& name = "");

    FlowVM::Opcode opcode() const { return opcode_; }

    virtual void dump();

private:
    FlowVM::Opcode opcode_;
};

/**
 * Creates a PHI (phoney) instruction.
 *
 * Creates a synthetic instruction that purely informs the target register allocator
 * to allocate the very same register for all given operands,
 * which is then used across all their basic blocks.
 */
class X0_API PhiNode : public Instr {
public:
    explicit PhiNode(BasicBlock* parent, const std::vector<Value*>& ops, const std::string& name = "");

    virtual void dump();
};

/**
 * Conditional branch instruction.
 *
 * Creates a terminate instruction that transfers control to one of the two
 * given alternate basic blocks, depending on the given input condition.
 */
class X0_API CondBrInstr : public Instr {
public:
    /**
     * Initializes the object.
     *
     * @param parent owning basic block this instruction is inserted to
     * @param cond input condition that (if true) causes \p trueBlock to be jumped to, \p falseBlock otherwise.
     * @param trueBlock basic block to run if input condition evaluated to true.
     * @param falseBlock basic block to run if input condition evaluated to false.
     */
    CondBrInstr(BasicBlock* parent, Value* cond, BasicBlock* trueBlock, BasicBlock* falseBlock, const std::string& name = "") :
        Instr(parent, FlowType::Void, {cond, trueBlock, falseBlock}, name)
    {}

    virtual void dump();
};

/**
 * Unconditional jump instruction.
 */
class X0_API BrInstr : public Instr {
public:
    BrInstr(BasicBlock* parent, BasicBlock* targetBlock, const std::string& name = "") :
        Instr(parent, FlowType::Void, {targetBlock}, name)
    {}

    virtual void dump();
};

/**
 * Match instruction, implementing the Flow match-keyword.
 */
class X0_API MatchInstr : public Instr {
public:
    MatchInstr(BasicBlock* parent, FlowVM::MatchClass op, const std::string& name = "");

    void setCondition(Value* condition);

    FlowVM::MatchClass op() const { return op_; }

    void addCase(Value* label, BasicBlock* code);
    const std::vector<std::pair<Value*, BasicBlock*>>& cases() const { return cases_; }
    std::vector<std::pair<Value*, BasicBlock*>>& cases() { return cases_; }

    BasicBlock* elseBlock() const { return elseBlock_; }
    void setElseBlock(BasicBlock* code) { elseBlock_ = code; }

private:
    FlowVM::MatchClass op_;
    BasicBlock* elseBlock_;
    std::vector<std::pair<Value*, BasicBlock*>> cases_;
};

class X0_API IRHandler : public Constant {
public:
    IRHandler(IRProgram* parent, size_t id, const std::string& name);
    ~IRHandler();

    BasicBlock* createBlock(const std::string& name = "");

    void setEntryPoint(BasicBlock* bb);
    BasicBlock* entryPoint() const { return entryPoint_; }

    IRProgram* parent() const { return parent_; }

private:
    IRProgram* parent_;
    BasicBlock* entryPoint_;
    std::list<BasicBlock*> blocks_;

    friend class IRBuilder;
};

class X0_API IRProgram {
public:
    IRProgram();
    ~IRProgram();

    void dump();

private:
    std::vector<std::pair<int64_t, Constant*>> numbers_;
    std::vector<std::pair<std::string, Constant*>> strings_;
    std::vector<std::pair<IPAddress, Constant*>> ipaddrs_;
    std::vector<std::pair<Cidr, Constant*>> cidrs_;
    std::vector<std::pair<RegExp, Constant*>> regexps_;
    std::vector<std::pair<std::string, IRHandler*>> handlers_;

    friend class IRBuilder;
};

class X0_API IRBuilder {
private:
    IRProgram* program_;
    IRHandler* handler_;
    BasicBlock* insertPoint_;

public:
    IRBuilder();
    ~IRBuilder();

    void setProgram(IRProgram* program);
    IRProgram* program() const { return program_; }

    void setHandler(IRHandler* hn);
    IRHandler* handler() const { return handler_; }

    BasicBlock* createBlock(const std::string& name = "");

    void setInsertPoint(BasicBlock* bb);
    BasicBlock* getInsertPoint() const { return insertPoint_; }

    Instr* insert(Instr* instr);

    // literals
    Value* get(int64_t number);
    Value* get(const std::string& ipaddr);
    Value* get(const IPAddress& ipaddr);
    Value* get(const Cidr& cidr);
    Value* get(const RegExp& regexp);
    IRHandler* getHandler(const std::string& name);

    // values
    Instr* createAlloca(FlowType ty, size_t arraySize = 1, const std::string& name = "");
    Instr* createLoad(Value* value, const std::string& name = "");
    Instr* createStore(Value* lhs, Value* rhs, const std::string& name = "");
    Instr* createPhi(const std::vector<Value*>& incomings);

    // binary data manipulation
    Value* createConvert(FlowType ty, Value* rhs, const std::string& name = "");
    Value* createAdd(Value* lhs, Value* rhs, const std::string& name = "");   // +
    Value* createSub(Value* lhs, Value* rhs, const std::string& name = "");   // -
    Value* createMul(Value* lhs, Value* rhs, const std::string& name = "");   // *
    Value* createDiv(Value* lhs, Value* rhs, const std::string& name = "");   // /
    Value* createRem(Value* lhs, Value* rhs, const std::string& name = "");   // %
    Value* createShl(Value* lhs, Value* rhs, const std::string& name = "");   // <<
    Value* createShr(Value* lhs, Value* rhs, const std::string& name = "");   // >>
    Value* createPow(Value* lhs, Value* rhs, const std::string& name = "");   // **
    Value* createAnd(Value* lhs, Value* rhs, const std::string& name = "");   // &
    Value* createOr(Value* lhs, Value* rhs, const std::string& name = "");    // |
    Value* createXor(Value* lhs, Value* rhs, const std::string& name = "");   // ^

    Value* createCmpEQ(Value* lhs, Value* rhs, const std::string& name = ""); // ==
    Value* createCmpNE(Value* lhs, Value* rhs, const std::string& name = ""); // !=
    Value* createCmpLE(Value* lhs, Value* rhs, const std::string& name = ""); // <=
    Value* createCmpGE(Value* lhs, Value* rhs, const std::string& name = ""); // >=
    Value* createCmpLT(Value* lhs, Value* rhs, const std::string& name = ""); // <
    Value* createCmpGT(Value* lhs, Value* rhs, const std::string& name = ""); // >
    Value* createCmpRE(Value* lhs, Value* rhs, const std::string& name = ""); // =~
    Value* createCmpEB(Value* lhs, Value* rhs, const std::string& name = ""); // =^
    Value* createCmpEE(Value* lhs, Value* rhs, const std::string& name = ""); // =$

    // unary data manipulation
    Value* createNeg(Value* rhs, const std::string& name = ""); // -
    Value* createNot(Value* rhs, const std::string& name = ""); // ~ (int) ! (bool)

    // calls
    Instr* createCallFunction(Value* callee /*, ...*/);
    Instr* createInvokeHandler(Value* callee /*, ...*/);

    // exit points
    Instr* createRet(Value* result, const std::string& name = "");
    Instr* createBr(BasicBlock* block);
    Instr* createCondBr(Value* condValue, BasicBlock* trueBlock, BasicBlock* falseBlock, const std::string& name = "");
    Value* createMatchSame(Value* cond, size_t matchId, const std::string& name = "");
    Value* createMatchHead(Value* cond, size_t matchId, const std::string& name = "");
    Value* createMatchTail(Value* cond, size_t matchId, const std::string& name = "");
    Value* createMatchRegExp(Value* cond, size_t matchId, const std::string& name = "");
};

class IRGenerator :
    public IRBuilder,
    public ASTVisitor
{
public:
    IRGenerator();
    ~IRGenerator();

    static IRProgram* generate(Unit* unit);

private:
    Value* result_;

    Value* generate(Expr* expr);
    Value* generate(Stmt* stmt);
    Value* generate(Symbol* sym);

private:
    // symbols
    virtual void accept(Unit& symbol);
    virtual void accept(Variable& variable);
    virtual void accept(Handler& handler);
    virtual void accept(BuiltinFunction& symbol);
    virtual void accept(BuiltinHandler& symbol);

    // expressions
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

    // statements
    virtual void accept(ExprStmt& stmt);
    virtual void accept(CompoundStmt& stmt);
    virtual void accept(CondStmt& stmt);
    virtual void accept(MatchStmt& stmt);
    virtual void accept(AssignStmt& stmt);
    virtual void accept(HandlerCall& stmt);

    // error handling
    void reportError(const std::string& message);
    template<typename... Args> void reportError(const std::string& fmt, Args&&...);
};

// {{{ IRGenerator inlines
template<typename... Args>
inline void IRGenerator::reportError(const std::string& fmt, Args&&... args)
{
    char buf[1024];
    ssize_t n = snprintf(buf, sizeof(buf), fmt.c_str(), std::forward<Args>(args)...);

    if (n > 0) {
        reportError(buf);
    }
}
// }}}

} // namespace x0
