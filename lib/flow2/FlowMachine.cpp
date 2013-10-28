#include <x0/flow2/FlowMachine.h>
#include <x0/flow2/FlowType.h>
#include <x0/flow2/FlowBackend.h>
#include <x0/DebugLogger.h>

#if defined(LLVM_VERSION_3_3)
#  include <llvm/IR/DerivedTypes.h>
#  include <llvm/IR/LLVMContext.h>
#  include <llvm/IR/Module.h>
#  include <llvm/IR/IRBuilder.h>
#  include <llvm/IR/Intrinsics.h>
#  include <llvm/Analysis/Passes.h>
#else
#  include <llvm/DerivedTypes.h>
#  include <llvm/LLVMContext.h>
#  include <llvm/Module.h>
#  include <llvm/LLVMContext.h>
#  include <llvm/DefaultPasses.h>
#  include <llvm/Support/IRBuilder.h>
#  include <llvm/Intrinsics.h>
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
struct fntrace2 {
	std::string msg_;

	fntrace2(const char* msg) : msg_(msg)
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

		XZERO_DEBUG("FlowMachine", 5, "%s", fmt);
		++fnd;
	}

	~fntrace2() {
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

		XZERO_DEBUG("FlowMachine", 5, "%s", fmt);
	}
};
// }}}
#	define FNTRACE() fntrace2 _(__PRETTY_FUNCTION__)
#	define TRACE(level, msg...) XZERO_DEBUG("FlowMachine", (level), msg)
#else
#	define FNTRACE() /*!*/
#	define TRACE(level, msg...) /*!*/
#endif

/*
 * TODO:
 *  - Do we wanna support *signed* numbers et all? or just unsigned numbers?
 *
 * BINARY OPERATIONS
 *
 *   (int, int)        == != <= >= < > + - * / shl shr mod pow
 *   (bool, bool)      == != and or xor
 *   (string, string)  == != <= >= < > + and or xor
 *   (ip, ip)          == !=
 *   (cidr, cidr)      == !=
 *   (ip, cidr)        in
 *   (string, regex)   =~
 *
 * UNARY OPERATION: `not`
 *
 *   int               0
 *   bool              false
 *   string            empty
 *
 * OPERATOR CLASSES;
 *
 *   logic             and or xor
 *   rel               == != <= >= < >
 *   group             + - * / ** shl shr
 *     assoc
 *     right-assoc
 *   set               in
 *   bit-logic         & | ~
 *
 * BUILTIN FUNCTIONS:
 *
 *   int strlen(string)
 *   string bool2str(bool)
 *   string int2str(int)
 *   string re2str(regex)
 *   string ip2str(ip)
 *   string cidr2str(cidr)
 *   string buf2str(buf)
 *   string handler2str(handlerRef)
 */

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

FlowMachine::FlowMachine(FlowBackend* backend) :
	optimizationLevel_(0),
	backend_(backend),
	scope_(nullptr),
	cx_(),
	module_(nullptr),
	executionEngine_(nullptr),
	modulePassMgr_(nullptr),
	functionPassMgr_(nullptr),
	valuePtrType_(nullptr),
	valueType_(nullptr),
	regexType_(nullptr),
	ipaddrType_(nullptr),
	cidrType_(nullptr),
	bufferType_(nullptr),
	coreFunctions_(), // vector<> of core functions
	userdata_(nullptr),
	builder_(cx_),
	value_(nullptr),
	listSize_(0),
	initializerFn_(nullptr),
	initializerBB_(nullptr),
	requestingLvalue_(false),
	functions_()
{
	std::memset(coreFunctions_, 0, sizeof(coreFunctions_));
	scope_ = new Scope();
	llvm::InitializeNativeTarget();
}

FlowMachine::~FlowMachine()
{
	delete scope_;
}

void FlowMachine::shutdown()
{
	llvm::llvm_shutdown();
}

extern "C" void flow_native_call(FlowMachine* self, uint32_t id, FlowContext* cx, uint32_t argc, FlowValue* argv)
{
	printf("flow_native_call(self:%p, id:%d, cx:%p, argc:%d, argv:%p)\n", self, id, cx, argc, argv);
}

void FlowMachine::dump()
{
	module_->dump();
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
	std::vector<llvm::Type*> argTypes;
	argTypes.push_back(int8PtrType());   // name (const char *)
	argTypes.push_back(int8PtrType());   // handle (pcre *)
	regexType_ = llvm::StructType::create(cx_, argTypes, "RegExp", true /*packed*/);

	// IPAddress
	argTypes.clear();
	argTypes.push_back(int32Type());	// domain (AF_INET, AF_INET6)
	for (size_t i = 0; i < 8; ++i)	// IPv6 raw address (128 bits)
		argTypes.push_back(int16Type());
	ipaddrType_ = llvm::StructType::create(cx_, argTypes, "IPAddress", true /*packed*/);

	// FlowValue
	argTypes.clear();
	argTypes.push_back(int32Type());     // type id
	argTypes.push_back(int64Type());     // number
	argTypes.push_back(int8PtrType());   // data ptr
	valueType_ = llvm::StructType::create(cx_, argTypes, "Value", true /*packed*/);
	valuePtrType_ = valueType_->getPointerTo();

	// FlowBuffer
	argTypes.clear();
	argTypes.push_back(int64Type());     // size (uint64_t)
	argTypes.push_back(int8PtrType());   // data (char*)
	bufferType_ = llvm::StructType::create(cx_, argTypes, "Buffer", true /*packed*/);

	// Cidr
	argTypes.clear();
	argTypes.push_back(ipaddrType_->getPointerTo());
	argTypes.push_back(int32Type());
	cidrType_ = llvm::StructType::create(cx_, argTypes, "Cidr", true /*packed*/);

	// initializer
	argTypes.clear();
	initializerFn_ = llvm::Function::Create(
		llvm::FunctionType::get(llvm::Type::getVoidTy(cx_), argTypes, false),
		llvm::Function::ExternalLinkage, "__flow_initialize", module_);
	initializerBB_ = llvm::BasicBlock::Create(cx_, "entry", initializerFn_);

	emitNativeFunctionSignature();
	emitCoreFunctions();

	return true;
}

