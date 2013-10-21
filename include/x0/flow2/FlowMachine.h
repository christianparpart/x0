#pragma once

#include <x0/sysconfig.h>
#include <x0/flow2/AST.h>
#include <x0/flow2/ASTVisitor.h>
#include <x0/flow2/FlowValue.h>
#include <x0/flow2/FlowToken.h>

#if defined(LLVM_VERSION_3_3)
#  include <llvm/IR/DerivedTypes.h>
#  include <llvm/IR/LLVMContext.h>
#  include <llvm/IR/Module.h>
#  include <llvm/IR/IRBuilder.h>
#  include <llvm/Analysis/Verifier.h>
#else // something older
#  include <llvm/DerivedTypes.h>
#  include <llvm/LLVMContext.h>
#  include <llvm/Module.h>
#  include <llvm/Analysis/Verifier.h>
#  include <llvm/Support/IRBuilder.h>
#endif

#include <functional>
#include <cstdio>
#include <deque>
#include <unordered_map>

namespace llvm {
	class ExecutionEngine;
	class PassManager;
	class FunctionPassManager;
}

namespace x0 {

class X0_API FlowMachine :
	public ASTVisitor
{
	class Scope;

public:
	explicit FlowMachine(FlowBackend* backend);
	~FlowMachine();

	static void shutdown();

	void dump();
	void clear();

	bool compile(Unit* unit);

	FlowValue::Handler findHandler(const std::string& name);

private:
	bool prepare();
	int findNative(const std::string& name) const;
	void emitInitializerTail();
	Scope& scope() const { return *scope_; }

	// error handling
	void reportError(const std::string& message);
	template<typename... Args> void reportError(const std::string& fmt, Args ...);

	// code generation entries
	llvm::Value* codegen(Expr* expr);
	llvm::Value* codegen(Symbol* stmt);
	void codegen(Stmt* stmt);

	// code generation helper
	llvm::Value* toBool(llvm::Value* value);
	llvm::Type* boolType() const;
	llvm::Type* int8Type() const;
	llvm::Type* int16Type() const;
	llvm::Type* int32Type() const;
	llvm::Type* int64Type() const;
	llvm::Type* numberType() const;

	llvm::Type* int8PtrType() const;

	// AST code generation
	virtual void visit(Variable& variable);
	virtual void visit(Handler& handler);
	virtual void visit(BuiltinFunction& symbol);
	virtual void visit(BuiltinHandler& symbol);
	virtual void visit(Unit& symbol);
	virtual void visit(UnaryExpr& expr);
	virtual void visit(BinaryExpr& expr);
	virtual void visit(FunctionCallExpr& expr);
	virtual void visit(VariableExpr& expr);
	virtual void visit(HandlerRefExpr& expr);
	virtual void visit(ListExpr& expr);
	virtual void visit(StringExpr& expr);
	virtual void visit(NumberExpr& expr);
	virtual void visit(BoolExpr& expr);
	virtual void visit(RegExpExpr& expr);
	virtual void visit(IPAddressExpr& expr);
	virtual void visit(CidrExpr& cidr);
	virtual void visit(ExprStmt& stmt);
	virtual void visit(CompoundStmt& stmt);
	virtual void visit(CondStmt& stmt);
	virtual void visit(AssignStmt& stmt);
	virtual void visit(HandlerCallStmt& stmt);
	virtual void visit(BuiltinHandlerCallStmt& stmt);

private:
	int optimizationLevel_;
	FlowBackend* backend_;
	Scope* scope_;

	mutable llvm::LLVMContext cx_;
	llvm::Module* module_;

	llvm::ExecutionEngine* executionEngine_;
	llvm::PassManager* modulePassMgr_;
	llvm::FunctionPassManager* functionPassMgr_;

	llvm::Type* valuePtrType_;
	llvm::StructType* valueType_;
	llvm::StructType* regexType_;
	llvm::StructType* arrayType_;
	llvm::StructType* ipaddrType_;
	llvm::StructType* cidrType_;
	llvm::StructType* bufferType_;

	std::vector<llvm::Function*> coreFunctions_;

	llvm::IRBuilder<> builder_;
	llvm::Value* value_;
	llvm::Function* initializerFn_;
	llvm::BasicBlock* initializerBB_;

	std::vector<llvm::Function *> functions_;
};

// {{{ class FlowMachine::Scope
class FlowMachine::Scope
{
private:
	std::deque<std::unordered_map<Symbol*, llvm::Value*>*> scope_;

public:
	Scope();
	~Scope();

	void clear();

	void enter();
	void leave();

	llvm::Value* lookup(Symbol* symbol) const;
	void insert(Symbol* symbol, llvm::Value* value);
	void insertGlobal(Symbol* symbol, llvm::Value* value);
	void remove(Symbol* symbol);
};
// }}}

//{{{ FlowMachine inlines
template<typename... Args>
inline void FlowMachine::reportError(const std::string& fmt, Args... args)
{
	char buf[1024];
	ssize_t n = snprintf(buf, sizeof(buf), fmt.c_str(), std::forward(args)...);

	if (n > 0) {
		reportError(buf);
	}
}
//}}}

} // namespace x0
