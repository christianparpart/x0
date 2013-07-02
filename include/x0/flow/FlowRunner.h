/* <flow/FlowRunner.h>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://redmine.xzero.io/projects/flow
 *
 * (c) 2009-2013 Christian Parpart <trapni@gmail.com>
 */

#ifndef sw_flow_runner_h
#define sw_flow_runner_h

#include <x0/sysconfig.h>
#include <x0/flow/Flow.h>
#include <x0/flow/FlowValue.h>
#include <x0/flow/FlowToken.h>

#if defined(LLVM_VERSION_3_3)
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/Analysis/Verifier.h>
#else // something older
#include <llvm/DerivedTypes.h>
#include <llvm/LLVMContext.h>
#include <llvm/Module.h>
#include <llvm/Analysis/Verifier.h>
#include <llvm/Support/IRBuilder.h>
#endif

#include <functional>
#include <cstdio>
#include <deque>
#include <map>

namespace llvm {
	class FunctionPassManager;
	class PassManager;
	class ExecutionEngine;
	class Type;
}

namespace x0 {

class FlowParser;
class FlowBackend;
class Unit;

typedef llvm::Type Type;

class X0_API FlowRunner :
	public ASTVisitor
{
public:
	typedef bool (*HandlerFunction)(void *);

private:
	enum class CF // {{{
	{
		native, // for native callbacks

		// string compare operators
		strlen,
		strcasecmp,
		strncasecmp,
		strcasestr,
		strcmp,
		strncmp,
		regexmatch,
		regexmatch2,

		endsWith,
		pow,
		strcat,
		strcpy,
		memcpy,

		arraylen,
		arrayadd,
		arraycmp,

		NumberInArray,
		StringInArray,

		ipstrcmp, // compare(IPAddress, String)
		ipcmp,    // compare(IPAddress, IPAddress)

		// conversion
		bool2str,
		int2str,
		str2int,
		buf2int,

		COUNT // synthetic end of enumeration
	}; // }}}

	class Scope // {{{
	{
	private:
		std::deque<std::map<Symbol *, llvm::Value *> *> scope_;

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
	}; // }}}

private:
	FlowBackend* backend_;
	FlowParser* parser_;
	Unit* unit_;
	size_t listSize_;

	int optimizationLevel_;
	std::function<void(const std::string&)> errorHandler_;

	mutable llvm::LLVMContext cx_;

	llvm::Module* module_;
	llvm::StructType* valueType_;
	llvm::StructType* regexpType_;
	llvm::StructType* arrayType_;
	llvm::StructType* ipaddrType_;
	llvm::StructType* bufferType_;
	llvm::Function* coreFunctions_[static_cast<size_t>(CF::COUNT)];
	llvm::IRBuilder<> builder_;
	llvm::Value* value_;
	llvm::Function* initializerFn_;
	llvm::BasicBlock* initializerBB_;

	Scope scope_;

	bool requestingLvalue_;

	llvm::FunctionPassManager* functionPassMgr_;
	llvm::PassManager* modulePassMgr_;
	llvm::ExecutionEngine* executionEngine_;

	std::vector<llvm::Function *> functions_;

public:
	explicit FlowRunner(FlowBackend* backend);
	~FlowRunner();

	static void initialize();
	static void shutdown();

	void clear();
	void reset();

	void dump(const char* msg = NULL);

	int optimizationLevel() const;
	void setOptimizationLevel(int value);

	void setErrorHandler(std::function<void(const std::string&)>&& callback);

	// execution
	bool open(const std::string& filename, std::istream* stream = nullptr);
	std::vector<Function*> getHandlerList() const;
	Function* findHandler(const std::string& name) const;
	HandlerFunction getPointerTo(Function* handler);
	bool invoke(Function* handler, void* data = NULL);
	void close();

	// {{{ types
	Type* stringType() const;
	Type* numberType() const;
	Type* boolType() const;
	Type* voidType() const;
	Type* bufferType() const;
	Type* arrayType() const;
	Type* regexpType() const;
	Type* ipaddrType() const;

	Type* int8Type() const;
	Type* int16Type() const;
	Type* int32Type() const;
	Type* int64Type() const;
	Type* doubleType() const;

	Type* int8PtrType() const;
	// }}}

private:
	bool reinitialize();

	void emitInitializerHead();
	void emitInitializerTail();

	int findNative(const std::string& name) const;

	llvm::Type* makeType(FlowToken t) const;

	void setHandlerUserData(llvm::Value* value) { scope_.insert(nullptr, value); }
	llvm::Value* handlerUserData() const { return scope_.lookup(nullptr); }

	// buffer API
	llvm::Value* emitAllocaBuffer(llvm::Value* length, llvm::Value* data, const std::string& name = "");
	llvm::Value* emitLoadBufferLength(llvm::Value* nstr);
	llvm::Value* emitLoadBufferData(llvm::Value* nstr);
	llvm::Value* emitStoreBufferLength(llvm::Value* nstr, llvm::Value* length);
	llvm::Value* emitStoreBufferData(llvm::Value* nstr, llvm::Value* data);
	llvm::Value* emitStoreBuffer(llvm::Value* nstr, llvm::Value* length, llvm::Value* data);
	llvm::Value* emitCastNumberToString(llvm::Value* number);
	llvm::Value* emitCastBoolToString(llvm::Value* number);
	bool isBufferTy(llvm::Type* nbuf) const;
	bool isBuffer(llvm::Value* nbuf) const;
	bool isBufferPtrTy(llvm::Type* nbuf) const;
	bool isBufferPtr(llvm::Value* nbuf) const;

	// C-string helper
	bool isCStringTy(llvm::Type* nstr) const;
	bool isCString(llvm::Value* nstr) const;

	//! \return true if v1 and v2 is C-string (i8*) OR a string buffer (%nbuf*)
	bool isStringPair(llvm::Value* v1, llvm::Value* v2) const;

	//! \return true if v1 is either a C-string (i8*) or a string buffer (%nbuf*)
	bool isString(llvm::Value* v) const;
	bool isNumber(llvm::Value* v) const;
	bool isRegExp(llvm::Value* value) const;
	bool isIPAddress(llvm::Value* value) const;
	bool isFunctionPtr(llvm::Value* value) const;

	// array helper
	bool isArray(llvm::Value* value) const;
	bool isArray(llvm::Type* type) const;
	llvm::Value* emitLoadArrayLength(llvm::Value* array);

	// string ops
	llvm::Value* emitCmpString(Operator op, llvm::Value* left, llvm::Value* right);
	llvm::Value* emitCmpString(llvm::Value* len1, llvm::Value* buf1, llvm::Value* len2, llvm::Value* buf2);
	llvm::Value* emitStrCaseStr(llvm::Value* haystack, llvm::Value* needle);
	llvm::Value* emitIsSubString(llvm::Value* haystack, llvm::Value* needle);
	llvm::Value* emitStringCat(llvm::Value* v1, llvm::Value* v2);
	llvm::Value* emitPrefixMatch(llvm::Value* left, llvm::Value* right);
	llvm::Value* emitSuffixMatch(llvm::Value* left, llvm::Value* right);
	llvm::Value* emitLoadStringLength(llvm::Value* value);
	llvm::Value* emitLoadStringBuffer(llvm::Value* value);

	// helper ops
	llvm::Value* emitToLower(llvm::Value* value); // like std::tolower()
	llvm::Value* emitGetUMin(llvm::Value* len1, llvm::Value* len2); // like std::min()

	// core-function API
	void emitCoreFunctions();
	void emitCoreFunction(CF id, const std::string& name, Type* rt, Type* p1, bool isVaArg);
	void emitCoreFunction(CF id, const std::string& name, Type* rt, Type* p1, Type* p2, bool isVaArg);
	void emitCoreFunction(CF id, const std::string& name, Type* rt, Type* p1, Type* p2, Type* p3, bool isVaArg);
	void emitCoreFunction(CF id, const std::string& name, Type* rt, Type* p1, Type* p2, Type* p3, Type* p4, bool isVaArg);
	void emitCoreFunction(CF id, const std::string& name, Type* rt, Type* p1, Type* p2, Type* p3, Type* p4, Type* p5, bool isVaArg);
	template<typename T>
	void emitCoreFunction(CF id, const std::string& name, Type* rt, T pbegin, T pend, bool isVaArg);
	llvm::Value* emitCoreCall(CF id, llvm::Value* p1);
	llvm::Value* emitCoreCall(CF id, llvm::Value* p1, llvm::Value* p2);
	llvm::Value* emitCoreCall(CF id, llvm::Value* p1, llvm::Value* p2, llvm::Value* p3);
	llvm::Value* emitCoreCall(CF id, llvm::Value* p1, llvm::Value* p2, llvm::Value* p3, llvm::Value* p4);
	llvm::Value* emitCoreCall(CF id, llvm::Value* p1, llvm::Value* p2, llvm::Value* p3, llvm::Value* p4, llvm::Value* p5);

	// native-function API
	void emitNativeFunctionSignature();
	void emitNativeCall(int id, ListExpr* args);
	llvm::Value* emitNativeValue(int index, llvm::Value* lhs, llvm::Value* rhs, const std::string& name = "result");
	llvm::Value* emitToValue(llvm::Value* value, const std::string& name);

	// general
	void emitCall(Function* callee, ListExpr* args);

	llvm::Value* toBool(llvm::Value* value);

	llvm::Value* codegen(Symbol* symbol);
	llvm::Value* codegen(Expr* expression);
	void codegen(Stmt* statement);

	template<typename T> T* codegen(Symbol* symbol) { return static_cast<T*>(codegen(symbol)); }

	void reportError(const std::string& message);

	template<typename... Args>
	void reportError(const std::string& fmt, Args&& ...);

protected:
	virtual void visit(Variable& symbol);
	virtual void visit(Function& symbol);
	virtual void visit(Unit& symbol);

	virtual void visit(UnaryExpr& expr);
	virtual void visit(BinaryExpr& expr);
	virtual void visit(StringExpr& expr);
	virtual void visit(NumberExpr& expr);
	virtual void visit(BoolExpr& expr);
	virtual void visit(RegExpExpr& expr);
	virtual void visit(IPAddressExpr& expr);
	virtual void visit(VariableExpr& expr);
	virtual void visit(FunctionRefExpr& expr);
	virtual void visit(CastExpr& expr);
	virtual void visit(CallExpr& expr);
	virtual void visit(ListExpr& expr);

	virtual void visit(ExprStmt& stmt);
	virtual void visit(CompoundStmt& stmt);
	virtual void visit(CondStmt& stmt);
};

//{{{ inlines
template<typename... Args>
inline void FlowRunner::reportError(const std::string& fmt, Args&&... args)
{
	char buf[1024];
	snprintf(buf, sizeof(buf), fmt.c_str(), args...);
	reportError(buf);
}
//}}}

} // namespace x0

#endif