void FlowMachine::emitInitializerTail()
{
	llvm::BasicBlock* lastBB = builder_.GetInsertBlock();
	builder_.SetInsertPoint(initializerBB_);

	builder_.CreateRetVoid();

	if (lastBB)
		builder_.SetInsertPoint(lastBB);
	else
		builder_.ClearInsertionPoint();

	llvm::verifyFunction(*initializerFn_);

	if (functionPassMgr_) {
		functionPassMgr_->run(*initializerFn_);
	}
}

int FlowMachine::findNative(const std::string& name) const
{
	FNTRACE();
	return backend_->find(name);
}

bool FlowMachine::compile(Unit* unit)
{
	if (!prepare())
		return false;

	codegen(unit);

	return value_ != nullptr;
}

FlowValue::Handler FlowMachine::findHandler(const std::string& name)
{
	return nullptr;
}

void FlowMachine::reportError(const std::string& message)
{
}

llvm::Value* FlowMachine::codegen(Expr* expr)
{
	FNTRACE();

	auto c1 = builder_.GetInsertBlock();

	if (expr)
		expr->accept(*this);

	auto c2 = builder_.GetInsertBlock();

	assert(c1->getParent() == c2->getParent());
	(void) c1;
	(void) c2;

	return value_;
}

llvm::Value* FlowMachine::codegen(Symbol* symbol)
{
	FNTRACE();

	if (llvm::Value* v = scope().lookup(symbol))
		return value_ = v;

	auto c1 = builder_.GetInsertBlock();

	if (symbol)
		symbol->accept(*this);

	auto c2 = builder_.GetInsertBlock();

	if (c1 && c2 && c1->getParent() != c2->getParent())
		module_->dump();
	assert((!c1 && !c2) || (c1 && c2 && c1->getParent() == c2->getParent()));

	return value_;
}

void FlowMachine::codegen(Stmt* stmt)
{
	FNTRACE();

	auto c1 = builder_.GetInsertBlock();

	if (stmt)
		stmt->accept(*this);

	auto c2 = builder_.GetInsertBlock();

	assert(c1->getParent() == c2->getParent());
	(void) c1;
	(void) c2;
}

llvm::Value* FlowMachine::toBool(llvm::Value* value)
{
	FNTRACE();
	if (!value)
		return value_ = nullptr;

	if (isBool(value->getType()))
		return value;

	if (value->getType()->isIntegerTy())
		return builder_.CreateICmpNE(value, llvm::ConstantInt::get(value->getType(), 0), "int2bool");

	return value_; // TODO nullptr;
}

bool FlowMachine::isBool(llvm::Type* type) const
{
	if (!type->isIntegerTy())
		return false;

	llvm::IntegerType* i = static_cast<llvm::IntegerType *>(type);
	if (i->getBitWidth() != 1)
		return false;

	return true;
}

bool FlowMachine::isBool(llvm::Value* value) const
{
	return isBool(value->getType());
}

bool FlowMachine::isInteger(llvm::Value* value) const
{
	return value->getType()->isIntegerTy();
}

bool FlowMachine::isCString(llvm::Value* value) const
{
	return value && isCString(value->getType());
}

bool FlowMachine::isCString(llvm::Type* type) const
{
	if (!type || !type->isPointerTy())
		return false;

	llvm::PointerType* ptr = static_cast<llvm::PointerType *>(type);

	if (!ptr->getElementType()->isIntegerTy())
		return false;

	llvm::IntegerType* i = static_cast<llvm::IntegerType *>(ptr->getElementType());
	if (i->getBitWidth() != 8)
		return false;

	return true;
}

bool FlowMachine::isString(llvm::Value* value) const
{
	return isCString(value) || isBuffer(value);
}

bool FlowMachine::isString(llvm::Type* type) const
{
	return isCString(type) || isBuffer(type);
}

bool FlowMachine::isBuffer(llvm::Value* value) const
{
	return value && isBuffer(value->getType());
}

bool FlowMachine::isBuffer(llvm::Type* type) const
{
	return type == bufferType_;
}

bool FlowMachine::isBufferPtr(llvm::Type* type) const
{
	return type == bufferType_->getPointerTo();
}

bool FlowMachine::isBufferPtr(llvm::Value* value) const
{
	return value && isBufferPtr(value->getType());
}

bool FlowMachine::isRegExp(llvm::Value* value) const
{
	return value && isRegExp(value->getType());
}

bool FlowMachine::isRegExp(llvm::Type* type) const
{
	return type == regexType_->getPointerTo();
}

bool FlowMachine::isIPAddress(llvm::Value* value) const
{
	return value && value->getType() == ipaddrType_->getPointerTo();
}

bool FlowMachine::isHandlerRef(llvm::Value* value) const
{
	if (!value->getType()->isPointerTy())
		return false;

	llvm::PointerType* ptr = static_cast<llvm::PointerType *>(value->getType());
	llvm::Type* elementType = ptr->getElementType();

	if (!elementType->isFunctionTy())
		return false;

	// TODO: check prototype: i1 (i8* cx_udata)

	return true;
}

bool FlowMachine::isArray(llvm::Value* value) const
{
	return value && value->getType() == arrayType();
}

bool FlowMachine::isArray(llvm::Type* type) const
{
	return type == arrayType();
}

