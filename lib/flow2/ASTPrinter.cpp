#include <x0/flow2/ASTPrinter.h>
#include <x0/flow2/AST.h>

namespace x0 {

void ASTPrinter::print(ASTNode* node)
{
	ASTPrinter printer;
	node->accept(printer);
}

ASTPrinter::ASTPrinter() :
	depth_(0)
{
}

void ASTPrinter::prefix()
{
	for (int i = 0; i < depth_; ++i)
		printf("  ");
}

void ASTPrinter::visit(Variable& variable)
{
	prefix(); printf("Variable: '%s'\n", variable.name().c_str());
	enter();
		prefix(); printf("initializer:\n");
		enter();
			variable.initializer()->accept(*this);
		leave();
	leave();
}

void ASTPrinter::visit(Handler& handler)
{
	prefix(); printf("Handler: %s\n", handler.name().c_str());

	enter();
	prefix(); printf("scope:\n");
		enter();
			for (Symbol* symbol: *handler.scope())
				symbol->accept(*this);
		leave();

		prefix(); printf("body:\n");
		enter();
			handler.body()->accept(*this);
		leave();
	leave();
}

void ASTPrinter::visit(BuiltinFunction& symbol)
{
}

void ASTPrinter::visit(BuiltinHandler& symbol)
{
}

void ASTPrinter::visit(Unit& unit)
{
	prefix(); printf("Unit: %s\n", unit.name().c_str());

	enter();
	for (Symbol* symbol: *unit.scope())
		symbol->accept(*this);

	leave();
}

void ASTPrinter::visit(UnaryExpr& expr)
{
}

void ASTPrinter::visit(BinaryExpr& expr)
{
	prefix(); printf("BinaryExpr: %s\n", expr.op().c_str());
	enter();
		prefix(); printf("lhs:\n");
		enter();
			expr.leftExpr()->accept(*this);
		leave();
	leave();
	enter();
		prefix(); printf("rhs:\n");
		enter();
			expr.rightExpr()->accept(*this);
		leave();
	leave();
}

void ASTPrinter::visit(FunctionCallExpr& expr)
{
}

void ASTPrinter::visit(VariableExpr& expr)
{
	prefix(); printf("VariableExpr: %s\n", expr.variable()->name().c_str());
}

void ASTPrinter::visit(HandlerRefExpr& expr)
{
}

void ASTPrinter::visit(ListExpr& expr)
{
}

void ASTPrinter::visit(StringExpr& expr)
{
}

void ASTPrinter::visit(NumberExpr& expr)
{
	prefix(); printf("NumberExpr: %lli\n", expr.value());
}

void ASTPrinter::visit(BoolExpr& expr)
{
}

void ASTPrinter::visit(RegExpExpr& expr)
{
}

void ASTPrinter::visit(IPAddressExpr& expr)
{
}

void ASTPrinter::visit(ExprStmt& stmt)
{
	prefix(); printf("ExprStmt\n");
}

void ASTPrinter::visit(CompoundStmt& compoundStmt)
{
	prefix(); printf("CompoundStmt\n");
	enter();
	for (auto& stmt: compoundStmt) {
		stmt->accept(*this);
	}
	leave();
}

void ASTPrinter::visit(CondStmt& stmt)
{
	prefix(); printf("CondStmt\n");
}

void ASTPrinter::visit(AssignStmt& assign)
{
	prefix(); printf("AssignStmt\n");
	enter();
		prefix(); printf("lhs: %s\n", assign.variable()->name().c_str());
		enter();
			assign.variable()->accept(*this);
		leave();
		prefix(); printf("rhs\n");
		enter();
			assign.expression()->accept(*this);
		leave();
	leave();
}

void ASTPrinter::visit(HandlerCallStmt& stmt)
{
	prefix(); printf("HandlerCallStmt\n");
}

void ASTPrinter::visit(BuiltinHandlerCallStmt& stmt)
{
	prefix(); printf("BuiltinHandlerCallStmt\n");
}

} // namespace x0

