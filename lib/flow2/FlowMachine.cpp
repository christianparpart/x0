#include <x0/flow2/FlowMachine.h>
#include <x0/DebugLogger.h>

#if defined(LLVM_VERSION_3_3)
#  include <llvm/IR/DerivedTypes.h>
#  include <llvm/IR/LLVMContext.h>
#  include <llvm/IR/Module.h>
#  include <llvm/IR/IRBuilder.h>
#  include <llvm/Analysis/Passes.h>
#else
#  include <llvm/DerivedTypes.h>
#  include <llvm/LLVMContext.h>
#  include <llvm/Module.h>
#  include <llvm/LLVMContext.h>
#  include <llvm/DefaultPasses.h>
#  include <llvm/Support/IRBuilder.h>
#  include <llvm/Target/TargetData.h>
#endif

#include <llvm/PassManager.h>
#include <llvm/Analysis/Verifier.h>
#include <llvm/Support/ManagedStatic.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Transforms/IPO/PassManagerBuilder.h>
#include <llvm/Transforms/Scalar.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/JIT.h>
#include <llvm/Target/TargetLibraryInfo.h>
#include <llvm/ADT/Triple.h>

namespace x0 {

#define FLOW_DEBUG_CODEGEN 1

#if defined(FLOW_DEBUG_CODEGEN)
// {{{ trace
static size_t fnd = 0;
struct fntrace {
	std::string msg_;

	fntrace(const char* msg) : msg_(msg)
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

		XZERO_DEBUG("FlowParser", 5, "%s", fmt);
		++fnd;
	}

	~fntrace() {
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

		XZERO_DEBUG("FlowParser", 5, "%s", fmt);
	}
};
// }}}
#	define FNTRACE() fntrace _(__PRETTY_FUNCTION__)
#	define TRACE(level, msg...) XZERO_DEBUG("FlowMachine", (level), msg)
#else
#	define FNTRACE() /*!*/
#	define TRACE(level, msg...) /*!*/
#endif

enum class CoreFunction {
	strlen = 1,
	strcasecmp,
	strncasecmp,
	strcasestr,
	strcmp,
	strncmp,
	regexmatch,
	regexmatch2,
	endsWith,
	strcat,
	strcpy,
	memcpy,

	arraylen,
	arrayadd,
	arraycmp,

	NumberInArray,
	StringInArray,

	ipstrcmp,	// compare(IPAddress, String)
	ipcmp,		// compare(IPAddress, IPAddress)
	ipInCidr,	// IP in CIDR
	ipInArray,	// IP in [...]

	// conversion
	bool2str,
	int2str,
	str2int,
	buf2int,

	// mathematical helper
	pow,
};

// {{{ FlowMachine::Scope
FlowMachine::Scope::Scope()
{
	enter(); // global scope
}

FlowMachine::Scope::~Scope()
{
	while (scope_.size())
		leave();
}

void FlowMachine::Scope::clear()
{
	// pop all scopes
	while (scope_.size())
		leave();

	// re-enter new global scope
	enter();
}

void FlowMachine::Scope::enter()
{
	scope_.push_front(new std::unordered_map<Symbol*, llvm::Value*>());
}

void FlowMachine::Scope::leave()
{
	delete scope_.front();
	scope_.pop_front();
}

llvm::Value* FlowMachine::Scope::lookup(Symbol* symbol) const
{
	for (auto i: scope_) {
		auto k = i->find(symbol);

		if (k != i->end()) {
			return k->second;
		}
	}

	return nullptr;
}

void FlowMachine::Scope::insert(Symbol* symbol, llvm::Value* value)
{
	(*scope_.front())[symbol] = value;
}

void FlowMachine::Scope::insertGlobal(Symbol* symbol, llvm::Value* value)
{
	(*scope_.back())[symbol] = value;
}

void FlowMachine::Scope::remove(Symbol* symbol)
{
	auto i = scope_.front()->find(symbol);

	if (i != scope_.front()->end())
		scope_.front()->erase(i);
}
// }}}

FlowMachine::FlowMachine() :
	optimizationLevel_(0),
	backend_(nullptr),
	scope_(nullptr),
	cx_(),
	module_(nullptr),
	executionEngine_(nullptr),
	modulePassMgr_(nullptr),
	functionPassMgr_(nullptr),
	valueType_(nullptr),
	regexType_(nullptr),
	arrayType_(nullptr),
	ipaddrType_(nullptr),
	cidrType_(nullptr),
	bufferType_(nullptr),
	coreFunctions_(), // vector<> of core functions
	builder_(cx_),
	value_(nullptr),
	initializerFn_(nullptr),
	initializerBB_(nullptr)
{
	scope_ = new Scope();
}

FlowMachine::~FlowMachine()
{
	delete scope_;
}

void FlowMachine::initialize()
{
	llvm::InitializeNativeTarget();
}

void FlowMachine::shutdown()
{
	llvm::llvm_shutdown();
}

void FlowMachine::dump()
{
}

void FlowMachine::clear()
{
}