void FlowMachine::visit(Variable& var)
{
	FNTRACE();

	// local variables: put into the functions entry block with an alloca inbufuction
	TRACE(1, "local variable '%s'", var.name().c_str());
	llvm::Value* initialValue = codegen(var.initializer());
	if (!initialValue)
		return;

	llvm::Function* fn = builder_.GetInsertBlock()->getParent();

	llvm::IRBuilder<> ebb(&fn->getEntryBlock(), fn->getEntryBlock().begin());

	value_ = ebb.CreateAlloca(initialValue->getType(), 0, (var.name() + ".ptr").c_str());
	builder_.CreateStore(initialValue, value_);

	scope().insert(&var, value_);
}

void FlowMachine::visit(Handler& handler)
{
	FNTRACE();

	std::vector<llvm::Type *> argTypes;
	argTypes.push_back(int8PtrType()); // FlowContext*

	llvm::Function* fn = llvm::Function::Create(
		llvm::FunctionType::get(boolType(), argTypes, false /*no vaarg*/),
		llvm::Function::ExternalLinkage,
		handler.name(),
		module_
	);
	functions_.push_back(fn);

	scope().enter();

	// save handler's userdata (context) data, for later reference
	fn->getArgumentList().front().setName("userdata");
	userdata_ = &fn->getArgumentList().front();

	// create entry-BasicBlock for this function and enter inner scope
	llvm::BasicBlock* lastBB = builder_.GetInsertBlock();
	llvm::BasicBlock* bb = llvm::BasicBlock::Create(cx_, "entry", fn);
	builder_.SetInsertPoint(bb);

	// generate code: local-scope variables
	for (auto i = handler.scope()->begin(), e = handler.scope()->end(); i != e; ++i)
		codegen(*i);

	// generate code: function body
	codegen(handler.body());

	// generate code: catch-all return
	builder_.CreateRet(llvm::ConstantInt::get(boolType(), 0));

	TRACE(1, "verify function");
	llvm::verifyFunction(*fn);

	// perform function-level optimizations
	if (functionPassMgr_)
		functionPassMgr_->run(*fn);

	// restore outer BB insert-point & leave scope
	scope().leave();

	if (lastBB)
		builder_.SetInsertPoint(lastBB);
	else
		builder_.ClearInsertionPoint();

	value_ = fn;
	scope().insertGlobal(&handler, value_);

	TRACE(1, "handler `%s` compiled", handler.name().c_str());
}

void FlowMachine::visit(BuiltinFunction& symbol)
{
	FNTRACE();
}

void FlowMachine::visit(BuiltinHandler& symbol)
{
	FNTRACE();
}

void FlowMachine::visit(Unit& unit)
{
	FNTRACE();

	for (auto i = unit.scope()->begin(), e = unit.scope()->end(); i != e; ++i)
		codegen(*i);

	emitInitializerTail();

	//value_ = nullptr;
}

void FlowMachine::visit(UnaryExpr& expr)
{
	// TODO: not INT
	// TODO: not BOOL
	// TODO: not STRING
	// TODO: not BUFFER
	//
	// TODO: - INT

	FNTRACE();
}

void FlowMachine::visit(BinaryExpr& expr)
{
	FNTRACE();

	switch (expr.op()) {
		case FlowToken::And: {
			llvm::Value* left = toBool(codegen(expr.leftExpr()));
			if (!left) return;

			llvm::Value* right = toBool(codegen(expr.rightExpr()));
			if (!right) return;

			value_ = builder_.CreateAnd(left, right);
			break;
		}
		case FlowToken::Xor: {
			llvm::Value* left = toBool(codegen(expr.leftExpr()));
			if (!left) return;

			llvm::Value* right = toBool(codegen(expr.rightExpr()));
			if (!right) return;

			value_ = builder_.CreateXor(left, right);
			break;
		}
		case FlowToken::Or: {
			requestingLvalue_ = true;
			llvm::Value* left = toBool(codegen(expr.leftExpr()));
			requestingLvalue_ = false;
			if (!left) return;

			llvm::Function* caller = builder_.GetInsertBlock()->getParent();
			llvm::BasicBlock* rhsBB = llvm::BasicBlock::Create(cx_, "or.rhs", caller);
			llvm::BasicBlock* contBB = llvm::BasicBlock::Create(cx_, "or.cont");

			// cond
			builder_.CreateCondBr(left, contBB, rhsBB);
			llvm::BasicBlock* cmpBB = builder_.GetInsertBlock();

			// rhs-bb
			builder_.SetInsertPoint(rhsBB);
			llvm::Value* right = toBool(codegen(expr.rightExpr()));
			if (!right) return;
			builder_.CreateBr(contBB);
			rhsBB = builder_.GetInsertBlock();

			// cont-bb
			caller->getBasicBlockList().push_back(contBB);
			builder_.SetInsertPoint(contBB);

			llvm::PHINode* pn = builder_.CreatePHI(llvm::Type::getInt1Ty(cx_), 2, "or.phi");
			pn->addIncoming(left, cmpBB);
			pn->addIncoming(right, rhsBB);

			value_ = pn;
			return;
		}
		default:
			break;
	}

	llvm::Value* left = codegen(expr.leftExpr());
	llvm::Value* right = codegen(expr.rightExpr());

	// bool, bool
	if (isBool(left) && isBool(right))
		return emitOpBoolBool(expr.op(), left, right);

	// int, int
	if (isInteger(left) && isInteger(right))
		return emitOpIntInt(expr.op(), left, right);

	// string, string
	if (isString(left) && isString(right))
		return emitOpStrStr(expr.op(), left, right);

	// TODO: IP, IP
	// TODO: CIDR, CIDR
	// TODO: IP, CIDR
	// TODO: string, regex

	fprintf(stderr, "Code generation type error (%s).\n", expr.op().c_str());
	left->dump();
	right->dump();

	value_ = nullptr;
}

