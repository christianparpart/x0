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
#include <x0/flow/vm/Signature.h>
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

    void addUse(Instr* user);
    const std::vector<Instr*>& uses() const { return uses_; }

    virtual void dump() = 0;

private:
    FlowType type_;
    std::string name_;

    std::vector<Instr*> uses_; //! list of instructions that <b>use</b> this value.
};

class X0_API IRVariable : public Value {
public:
    explicit IRVariable(FlowType ty, const std::string& name = "") :
        Value(ty, name)
        {}

    void dump() override;
};

class X0_API Constant : public Value {
public:
    Constant(FlowType ty, size_t id, const std::string& name = "") :
        Value(ty, name), id_(id) {}

    size_t id() const { return id_; }

    void dump() override;

private:
    int64_t id_;
};

class X0_API IRBuiltinFunction : public Constant {
public:
    IRBuiltinFunction(size_t id, const FlowVM::Signature& sig) :
        Constant(sig.returnType(), id, sig.name()),
        signature_(sig)
    {}

    const FlowVM::Signature& signature() const { return signature_; }
    const FlowVM::Signature& get() const { return signature_; }

private:
    FlowVM::Signature signature_;
};

template<typename T, const FlowType Ty>
class X0_API ConstantValue : public Constant {
public:
    ConstantValue(size_t id, const T& value, const std::string& name = "") :
        Constant(Ty, id, name),
        value_(value)
        {}

    T get() const { return value_; }

private:
    T value_;
};

typedef ConstantValue<int64_t, FlowType::Number> ConstantInt;
typedef ConstantValue<bool, FlowType::Boolean> ConstantBoolean;
typedef ConstantValue<std::string, FlowType::String> ConstantString;
typedef ConstantValue<IPAddress, FlowType::IPAddress> ConstantIP;
typedef ConstantValue<Cidr, FlowType::Cidr> ConstantCidr;
typedef ConstantValue<RegExp, FlowType::RegExp> ConstantRegExp;

class X0_API BasicBlock : public Value {
public:
    explicit BasicBlock(const std::string& name = "");
    ~BasicBlock();

    IRHandler* parent() const { return parent_; }
    void setParent(IRHandler* handler) { parent_ = handler; }

    const std::vector<Instr*>& instructions() const { return code_; }
    std::vector<Instr*>& instructions() { return code_; }

    void dump() override;

    /** Retrieves all predecessors of given basic block. */
    std::vector<BasicBlock*>& predecessors() { return predecessors_; }

    /** Retrieves all uccessors of the given basic block. */
    std::vector<BasicBlock*>& successors() { return successors_; }

    /** Retrieves all dominators of given basic block. */
    std::vector<BasicBlock*> dominators();

    /** Retrieves all immediate dominators of given basic block. */
    std::vector<BasicBlock*> immediateDominators();

private:
    void collectIDom(std::vector<BasicBlock*>& output);

private:
    IRHandler* parent_;
    std::vector<Instr*> code_;
    std::vector<BasicBlock*> predecessors_;
    std::vector<BasicBlock*> successors_;

    friend class IRBuilder;
    friend class Instr;
};

class InstructionVisitor;

// NCMPEQ, NADD, SMATCHR, JMP, EXIT, ...
class X0_API Instr : public Value {
public:
    Instr(FlowType ty, const std::vector<Value*>& ops = {}, const std::string& name = "");
    ~Instr();

    BasicBlock* parent() const { return parent_; }
    void setParent(BasicBlock* bb) { parent_ = bb; }

    const std::vector<Value*>& operands() const { return operands_; }
    std::vector<Value*>& operands() { return operands_; }

    virtual void accept(InstructionVisitor& v) = 0;

protected:
    void dumpOne(const char* mnemonic);

private:
    BasicBlock* parent_;
    std::vector<Value*> operands_;
};

/**
 * Allocates an array of given type and elements.
 */
class X0_API AllocaInstr : public Instr {
private:
    static FlowType computeType(FlowType elementType, Value* size) {
        if (auto n = dynamic_cast<ConstantInt*>(size)) {
            if (n->get() == 1)
                return elementType;
        }

        switch (elementType) {
            case FlowType::Number:
                return FlowType::IntArray;
            case FlowType::String:
                return FlowType::StringArray;
            default:
                return FlowType::Void;
        }
    }

public:
    AllocaInstr(FlowType ty, Value* n, const std::string& name = "") :
        Instr(
            computeType(ty, n),
            /*ty == FlowType::Number
                ? FlowType::IntArray
                : (ty == FlowType::String
                    ? FlowType::StringArray
                    : FlowType::Void),*/
            {n},
            name)
    {
    }

