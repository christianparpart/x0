#include <x0/flow2/FlowEngine.h>
#include <x0/flow2/FlowBackend.h>
#include <x0/flow2/AST.h>
#include <x0/DebugLogger.h>

namespace x0 {

enum class FlowOpcode {
	Nop,

	ICastB, // cast int: bool
	ICastS, // cast int: string
	SCastB, // cast string: bool

	ILoad, // load int
	BLoad, // load bool
	SLoad, // load string

	IStore, // store int
	BStore, // store bool
	SStore, // store string

	ICmpEQ, // ==(int, int): bool
	ICmpNE, // ==(int, int): bool
	ICmpLE, // <=(int, int): bool
	ICmpGE, // >=(int, int): bool
	ICmpLT, // <(int, int): bool
	ICmpGT, // >(int, int): bool
	IAdd,   // +int, int): bool
	ISum,   // -int, int): bool
	IMul,   // *(int, int): int
	IDiv,   // /(int, int): int
	IMod,   // %(int, int): int
	IPow,   // **(int, int): int
	INeg,   // neg(int): int

	CmpEQ,  // ==(bool, bool): bool
	CmpNE,  // !=(bool, bool): bool

	SCmpEQ, // ==(str, str): bool
	SCmpNE, // !=(str, str): bool
	SCmpLE, // <=(str, str): bool
	SCmpGE, // >=(str, str): bool
	SCmpLT, // <(str, str): bool
	SCmpGT, // >(str, str): bool
	SCmpRE, // =~(str, regex): bool

	HCall,  // hcall(handler): bool
	CallI,  // fcall(function, args...): int
	CallB,  // fcall(function, args...): bool
	CallS,  // fcall(function, args...): str

	JT,     // jump if true
	JF,     // jump if false
	RET,	// return call
};

#define FLOW_DEBUG_CODEGEN 1 // {{{
#if defined(FLOW_DEBUG_CODEGEN)
// {{{ trace
static size_t fnd = 0;
struct fntrace3 {
	std::string msg_;

	fntrace3(const char* msg) : msg_(msg)
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

	~fntrace3() {
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
#	define FNTRACE() fntrace3 _(__PRETTY_FUNCTION__)
#	define TRACE(level, msg...) XZERO_DEBUG("FlowEngine", (level), msg)
#else
#	define FNTRACE() /*!*/
#	define TRACE(level, msg...) /*!*/
#endif // }}}

FlowEngine::FlowEngine(FlowBackend* backend) :
	data_(),
	backend_(backend),
	userdata_(nullptr),
	currentNode_(nullptr),
	result_(nullptr)
{
}

FlowEngine::~FlowEngine()
{
}

bool FlowEngine::compile(Unit* unit)
{
	FNTRACE();
	return true;
}

void FlowEngine::dump()
{
	FNTRACE();
}

bool FlowEngine::run(Handler* handler, void* userdata)
{
	userdata_ = userdata;

	visit(*handler);
	return false;
}

void FlowEngine::visit(Unit& symbol)
{
	FNTRACE();
}

void FlowEngine::visit(Variable& variable)
{
	FNTRACE();

	result_ = nullptr; // TODO
}

void FlowEngine::visit(Handler& handler)
{
	FNTRACE();
}

void FlowEngine::visit(BuiltinFunction& symbol)
{
	FNTRACE();
}

void FlowEngine::visit(BuiltinHandler& symbol)
{
	FNTRACE();
}

void FlowEngine::visit(UnaryExpr& expr)
{
	FNTRACE();
}

void FlowEngine::visit(BinaryExpr& expr)
{
	FNTRACE();
}

void FlowEngine::visit(FunctionCallExpr& expr)
{
	FNTRACE();
}

void FlowEngine::visit(VariableExpr& expr)
{
	FNTRACE();
}

void FlowEngine::visit(HandlerRefExpr& expr)
{
	FNTRACE();
}

void FlowEngine::visit(ListExpr& expr)
{
	FNTRACE();
}

void FlowEngine::visit(StringExpr& expr)
{
	FNTRACE();
}

void FlowEngine::visit(NumberExpr& expr)
{
	FNTRACE();
}

void FlowEngine::visit(BoolExpr& expr)
{
	FNTRACE();
}

void FlowEngine::visit(RegExpExpr& expr)
{
	FNTRACE();
}

void FlowEngine::visit(IPAddressExpr& expr)
{
	FNTRACE();
}

void FlowEngine::visit(CidrExpr& cidr)
{
	FNTRACE();
}

void FlowEngine::visit(ExprStmt& stmt)
{
	FNTRACE();
}

void FlowEngine::visit(CompoundStmt& stmt)
{
	FNTRACE();
}

void FlowEngine::visit(CondStmt& stmt)
{
	FNTRACE();
}

void FlowEngine::visit(AssignStmt& stmt)
{
	FNTRACE();
}

void FlowEngine::visit(CallStmt& stmt)
{
	FNTRACE();
}

} // namespace x0