void FlowMachine::emitNativeFunctionSignature()
{
	std::vector<llvm::Type *> argTypes;

	argTypes.push_back(int64Type());                // self ptr
	argTypes.push_back(int32Type());                // function id
	argTypes.push_back(int8PtrType());              // context userdata
	argTypes.push_back(int32Type());                // argc
	argTypes.push_back(valueType_->getPointerTo()); // FlowValue* argv

	llvm::FunctionType* ft = llvm::FunctionType::get(
		voidType(), // return type
		argTypes,   // arg types
		false       // isVaArg
	);

	coreFunctions_[static_cast<size_t>(CF::native)] = llvm::Function::Create(
		ft,
		llvm::Function::ExternalLinkage,
		"flow_native_call",
		module_
	);
}

void FlowMachine::emitCoreFunctions() // TODO
{
	emitCoreFunction(CF::strlen, "strlen", int64Type(), stringType(), false);
	emitCoreFunction(CF::strcat, "strcat", stringType(), stringType(), stringType(), false);
	emitCoreFunction(CF::strcpy, "strcpy", stringType(), stringType(), stringType(), false);
	emitCoreFunction(CF::memcpy, "memcpy", stringType(), stringType(), stringType(), int64Type(), false);

	emitCoreFunction(CF::strcasecmp, "strcasecmp", int32Type(), stringType(), stringType(), false);
	emitCoreFunction(CF::strncasecmp, "strncasecmp", int32Type(), stringType(), stringType(), int64Type(), false);
	emitCoreFunction(CF::strcasestr, "strcasestr", stringType(), stringType(), stringType(), false);

	emitCoreFunction(CF::strcmp, "strcmp", int32Type(), stringType(), stringType(), false);
	emitCoreFunction(CF::strncmp, "strncmp", int32Type(), stringType(), stringType(), false);

	emitCoreFunction(CF::endsWith, "flow_endsWidth", int32Type(), stringType(), stringType(), false);

	emitCoreFunction(CF::arrayadd, "flow_array_add", voidType(), arrayType(), arrayType(), arrayType(), false);
	emitCoreFunction(CF::arraycmp, "flow_array_cmp", int32Type(), arrayType(), arrayType(), false);

	emitCoreFunction(CF::regexmatch, "flow_regexmatch", int32Type(), int8PtrType(), int64Type(), stringType(), int64Type(), stringType(), false);
	emitCoreFunction(CF::regexmatch2, "flow_regexmatch2", int32Type(), int8PtrType(), int64Type(), stringType(), regexType_->getPointerTo(), false);

	emitCoreFunction(CF::NumberInArray, "flow_NumberInArray", int32Type(), int64Type(), arrayType(), false);
	emitCoreFunction(CF::StringInArray, "flow_StringInArray", int32Type(), int64Type(), stringType(), arrayType(), false);

	emitCoreFunction(CF::ipstrcmp, "flow_ipstrcmp", int32Type(), ipaddrType(), stringType(), false);
	emitCoreFunction(CF::ipcmp, "flow_ipcmp", int32Type(), ipaddrType(), ipaddrType(), false);
	emitCoreFunction(CF::pow, "llvm.pow.f64", doubleType(), doubleType(), doubleType(), false);

	emitCoreFunction(CF::bool2str, "flow_bool2str", stringType(), boolType(), false);
	emitCoreFunction(CF::int2str, "flow_int2str", int32Type(), stringType(), int64Type(), false);
	emitCoreFunction(CF::str2int, "flow_str2int", int64Type(), stringType(), false);
	emitCoreFunction(CF::buf2int, "flow_buf2int", int64Type(), stringType(), int64Type(), false);
}

void FlowMachine::emitCoreFunction(CF id, const std::string& name, llvm::Type* rt, llvm::Type* p1, bool isVaArg)
{
	llvm::Type* p[1] = { p1 };
	emitCoreFunction(id, name, rt, p, p + sizeof(p) / sizeof(*p), isVaArg);
}

void FlowMachine::emitCoreFunction(CF id, const std::string& name, llvm::Type* rt, llvm::Type* p1, llvm::Type* p2, bool isVaArg)
{
	llvm::Type* p[2] = { p1, p2 };
	emitCoreFunction(id, name, rt, p, p + sizeof(p) / sizeof(*p), isVaArg);
}

void FlowMachine::emitCoreFunction(CF id, const std::string& name, llvm::Type* rt, llvm::Type* p1, llvm::Type* p2, llvm::Type* p3, bool isVaArg)
{
	llvm::Type* p[3] = { p1, p2, p3 };
	emitCoreFunction(id, name, rt, p, p + sizeof(p) / sizeof(*p), isVaArg);
}

void FlowMachine::emitCoreFunction(CF id, const std::string& name, llvm::Type* rt, llvm::Type* p1, llvm::Type* p2, llvm::Type* p3, llvm::Type* p4, bool isVaArg)
{
	llvm::Type* p[4] = { p1, p2, p3, p4 };
	emitCoreFunction(id, name, rt, p, p + sizeof(p) / sizeof(*p), isVaArg);
}

void FlowMachine::emitCoreFunction(CF id, const std::string& name, llvm::Type* rt, llvm::Type* p1, llvm::Type* p2, llvm::Type* p3, llvm::Type* p4, llvm::Type* p5, bool isVaArg)
{
	llvm::Type* p[5] = { p1, p2, p3, p4, p5 };
	emitCoreFunction(id, name, rt, p, p + sizeof(p) / sizeof(*p), isVaArg);
}