    FlowType elementType() const {
        switch (type()) {
            case FlowType::StringArray:
                return FlowType::String;
            case FlowType::IntArray:
                return FlowType::Number;
            default:
                return FlowType::Void;
        }
    }

    Value* arraySize() const { return operands()[0]; }

    void dump() override;

    virtual void accept(InstructionVisitor& v);
};

class X0_API ArraySetInstr : public Instr {
public:
    ArraySetInstr(Value* array, Value* index, Value* value, const std::string& name = "") :
        Instr(FlowType::Void, {array, index, value}, name)
        {}

    Value* array() const { return operands()[0]; }
    Value* index() const { return operands()[1]; }
    Value* value() const { return operands()[2]; }

    void dump() override;

    virtual void accept(InstructionVisitor& v);
};

class X0_API StoreInstr : public Instr {
public:
    StoreInstr(Value* variable, Value* expression, const std::string& name = "") :
        Instr(FlowType::Void, {variable, expression}, name)
        {}

    Value* variable() const { return operands()[0]; }
    Value* expression() const { return operands()[1]; }

    void dump() override;

    virtual void accept(InstructionVisitor& v);
};

class X0_API LoadInstr : public Instr {
public:
    LoadInstr(Value* variable, const std::string& name = "") :
        Instr(variable->type(), {variable}, name)
        {}

    Value* variable() const { return operands()[0]; }

    void dump() override;

    virtual void accept(InstructionVisitor& v);
};

class X0_API CallInstr : public Instr {
public:
    CallInstr(IRBuiltinFunction* callee, const std::vector<Value*>& args, const std::string& name = "");

    IRBuiltinFunction* callee() const { return (IRBuiltinFunction*) operands()[0]; }

    void dump() override;

    virtual void accept(InstructionVisitor& v);
};

class X0_API VmInstr : public Instr {
public:
    VmInstr(FlowVM::Opcode opcode, const std::vector<Value*>& ops = {}, const std::string& name = "");

    void setOpcode(FlowVM::Opcode opc) { opcode_ = opc; }
    FlowVM::Opcode opcode() const { return opcode_; }

    void dump() override;

    virtual void accept(InstructionVisitor& v);

private:
    FlowVM::Opcode opcode_;
};

class X0_API UnaryInstr : public Instr {
public:
    UnaryInstr(FlowVM::Opcode opcode, Value* op, const std::string& name = "") :
        Instr(FlowVM::resultType(opcode), {op}, name),
        opcode_(opcode)
        {}

    FlowVM::Opcode opcode() const { return opcode_; }

    void dump() override;

    virtual void accept(InstructionVisitor& v);

private:
    FlowVM::Opcode opcode_;
};

class X0_API BinaryInstr : public Instr {
public:
    BinaryInstr(FlowVM::Opcode opcode, Value* lhs, Value* rhs, const std::string& name = "") :
        Instr(FlowVM::resultType(opcode), {lhs, rhs}, name),
        opcode_(opcode)
        {}

    FlowVM::Opcode opcode() const { return opcode_; }

    void dump() override;

    virtual void accept(InstructionVisitor& v);

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
    explicit PhiNode(const std::vector<Value*>& ops, const std::string& name = "");

    void dump() override;

    virtual void accept(InstructionVisitor& v);
};

class X0_API BranchInstr : public Instr {
public:
    BranchInstr(const std::vector<Value*>& ops = {}, const std::string& name = "") :
        Instr(FlowType::Void, ops, name)
        {}

    virtual void accept(InstructionVisitor& v);
};

/**
 * Conditional branch instruction.
 *
 * Creates a terminate instruction that transfers control to one of the two
 * given alternate basic blocks, depending on the given input condition.
 */
class X0_API CondBrInstr : public BranchInstr {
public:
    /**
     * Initializes the object.
     *
     * @param cond input condition that (if true) causes \p trueBlock to be jumped to, \p falseBlock otherwise.
     * @param trueBlock basic block to run if input condition evaluated to true.
     * @param falseBlock basic block to run if input condition evaluated to false.
     */
    CondBrInstr(Value* cond, BasicBlock* trueBlock, BasicBlock* falseBlock, const std::string& name = "") :
        BranchInstr({cond, trueBlock, falseBlock}, name)
    {}

    void dump() override;

    Value* condition() const { return operands()[0]; }
    BasicBlock* trueBlock() const { return (BasicBlock*) operands()[1]; }
    BasicBlock* falseBlock() const { return (BasicBlock*) operands()[2]; }

    virtual void accept(InstructionVisitor& v);
};

/**
 * Unconditional jump instruction.
 */
