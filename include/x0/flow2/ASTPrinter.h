#include <x0/flow2/ASTVisitor.h>

namespace x0 {

class ASTNode;

class X0_API ASTPrinter :
	public ASTVisitor
{
public:
	static void print(ASTNode* node);

private:
	ASTPrinter();

	int depth_;

	void enter() { ++depth_; }
	void leave() { --depth_; }
	void prefix();
	void print(const char* title, ASTNode* node);

	void printf(const char* msg) { prefix(); std::printf("%s", msg); }
	template<typename... Args> void printf(const char* fmt, Args... args) { prefix(); std::printf(fmt, args...); }

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
};

} // namespace x0