template<typename T>
void FlowMachine::emitCoreFunction(CF id, const std::string& name, llvm::Type* rt, T pbegin, T pend, bool isVaArg)
{
	std::vector<llvm::Type *> params;
	for (; pbegin != pend; ++pbegin)
		params.push_back(*pbegin);

	coreFunctions_[static_cast<size_t>(id)] = llvm::Function::Create(
		llvm::FunctionType::get(rt, params, isVaArg),
		llvm::Function::ExternalLinkage,
		name,
		module_
	);
}

void FlowMachine::emitOpBoolBool(FlowToken op, llvm::Value* left, llvm::Value* right)
{
	// logical AND/OR/XOR already handled

	switch (op) {
		case FlowToken::Equal:
			value_ = builder_.CreateICmpEQ(left, right);
			break;
		case FlowToken::UnEqual:
			value_ = builder_.CreateICmpNE(left, right);
			break;
		default:
			value_ = nullptr;
			reportError("Invalid binary operator passed to code generator: (int %s int)\n", op.c_str());
			assert(!"Invalid binary operator passed to code generator");
	}
}

void FlowMachine::emitOpIntInt(FlowToken op, llvm::Value* left, llvm::Value* right)
{
	// logical AND/OR/XOR already handled

	switch (op) {
		case FlowToken::Equal:
			if (static_cast<llvm::IntegerType *>(left->getType())->getBitWidth() < 64)
				left = builder_.CreateIntCast(left, numberType(), false, "lhs.i64cast");

			if (static_cast<llvm::IntegerType *>(right->getType())->getBitWidth() < 64)
				right = builder_.CreateIntCast(right, numberType(), false, "rhs.i64cast");

			value_ = builder_.CreateICmpEQ(left, right);
		case FlowToken::UnEqual:
			if (static_cast<llvm::IntegerType *>(left->getType())->getBitWidth() < 64)
				left = builder_.CreateIntCast(left, numberType(), false, "lhs.i64cast");

			if (static_cast<llvm::IntegerType *>(right->getType())->getBitWidth() < 64)
				right = builder_.CreateIntCast(right, numberType(), false, "rhs.i64cast");

			value_ = builder_.CreateICmpNE(left, right);
			break;
		case FlowToken::Less:
			value_ = builder_.CreateICmpSLT(left, right);
			break;
		case FlowToken::Greater:
			value_ = builder_.CreateICmpSGT(left, right);
			break;
		case FlowToken::LessOrEqual:
			value_ = builder_.CreateICmpSGE(left, right);
			break;
		case FlowToken::GreaterOrEqual:
			value_ = builder_.CreateICmpSGE(left, right);
			break;
		case FlowToken::Plus:
			value_ = builder_.CreateAdd(left, right);
			break;
		case FlowToken::Minus:
			value_ = builder_.CreateSub(left, right);
			break;
		case FlowToken::Mul:
			value_ = builder_.CreateMul(left, right);
			break;
		case FlowToken::Div:
			value_ = builder_.CreateSDiv(left, right);
			break;
		case FlowToken::Mod:
			value_ = builder_.CreateSRem(left, right);
			break;
		case FlowToken::Shl:
			value_ = builder_.CreateShl(left, right);
			break;
		case FlowToken::Shr:
			// logical bit-shift right (meaning, it does not honor possible signedness)
			value_ = builder_.CreateLShr(left, right);
			break;
		case FlowToken::BitAnd:
			value_ = builder_.CreateAnd(left, right);
			break;
		case FlowToken::BitOr:
			value_ = builder_.CreateOr(left, right);
			break;
		case FlowToken::BitXor:
			value_ = builder_.CreateXor(left, right);
			break;
		case FlowToken::Pow: {
			auto argType = llvm::Type::getDoubleTy(cx_);
			left = builder_.CreateSIToFP(left, argType, "pow.lhs");
			right = builder_.CreateSIToFP(right, argType, "pow.rhs");
			auto pow = llvm::Intrinsic::getDeclaration(module_, llvm::Intrinsic::pow, left->getType());
			value_ = builder_.CreateCall2(pow, left, right, "pow.result");
			value_ = builder_.CreateIntCast(left, numberType(), false, "pow_result_to_i64");
			break;
		}
		default:
			value_ = nullptr;
			reportError("Invalid binary operator passed to code generator: (int %s int)\n", op.c_str());
			assert(!"Invalid binary operator passed to code generator");
	}
}

void FlowMachine::emitOpStrStr(FlowToken op, llvm::Value* left, llvm::Value* right)
{
	// TODO (string, string)  == != <= >= < > + and or xor
#if 1 == 0
	llvm::Value* len1;
	llvm::Value* buf1;
	llvm::Value* len2;
	llvm::Value* buf2;

	if (isBufferPtr(left)) {
		len1 = emitLoadBufferLength(left);
		buf1 = emitLoadBufferData(left);
	} else {
		len1 = emitCoreCall(CF::strlen, left);
		buf1 = left;
	}

	if (isBufferPtr(right)) {
		len2 = emitLoadBufferLength(right);
		buf2 = emitLoadBufferData(right);
	} else {
		len2 = emitCoreCall(CF::strlen, right);
		buf2 = right;
	}

	llvm::Value* rv = op == Operator::RegexMatch
		? emitCoreCall(CF::regexmatch, handlerUserData(), len1, buf1, len2, buf2)
		: emitCmpString(len1, buf1, len2, buf2);

	switch (op) {
		case FlowToken::RegexMatch:
			return builder_.CreateICmpNE(rv, llvm::ConstantInt::get(int32Type(), 0));
		case FlowToken::Equal:
			return builder_.CreateICmpEQ(rv, llvm::ConstantInt::get(int64Type(), 0));
		case FlowToken::UnEqual:
			return builder_.CreateICmpNE(rv, llvm::ConstantInt::get(int64Type(), 0));
		case FlowToken::LessOrEqual:
			return builder_.CreateICmpSLE(rv, llvm::ConstantInt::get(int64Type(), 0));
		case FlowToken::GreaterOrEqual:
			return builder_.CreateICmpSGE(rv, llvm::ConstantInt::get(int64Type(), 0));
		case FlowToken::Less:
			return builder_.CreateICmpSLT(rv, llvm::ConstantInt::get(int64Type(), 0));
		case FlowToken::Greater:
			return builder_.CreateICmpSGT(rv, llvm::ConstantInt::get(int64Type(), 0));
		case FlowToken::Plus:
			// TODO
		default:
			value_ = nullptr;
			reportError("Invalid binary operator passed to code generator: (int %s int)\n", op.c_str());
			assert(!"Invalid binary operator passed to code generator");
	}
#endif
}