class X0_API BrInstr : public BranchInstr {
public:
    BrInstr(BasicBlock* targetBlock, const std::string& name = "") :
        BranchInstr({targetBlock}, name)
    {}

    void dump() override;

    BasicBlock* targetBlock() const { return (BasicBlock*) operands()[0]; }

    virtual void accept(InstructionVisitor& v);
};

/**
 * handler-return instruction.
 */
class X0_API RetInstr : public Instr {
public:
    explicit RetInstr(Value* result, const std::string& name = "") :
        Instr(FlowType::Void, {result}, name)
        {}

    void dump() override;

    virtual void accept(InstructionVisitor& v);
};

/**
 * Match instruction, implementing the Flow match-keyword.
 */
class X0_API MatchInstr : public Instr {
public:
    MatchInstr(FlowVM::MatchClass op, Value* cond, const std::string& name = "");

    FlowVM::MatchClass op() const { return op_; }

    void addCase(Constant* label, BasicBlock* code);
    std::vector<std::pair<Constant*, BasicBlock*>>& cases() { return cases_; }

    BasicBlock* elseBlock() const { return elseBlock_; }
    void setElseBlock(BasicBlock* code);

    void dump() override;

    virtual void accept(InstructionVisitor& v);

private:
    FlowVM::MatchClass op_;
    std::vector<std::pair<Constant*, BasicBlock*>> cases_;
    BasicBlock* elseBlock_;
};

class X0_API IRHandler : public Constant {
public:
    IRHandler(size_t id, const std::string& name);
    ~IRHandler();

    BasicBlock* createBlock(const std::string& name = "");

    BasicBlock* setEntryPoint(BasicBlock* bb);
    BasicBlock* entryPoint() const { return entryPoint_; }

    IRProgram* parent() const { return parent_; }
    void setParent(IRProgram* prog) { parent_ = prog; }

    void dump() override;

    const std::list<BasicBlock*>& basicBlocks() const { return blocks_; }

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

    ConstantInt* get(int64_t literal) { return get<ConstantInt>(numbers_, literal); }
    ConstantString* get(const std::string& literal) { return get<ConstantString>(strings_, literal); }
    ConstantIP* get(const IPAddress& literal) { return get<ConstantIP>(ipaddrs_, literal); }
    ConstantCidr* get(const Cidr& literal) { return get<ConstantCidr>(cidrs_, literal); }
    ConstantRegExp* get(const RegExp& literal) { return get<ConstantRegExp>(regexps_, literal); }

    IRBuiltinFunction* get(const FlowVM::Signature& sig) { return get<IRBuiltinFunction>(builtinFunctions_, sig); }

    template<typename T, typename U>
    T* get(std::vector<T*>& table, const U& literal);

    void addImport(const std::string& name, const std::string& path) { imports_.push_back(std::make_pair(name, path)); }

	const std::vector<std::pair<std::string, std::string>>& imports() const { return imports_; }
    const std::vector<IRHandler*>& handlers() const { return handlers_; }

private:
	std::vector<std::pair<std::string, std::string> > imports_;
    std::vector<ConstantInt*> numbers_;
    std::vector<ConstantString*> strings_;
    std::vector<ConstantIP*> ipaddrs_;
    std::vector<ConstantCidr*> cidrs_;
    std::vector<ConstantRegExp*> regexps_;
    std::vector<IRBuiltinFunction*> builtinFunctions_;
    std::vector<IRHandler*> handlers_;

    friend class IRBuilder;
};

class X0_API IRBuilder {
private:
    IRProgram* program_;
    IRHandler* handler_;
    BasicBlock* insertPoint_;
    std::unordered_map<std::string, unsigned long> nameStore_;

public:
    IRBuilder();
    ~IRBuilder();

    std::string makeName(const std::string& name);

    void setProgram(IRProgram* program);
    IRProgram* program() const { return program_; }

    IRHandler* setHandler(IRHandler* hn);
    IRHandler* handler() const { return handler_; }

    BasicBlock* createBlock(const std::string& name);

    void setInsertPoint(BasicBlock* bb);
    BasicBlock* getInsertPoint() const { return insertPoint_; }

    Instr* insert(Instr* instr);

    IRHandler* getHandler(const std::string& name);

    // literals
    ConstantInt* get(int64_t literal) { return program_->get(literal); }
    ConstantString* get(const std::string& literal) { return program_->get(literal); }
    ConstantIP* get(const IPAddress& literal) { return program_->get(literal); }
    ConstantCidr* get(const Cidr& literal) { return program_->get(literal); }
    ConstantRegExp* get(const RegExp& literal) { return program_->get(literal); }
    IRBuiltinFunction* get(const FlowVM::Signature& sig) { return program_->get(sig); }