bool FlowMachine::prepare()
{
	module_ = new llvm::Module("flow", cx_);

	std::string errorStr;
	executionEngine_ = llvm::EngineBuilder(module_).setErrorStr(&errorStr).create();
	if (!executionEngine_) {
		TRACE(1, "execution engine creation failed. %s", errorStr.c_str());
		return false;
	}

	{ // {{{ setup optimization
		llvm::PassManagerBuilder pmBuilder;
		pmBuilder.OptLevel = optimizationLevel_;
		pmBuilder.LibraryInfo = new llvm::TargetLibraryInfo(llvm::Triple(module_->getTargetTriple()));

		// module pass mgr
		modulePassMgr_ = new llvm::PassManager();
#if !defined(LLVM_VERSION_3_3) // TODO port to LLVM 3.3+
		modulePassMgr_->add(new llvm::TargetData(module_));
#endif
		pmBuilder.populateModulePassManager(*modulePassMgr_);

		// function pass mgr
		functionPassMgr_ = new llvm::FunctionPassManager(module_);
#if !defined(LLVM_VERSION_3_3) // TODO port to LLVM 3.3+
		functionPassMgr_->add(new llvm::TargetData(module_));
#endif
		pmBuilder.populateFunctionPassManager(*functionPassMgr_);
	} // }}}

	// RegExp
	std::vector<llvm::Type*> elts;
	elts.push_back(int8PtrType());   // name (const char *)
	elts.push_back(int8PtrType());   // handle (pcre *)
	regexType_ = llvm::StructType::create(cx_, elts, "RegExp", true/*packed*/);

	// IPAddress
	elts.clear();
	elts.push_back(int32Type());	// domain (AF_INET, AF_INET6)
	for (size_t i = 0; i < 8; ++i)	// IPv6 raw address (128 bits)
		elts.push_back(int16Type());
	ipaddrType_ = llvm::StructType::create(cx_, elts, "IPAddress", true/*packed*/);

	// BufferRef
	elts.clear();
	elts.push_back(int64Type());     // buffer length
	elts.push_back(int8PtrType());   // buffer data
	bufferType_ = llvm::StructType::create(cx_, elts, "BufferRef", true/*packed*/);

	// FlowValue
	elts.push_back(int32Type());     // type id
	elts.push_back(numberType());    // number (long long)
	elts.push_back(int8PtrType());   // string (char*)
	valueType_ = llvm::StructType::create(cx_, elts, "Value", true/*packed*/);

	return true;
}

bool FlowMachine::codegen(Unit* unit)
{
	if (!prepare())
		return false;

	return false;
}

FlowMachine::Handler FlowMachine::findHandler(const std::string& name)
{
	return nullptr;
}

void FlowMachine::reportError(const std::string& message)
{
}

llvm::Value* FlowMachine::codegen(Expr* expr)
{
	return nullptr;
}

void FlowMachine::codegen(Stmt* stmt)
{
	FNTRACE();
}

llvm::Value* FlowMachine::toBool(llvm::Value* value)
{
	FNTRACE();
	return nullptr;
}

llvm::Type* FlowMachine::boolType() const
{
	return llvm::Type::getInt1Ty(cx_);
}

llvm::Type* FlowMachine::int8Type() const
{
	return llvm::Type::getInt8Ty(cx_);
}

llvm::Type* FlowMachine::int16Type() const
{
	return llvm::Type::getInt16Ty(cx_);
}

llvm::Type* FlowMachine::int32Type() const
{
	return llvm::Type::getInt32Ty(cx_);
}

llvm::Type* FlowMachine::int64Type() const
{
	return llvm::Type::getInt64Ty(cx_);
}

llvm::Type* FlowMachine::numberType() const
{
	return int64Type();
}

llvm::Type* FlowMachine::int8PtrType() const
{
	return int8Type()->getPointerTo();
}

void FlowMachine::visit(Variable& variable)
{
	FNTRACE();
}

void FlowMachine::visit(Handler& handler)
{
	FNTRACE();
}

void FlowMachine::visit(BuiltinFunction& symbol)
{
	FNTRACE();
}

void FlowMachine::visit(BuiltinHandler& symbol)
{
	FNTRACE();
}

void FlowMachine::visit(Unit& symbol)
{
	FNTRACE();
}

void FlowMachine::visit(UnaryExpr& expr)
{
	FNTRACE();
}

void FlowMachine::visit(BinaryExpr& expr)
{
	FNTRACE();
}

void FlowMachine::visit(FunctionCallExpr& expr)
{
	FNTRACE();
}

void FlowMachine::visit(VariableExpr& expr)
{
	FNTRACE();
}

void FlowMachine::visit(HandlerRefExpr& expr)
{
	FNTRACE();
}

void FlowMachine::visit(ListExpr& expr)
{
	FNTRACE();
}

void FlowMachine::visit(StringExpr& expr)
{
	FNTRACE();
	value_ = builder_.CreateGlobalStringPtr(expr.value().c_str());
}

void FlowMachine::visit(NumberExpr& expr)
{
	FNTRACE();
	value_ = llvm::ConstantInt::get(numberType(), expr.value());
}

void FlowMachine::visit(BoolExpr& expr)
{
	FNTRACE();
	value_ = llvm::ConstantInt::get(boolType(), expr.value() ? 1 : 0);
}

void FlowMachine::visit(RegExpExpr& expr)
{
	FNTRACE();
}

void FlowMachine::visit(IPAddressExpr& expr)
{
	FNTRACE();
}

void FlowMachine::visit(CidrExpr& cidr)
{
	FNTRACE();
}

void FlowMachine::visit(ExprStmt& stmt)
{
	FNTRACE();
}

void FlowMachine::visit(CompoundStmt& stmt)
{
	FNTRACE();
}

void FlowMachine::visit(CondStmt& stmt)
{
	FNTRACE();
}

void FlowMachine::visit(AssignStmt& stmt)
{
	FNTRACE();
}

void FlowMachine::visit(HandlerCallStmt& stmt)
{
	FNTRACE();
}

void FlowMachine::visit(BuiltinHandlerCallStmt& stmt)
{
	FNTRACE();
}

} // namespace x0