void FlowMachine::visit(FunctionCallExpr& expr)
{
	FNTRACE();
	// TODO
}

void FlowMachine::visit(VariableExpr& expr)
{
	FNTRACE();

	value_ = codegen(expr.variable());
	if (!value_)
		return;

	if (!requestingLvalue_)
		value_ = builder_.CreateLoad(value_, expr.variable()->name().c_str());
}

void FlowMachine::visit(HandlerRefExpr& expr)
{
	FNTRACE();
	// TODO
}

void FlowMachine::visit(ListExpr& list)
{
	FNTRACE();

	size_t listSize = list.size();

	llvm::Value* sizeValue = llvm::ConstantInt::get(int32Type(), listSize);
	llvm::Value* array = builder_.CreateAlloca(valueType_, sizeValue, "array");

	for (size_t i = 0; i != listSize; ++i) {
		char name[64];
		snprintf(name, sizeof(name), "array.value.%zu", i);
		emitNativeValue(i, array, codegen(list.at(i)), name);
	}

	value_ = array;
	listSize_ = listSize;
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

	for (auto& s: stmt)
		codegen(s.get());
}

void FlowMachine::visit(CondStmt& stmt)
{
	FNTRACE();

	llvm::Function* caller = builder_.GetInsertBlock()->getParent();

	llvm::Value* condValue = toBool(codegen(stmt.condition()));

	llvm::BasicBlock* thenBlock = llvm::BasicBlock::Create(cx_, "on.then", caller);
	llvm::BasicBlock* elseBlock = llvm::BasicBlock::Create(cx_, "on.else");
	llvm::BasicBlock* contBlock = llvm::BasicBlock::Create(cx_, "on.cont");
	builder_.CreateCondBr(condValue, thenBlock, elseBlock);

	// emit on.then block
	builder_.SetInsertPoint(thenBlock);
	codegen(stmt.thenStmt());
	builder_.CreateBr(contBlock);
	thenBlock = builder_.GetInsertBlock();

	// emit on.else block
	caller->getBasicBlockList().push_back(elseBlock);
	builder_.SetInsertPoint(elseBlock);
	codegen(stmt.elseStmt());
	builder_.CreateBr(contBlock);

	// emit on.cont block
	caller->getBasicBlockList().push_back(contBlock);
	builder_.SetInsertPoint(contBlock);
}

void FlowMachine::visit(AssignStmt& assign)
{
	FNTRACE();

	requestingLvalue_ = true;
	llvm::Value* left = codegen(assign.variable());
	requestingLvalue_ = false;

	llvm::Value* right = codegen(assign.expression());

	value_ = builder_.CreateStore(right, left);
}

void FlowMachine::visit(CallStmt& callStmt)
{
	FNTRACE();

	emitCall(callStmt.callee(), callStmt.args());
}

/** emits rhs into a FlowValue at lhs[index] (if lhs != null).
 *
 * @param index
 * @param lhs
 * @param rhs
 * @param name
 *
 * \returns &lhs[index] or the temporary holding the result.
 */
