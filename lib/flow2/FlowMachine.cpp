#include <x0/flow2/FlowMachine.h>
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
	arrayType_(nullptr),
	ipaddrType_(nullptr),
	cidrType_(nullptr),
	bufferType_(nullptr),
	coreFunctions_(), // vector<> of core functions
	builder_(cx_),
	value_(nullptr),
	initializerFn_(nullptr),
	initializerBB_(nullptr),
	requestingLvalue_(false),
	functions_()
{
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

	// BufferRef
	argTypes.clear();
	argTypes.push_back(int8PtrType());   // buffer data (char*)
	argTypes.push_back(int64Type());     // buffer length
	bufferType_ = llvm::StructType::create(cx_, argTypes, "BufferRef", true /*packed*/);

	// FlowValue
	argTypes.clear();
	argTypes.push_back(int32Type());     // type id
	argTypes.push_back(numberType());    // number (long long)
	argTypes.push_back(int8PtrType());   // string (char*)
	valueType_ = llvm::StructType::create(cx_, argTypes, "Value", true /*packed*/);
	valuePtrType_ = valueType_->getPointerTo();

	// FlowValue Array
	argTypes.clear();
	argTypes.push_back(valuePtrType_); // FlowValue*
	argTypes.push_back(int32Type());     // type id
	arrayType_ = llvm::StructType::create(cx_, argTypes, "ValueArray", true /*packed*/);

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
	//fn->dump();
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
		// TODO emitNativeValue(i, array, codegen(list.at(i)), name);
	}

	value_ = array;
//	listSize_ = listSize;
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

	int id = findNative(callStmt.callee()->name());
	if (id < 0) {
		reportError("Unknown native builtin. %s\n", callStmt.callee()->name().c_str());
		value_ = nullptr;
		return;
	}
	printf("emit native builtin call (%d), %s\n", id, callStmt.callee()->name().c_str());
	llvm::Value* args = codegen(callStmt.args());
	//emitNativeCall(id, call.args());
}

} // namespace x0