    // values
    AllocaInstr* createAlloca(FlowType ty, Value* arraySize, const std::string& name = "");
    Instr* createArraySet(Value* array, Value* index, Value* value, const std::string& name = "");
    Value* createLoad(Value* value, const std::string& name = "");
    Instr* createStore(Value* lhs, Value* rhs, const std::string& name = "");
    Instr* createPhi(const std::vector<Value*>& incomings, const std::string& name = "");

    // numerical operations
    Value* createNeg(Value* rhs, const std::string& name = "");                  // -
    Value* createAdd(Value* lhs, Value* rhs, const std::string& name = "");      // +
    Value* createSub(Value* lhs, Value* rhs, const std::string& name = "");      // -
    Value* createMul(Value* lhs, Value* rhs, const std::string& name = "");      // *
    Value* createDiv(Value* lhs, Value* rhs, const std::string& name = "");      // /
    Value* createRem(Value* lhs, Value* rhs, const std::string& name = "");      // %
    Value* createShl(Value* lhs, Value* rhs, const std::string& name = "");      // <<
    Value* createShr(Value* lhs, Value* rhs, const std::string& name = "");      // >>
    Value* createPow(Value* lhs, Value* rhs, const std::string& name = "");      // **
    Value* createAnd(Value* lhs, Value* rhs, const std::string& name = "");      // &
    Value* createOr(Value* lhs, Value* rhs, const std::string& name = "");       // |
    Value* createXor(Value* lhs, Value* rhs, const std::string& name = "");      // ^
    Value* createNCmpEQ(Value* lhs, Value* rhs, const std::string& name = "");   // ==
    Value* createNCmpNE(Value* lhs, Value* rhs, const std::string& name = "");   // !=
    Value* createNCmpLE(Value* lhs, Value* rhs, const std::string& name = "");   // <=
    Value* createNCmpGE(Value* lhs, Value* rhs, const std::string& name = "");   // >=
    Value* createNCmpLT(Value* lhs, Value* rhs, const std::string& name = "");   // <
    Value* createNCmpGT(Value* lhs, Value* rhs, const std::string& name = "");   // >

    // string ops
    Value* createSAdd(Value* lhs, Value* rhs, const std::string& name = "");     // +
    Value* createSCmpEQ(Value* lhs, Value* rhs, const std::string& name = "");   // ==
    Value* createSCmpNE(Value* lhs, Value* rhs, const std::string& name = "");   // !=
    Value* createSCmpLE(Value* lhs, Value* rhs, const std::string& name = "");   // <=
    Value* createSCmpGE(Value* lhs, Value* rhs, const std::string& name = "");   // >=
    Value* createSCmpLT(Value* lhs, Value* rhs, const std::string& name = "");   // <
    Value* createSCmpGT(Value* lhs, Value* rhs, const std::string& name = "");   // >
    Value* createSCmpRE(Value* lhs, Value* rhs, const std::string& name = "");   // =~
    Value* createSCmpEB(Value* lhs, Value* rhs, const std::string& name = "");   // =^
    Value* createSCmpEE(Value* lhs, Value* rhs, const std::string& name = "");   // =$

    // cast
    Value* createConvert(FlowType ty, Value* rhs, const std::string& name = ""); // cast<T>()
    Value* createB2S(Value* rhs, const std::string& name = "");
    Value* createI2S(Value* rhs, const std::string& name = "");
    Value* createP2S(Value* rhs, const std::string& name = "");
    Value* createC2S(Value* rhs, const std::string& name = "");
    Value* createR2S(Value* rhs, const std::string& name = "");
    Value* createS2I(Value* rhs, const std::string& name = "");

    // calls
    Instr* createCallFunction(IRBuiltinFunction* callee, const std::vector<Value*>& args, const std::string& name = "");
    Instr* createInvokeHandler(const std::vector<Value*>& args, const std::string& name = "");

    // termination instructions
    Instr* createRet(Value* result, const std::string& name = "");
    Instr* createBr(BasicBlock* block);
    Instr* createCondBr(Value* condValue, BasicBlock* trueBlock, BasicBlock* falseBlock, const std::string& name = "");
    MatchInstr* createMatch(FlowVM::MatchClass opc, Value* cond, const std::string& name = "");
    Value* createMatchSame(Value* cond, const std::string& name = "");
    Value* createMatchHead(Value* cond, const std::string& name = "");
    Value* createMatchTail(Value* cond, const std::string& name = "");
    Value* createMatchRegExp(Value* cond, const std::string& name = "");
};

} // namespace x0