llvm::Value* FlowMachine::emitNativeValue(size_t index, llvm::Value* lhs, llvm::Value* rhs, const std::string& name)
{
	FNTRACE();

	llvm::Value* result = lhs != nullptr
		? lhs
		: builder_.CreateAlloca(valueType_, llvm::ConstantInt::get(int32Type(), 1), name);

	llvm::Value* valueIndices[2] = {
		llvm::ConstantInt::get(llvm::Type::getInt32Ty(cx_), index),
		nullptr
	};

	FlowType typeCode;

	if (rhs == nullptr) {
		typeCode = FlowType::Void;
	}
	else if (isBool(rhs)) {
		typeCode = FlowType::Boolean;
		llvm::Value* source = builder_.CreateIntCast(rhs, numberType(), false, "bool2int");
		valueIndices[1] = llvm::ConstantInt::get(llvm::Type::getInt32Ty(cx_), FlowValueOffset::Number);
		llvm::Value* target = builder_.CreateInBoundsGEP(result, valueIndices, "val.bool");
		builder_.CreateStore(source, target, "store.arg.value");
	}
	else if (rhs->getType()->isIntegerTy()) {
		typeCode = FlowType::Number;
		valueIndices[1] = llvm::ConstantInt::get(llvm::Type::getInt32Ty(cx_), FlowValueOffset::Number);
		builder_.CreateStore(rhs, builder_.CreateInBoundsGEP(result, valueIndices), "store.arg.value");
	}
	else if (isArray(rhs)) { // some expression list
		typeCode = FlowType::Array;

		valueIndices[1] = llvm::ConstantInt::get(llvm::Type::getInt32Ty(cx_), FlowValueOffset::ArrayData);
		rhs = builder_.CreateBitCast(rhs, int8PtrType());
		builder_.CreateStore(rhs, builder_.CreateInBoundsGEP(result, valueIndices), "stor.ary");

		valueIndices[1] = llvm::ConstantInt::get(int32Type(), FlowValueOffset::ArraySize);
		builder_.CreateStore(llvm::ConstantInt::get(numberType(), listSize_),
			builder_.CreateInBoundsGEP(result, valueIndices), "store.array.length");
	}
	else if (isRegExp(rhs)) {
		typeCode = FlowType::RegExp;

		valueIndices[1] = llvm::ConstantInt::get(llvm::Type::getInt32Ty(cx_), FlowValueOffset::RegExp);
		rhs = builder_.CreateBitCast(rhs, int8PtrType());
		builder_.CreateStore(rhs, builder_.CreateInBoundsGEP(result, valueIndices), "stor.regexp");
	}
	else if (isIPAddress(rhs)) {
		typeCode = FlowType::IPAddress;

		valueIndices[1] = llvm::ConstantInt::get(llvm::Type::getInt32Ty(cx_), FlowValueOffset::IPAddress);
		rhs = builder_.CreateBitCast(rhs, int8PtrType());
		builder_.CreateStore(rhs, builder_.CreateInBoundsGEP(result, valueIndices), "stor.ip");
	}
	else if (isHandlerRef(rhs)) {
		typeCode = FlowType::Handler;
		valueIndices[1] = llvm::ConstantInt::get(llvm::Type::getInt32Ty(cx_), FlowValueOffset::Handler);
		rhs = builder_.CreateBitCast(rhs, int8PtrType());
		builder_.CreateStore(rhs, builder_.CreateInBoundsGEP(result, valueIndices), "stor.fnref");
	}
	else if (isCString(rhs)) {
		typeCode = FlowType::String;

		valueIndices[1] = llvm::ConstantInt::get(llvm::Type::getInt32Ty(cx_), FlowValueOffset::String);
		builder_.CreateStore(rhs, builder_.CreateInBoundsGEP(result, valueIndices), "stor.str");
	}
	else if (isBufferPtr(rhs)) {
		typeCode = FlowType::Buffer;

		llvm::Value* len = emitLoadBufferLength(rhs);
		llvm::Value* buf = emitLoadBufferData(rhs);

		valueIndices[1] = llvm::ConstantInt::get(llvm::Type::getInt32Ty(cx_), FlowValueOffset::BufferSize);
		builder_.CreateStore(len, builder_.CreateInBoundsGEP(result, valueIndices), "stor.buflen");

		valueIndices[1] = llvm::ConstantInt::get(llvm::Type::getInt32Ty(cx_), FlowValueOffset::BufferData);
		builder_.CreateStore(buf, builder_.CreateInBoundsGEP(result, valueIndices), "stor.bufdata");
	}
	else {
		fprintf(stderr, "Emitting native value of unknown type? (fn:%d)\n", rhs->getType()->isFunctionTy());
		typeCode = FlowType::Void;

		fprintf(stderr, "type:\n");
		rhs->getType()->dump();

		fprintf(stderr, "rhs:\n");
		rhs->dump();

		fprintf(stderr, "lhs:\n");
		result->dump();

		abort();
	}

	// store values type code
	llvm::Value* typeIndices[2] = {
		llvm::ConstantInt::get(llvm::Type::getInt32Ty(cx_), index),
		llvm::ConstantInt::get(llvm::Type::getInt32Ty(cx_), 0)
	};
	builder_.CreateStore(
		llvm::ConstantInt::get(llvm::Type::getInt32Ty(cx_), static_cast<int>(typeCode)),
		builder_.CreateInBoundsGEP(result, typeIndices, "arg.type"),
		"store.arg.type");

	return result;
}

llvm::Value* FlowMachine::emitLoadBufferData(llvm::Value* nbuf)
{
	llvm::Value* ii[2] = {
		llvm::ConstantInt::get(int32Type(), 0),
		llvm::ConstantInt::get(int32Type(), 1)
	};
	return builder_.CreateLoad(builder_.CreateInBoundsGEP(nbuf, ii), "load.nbuf.data");
}

llvm::Value* FlowMachine::emitLoadBufferLength(llvm::Value* nbuf)
{
	llvm::Value* ii[2] = {
		llvm::ConstantInt::get(int32Type(), 0),
		llvm::ConstantInt::get(int32Type(), 0)
	};
	return builder_.CreateLoad(builder_.CreateInBoundsGEP(nbuf, ii), "load.nbuf.len");
}

/**
 * Allocates a new buffer on stack and stores some data into it.
 *
 * @param data the data to store into the new buffer.
 * @param length number of bytes for the buffer, and which are to be filled by the input data.
 * @param name hint name of the new buffer.
 *
 * @return a handle to the allocated buffer.
 */
llvm::Value* FlowMachine::emitAllocaBuffer(llvm::Value* data, llvm::Value* length, const std::string& name)
{
	llvm::Value* nbuf = builder_.CreateAlloca(bufferType_, nullptr, name);
	emitStoreBuffer(nbuf, length, data);
	return nbuf;
}

llvm::Value* FlowMachine::emitStoreBufferLength(llvm::Value* nbuf, llvm::Value* length)
{
	llvm::Value* index[2] = {
		llvm::ConstantInt::get(int32Type(), 0),
		llvm::ConstantInt::get(int32Type(), 0)
	};
	llvm::Value* dest = builder_.CreateInBoundsGEP(nbuf, index);
	return builder_.CreateStore(length, dest, "store.nbuf.len");
}

llvm::Value* FlowMachine::emitStoreBufferData(llvm::Value* nbuf, llvm::Value* data)
{
	llvm::Value* index[2] = {
		llvm::ConstantInt::get(int32Type(), 0),
		llvm::ConstantInt::get(int32Type(), 1)
	};
	llvm::Value* dest = builder_.CreateInBoundsGEP(nbuf, index);
	return builder_.CreateStore(data, dest, "store.nbuf.data");
}

