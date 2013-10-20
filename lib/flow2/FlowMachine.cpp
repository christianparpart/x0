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

FlowMachine::FlowMachine() :
	cx_(),
	module_(nullptr),
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
}

FlowMachine::~FlowMachine()
{
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
//	module_ = new llvm::Module("flow", cx_);

//	std::string errorStr;
//	executionEngine_ = llvm::EngineBuilder(module_).setErrorStr(&errorStr).create();
//	if (!executionEngine_) {
//		TRACE(1, "execution engine creation failed. %s", errorStr.c_str());
//		return false;
//	}

//	std::vector<llvm::Type*> elts;
//	elts.push_back(int8PtrType());   // name (const char *)
//	elts.push_back(int8PtrType());   // handle (pcre *)
//	regexpType_ = llvm::StructType::create(cx_, elts, "RegExp", true/*packed*/);

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
