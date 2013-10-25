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
	template<typename... Args> void reportError(const std::string& fmt, Args&&...);

	// code generation entries
	llvm::Value* codegen(Expr* expr);
	llvm::Value* codegen(Symbol* stmt);
	void codegen(Stmt* stmt);

	// CG: casts
	llvm::Value* toBool(llvm::Value* value);

	// IR types
	llvm::Type* boolType() const { return llvm::Type::getInt1Ty(cx_); }
	llvm::Type* int8Type() const { return llvm::Type::getInt8Ty(cx_); }
	llvm::Type* int16Type() const { return llvm::Type::getInt16Ty(cx_); }
	llvm::Type* int32Type() const { return llvm::Type::getInt32Ty(cx_); }
	llvm::Type* int64Type() const { return llvm::Type::getInt64Ty(cx_); }
	llvm::Type* numberType() const { return int64Type(); }
	llvm::Type* int8PtrType() const { return llvm::Type::getInt8PtrTy(cx_); }

	llvm::Type* arrayType() const { return arrayType_; }

	// type checks
	bool isBool(llvm::Type* type) const;
	bool isBool(llvm::Value* value) const;

	bool isInteger(llvm::Value* value) const;
	bool isNumber(llvm::Value* v) const;

	bool isCString(llvm::Value* value) const;
	bool isCString(llvm::Type* type) const;

	bool isString(llvm::Value* value) const;
	bool isString(llvm::Type* type) const;

	bool isBuffer(llvm::Value* value) const;
	bool isBuffer(llvm::Type* type) const;

	bool isBufferPtr(llvm::Value* value) const;
	bool isBufferPtr(llvm::Type* type) const;

	bool isRegExp(llvm::Value* value) const;
	bool isIPAddress(llvm::Value* value) const;
	bool isFunctionPtr(llvm::Value* value) const;

	bool isArray(llvm::Value* value) const;
	bool isArray(llvm::Type* type) const;

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
	virtual void visit(CallStmt& stmt);

	void emitOpBoolBool(FlowToken op, llvm::Value* left, llvm::Value* right);
	void emitOpIntInt(FlowToken op, llvm::Value* left, llvm::Value* right);
	void emitOpStrStr(FlowToken op, llvm::Value* left, llvm::Value* right);
	void emitNativeValue(size_t index, llvm::Value* target, llvm::Value* source, const std::string& name = "");
	void emitCall(Callable* callee, ListExpr* argList);
	llvm::Value* emitToValue(llvm::Value* rhs, const std::string& name);
	llvm::Value* emitNativeValue(int index, llvm::Value* lhs, llvm::Value* rhs, const std::string& name);

	void setHandlerUserData(llvm::Value* value) { userdata_ = value; }
	llvm::Value* handlerUserData() const { return userdata_; }

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
	llvm::Value* userdata_;

	llvm::IRBuilder<> builder_;
	llvm::Value* value_;
	size_t listSize_;
	llvm::Function* initializerFn_;
	llvm::BasicBlock* initializerBB_;
	bool requestingLvalue_;

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
inline void FlowMachine::reportError(const std::string& fmt, Args&&... args)
{
	char buf[1024];
	ssize_t n = snprintf(buf, sizeof(buf), fmt.c_str(), std::forward<Args>(args)...);

	if (n > 0) {
		reportError(buf);
	}
}
//}}}

} // namespace x0