llvm::Value* FlowMachine::emitStoreBuffer(llvm::Value* nbuf, llvm::Value* length, llvm::Value* data)
{
	emitStoreBufferLength(nbuf, length);
	emitStoreBufferData(nbuf, data);
	return nbuf;
}

void FlowMachine::emitCall(Callable* callee, ListExpr* argList)
{
	FNTRACE();

	int id = findNative(callee->name());
	if (id < 0) {
		// TODO: emit call to flow handler
		reportError("TODO: call to custom handlers. %s\n", callee->name().c_str());
		value_ = nullptr;
		return;
	}
	TRACE(1, "emit native builtin call (%d), %s\n", id, callee->name().c_str());

	// prepare handler parameters
	llvm::Value* callArgs[5];

	callArgs[0] = llvm::ConstantInt::get(llvm::Type::getInt64Ty(cx_), reinterpret_cast<uint64_t>(backend_), false); // self
	callArgs[1] = llvm::ConstantInt::get(llvm::Type::getInt32Ty(cx_), id, false); // native callback id
	callArgs[2] = handlerUserData();

	// argc:
	int argc = argList ? argList->size() + 1 : 1;
	callArgs[3] = llvm::ConstantInt::get(llvm::Type::getInt32Ty(cx_), argc);
	TRACE(1, "argc: %d\n", argc);

	// argv:
	callArgs[4] = builder_.CreateAlloca(valueType_,
		llvm::ConstantInt::get(llvm::Type::getInt32Ty(cx_), argc), "args.ptr");

	emitNativeValue(0, callArgs[4], nullptr); // initialize return value

	int index = 1;
	if (argc > 1)
		for (auto i = argList->begin(), e = argList->end(); i != e; ++i)
			emitNativeValue(index++, callArgs[4], codegen(i->get()));

	// emit call
	value_ = builder_.CreateCall(
		coreFunctions_[static_cast<size_t>(CF::native)],
		llvm::ArrayRef<llvm::Value*>(callArgs, sizeof(callArgs) / sizeof(*callArgs))
	);

	// handle return value
	switch (callee->type()) {
		case Symbol::BuiltinFunction: {
			if (callee->returnType() == FlowType::Buffer) {
				// retrieve buffer length
				llvm::Value* valueIndices[2] = {
					llvm::ConstantInt::get(llvm::Type::getInt32Ty(cx_), FlowValueOffset::Type),
					llvm::ConstantInt::get(llvm::Type::getInt32Ty(cx_), FlowValueOffset::Number)
				};
				value_ = builder_.CreateInBoundsGEP(callArgs[4], valueIndices, "retval.buflen.tmp");
				llvm::Value* length = builder_.CreateLoad(value_, "retval.buflen.load");

				// retrieve ref to buffer data
				valueIndices[1] = llvm::ConstantInt::get(llvm::Type::getInt32Ty(cx_), FlowValueOffset::BufferData);
				value_ = builder_.CreateInBoundsGEP(callArgs[4], valueIndices, "retval.buf.tmp");
				llvm::Value* data = builder_.CreateLoad(value_, "retval.buf.load");

				value_ = emitAllocaBuffer(data, length, "retval");
			}
			else { // no buffer
				int valueIndex;
				switch (callee->returnType()) {
					case FlowType::Boolean: valueIndex = FlowValueOffset::Boolean; break;
					case FlowType::Number: valueIndex = FlowValueOffset::Number; break;
					case FlowType::String: valueIndex = FlowValueOffset::String; break;
					default: valueIndex = 0; break;
				}

				llvm::Value* valueIndices[2] = {
					llvm::ConstantInt::get(llvm::Type::getInt32Ty(cx_), 0),
					llvm::ConstantInt::get(llvm::Type::getInt32Ty(cx_), valueIndex)
				};
				value_ = builder_.CreateInBoundsGEP(callArgs[4], valueIndices, "retval.value.tmp");
				value_ = builder_.CreateLoad(value_, "retval.value.load");

				if (callee->returnType() == FlowType::Boolean) {
					value_ = builder_.CreateIntCast(value_, boolType(), false, "retval.value.boolcast");
				}
			}

			break;
		}
		case Symbol::Handler:
		case Symbol::BuiltinHandler:
		{
			llvm::Value* valueIndices[2] = {
				llvm::ConstantInt::get(llvm::Type::getInt32Ty(cx_), 0),
				llvm::ConstantInt::get(llvm::Type::getInt32Ty(cx_), FlowValueOffset::Number)
			};
			value_ = builder_.CreateInBoundsGEP(callArgs[4], valueIndices, "retval.value.tmp");
			value_ = builder_.CreateLoad(value_, "retval.value.load");

			// compare return value for not being false (zero)
			value_ = builder_.CreateICmpNE(value_, llvm::ConstantInt::get(numberType(), 0));

			// restore outer BB insert-point & leave scope
			llvm::Function* caller = builder_.GetInsertBlock()->getParent();
			llvm::BasicBlock* doneBlock = llvm::BasicBlock::Create(cx_, "handler.done", caller);
			llvm::BasicBlock* contBlock = llvm::BasicBlock::Create(cx_, "handler.cont");
			builder_.CreateCondBr(value_, doneBlock, contBlock);

			// emit handler.done block
			builder_.SetInsertPoint(doneBlock);
			builder_.CreateRet(llvm::ConstantInt::get(llvm::Type::getInt1Ty(cx_), 1));
			doneBlock = builder_.GetInsertBlock();

			// emit handler.cont block
			caller->getBasicBlockList().push_back(contBlock);
			builder_.SetInsertPoint(contBlock);
			break;
		}
		default:
			reportError("Unknown callback type (%d) encountered.", callee->type());
			break;
	}
}

} // namespace x0
