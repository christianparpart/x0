#include <x0/flow2/ASTPrinter.h>
#include <x0/flow2/AST.h>

namespace x0 {

inline std::string escape(char value) // {{{
{
	switch (value) {
		case '\t': return "<TAB>";
		case '\r': return "<CR>";
		case '\n': return "<LF>";
		case ' ': return "<SPACE>";
		default: break;
	}
	if (std::isprint(value)) {
		std::string s;
		s += value;
		return s;
	} else {
		char buf[16];
		snprintf(buf, sizeof(buf), "0x%02X", value);
		return buf;
	}
} // }}}
inline std::string escape(const std::string& value) // {{{
{
	std::string result;

	for (const char ch: value)
		result += escape(ch);

	return result;
} // }}}

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
		std::printf("  ");
}

void ASTPrinter::print(const char* title, ASTNode* node)
{
	enter();
	if (node) {
		printf("%s\n", title);
		enter();
		node->accept(*this);
		leave();
	} else {
		printf("%s (nil)\n", title);
	}
	leave();
}

void ASTPrinter::visit(Variable& variable)
{
	printf("Variable: %s as %s\n", variable.name().c_str(), tos(variable.initializer()->getType()).c_str());
	print("initializer", variable.initializer());
}

void ASTPrinter::visit(Handler& handler)
{
	printf("Handler: %s\n", handler.name().c_str());

	enter();
        if (handler.isForwardDeclared()) {
            printf("handler is forward-declared (unresolved)\n");
        } else {
            printf("scope:\n");
            enter();
                for (Symbol* symbol: *handler.scope())
                    symbol->accept(*this);
            leave();

            printf("body:\n");
            enter();
                handler.body()->accept(*this);
            leave();
        }
	leave();
}

void ASTPrinter::visit(BuiltinFunction& symbol)
{
	printf("BuiltinFunction: %s\n", symbol.signature().to_s().c_str());
}

void ASTPrinter::visit(BuiltinHandler& symbol)
{
	printf("BuiltinHandler: %s\n", symbol.signature().to_s().c_str());
}

void ASTPrinter::visit(Unit& unit)
{
	printf("Unit: %s\n", unit.name().c_str());

	enter();
	for (Symbol* symbol: *unit.scope())
		symbol->accept(*this);

	leave();
}

void ASTPrinter::visit(UnaryExpr& expr)
{
	printf("UnaryExpr: %s\n", mnemonic(expr.op()));
	print("subExpr", expr.subExpr());
}

void ASTPrinter::visit(BinaryExpr& expr)
{
	printf("BinaryExpr: %s\n", mnemonic(expr.op()));
	enter();
		printf("lhs:\n");
		enter();
			expr.leftExpr()->accept(*this);
		leave();
	leave();
	enter();
		printf("rhs:\n");
		enter();
			expr.rightExpr()->accept(*this);
		leave();
	leave();
}

void ASTPrinter::visit(FunctionCallExpr& expr)
{
	printf("FunctionCallExpr TODO\n");
}

void ASTPrinter::visit(VariableExpr& expr)
{
	printf("VariableExpr: %s\n", expr.variable()->name().c_str());
}

void ASTPrinter::visit(HandlerRefExpr& handlerRef)
{
	printf("HandlerRefExpr: %s\n", handlerRef.handler()->name().c_str());
}

void ASTPrinter::visit(StringExpr& string)
{
	printf("StringExpr: \"%s\"\n", escape(string.value()).c_str());
}

void ASTPrinter::visit(NumberExpr& number)
{
	printf("NumberExpr: %lli\n", number.value());
}

void ASTPrinter::visit(BoolExpr& boolean)
{
	printf("BoolExpr: %s\n", boolean.value() ? "true" : "false");
}

void ASTPrinter::visit(RegExpExpr& regexp)
{
	printf("RegExpExpr: /%s/\n", regexp.value().c_str());
}

void ASTPrinter::visit(IPAddressExpr& ipaddr)
{
	printf("IPAddressExpr: %s\n", ipaddr.value().str().c_str());
}

void ASTPrinter::visit(CidrExpr& cidr)
{
	printf("CidrExpr: %s\n", cidr.value().str().c_str());
}

void ASTPrinter::visit(ExprStmt& stmt)
{
	printf("ExprStmt\n");
}

void ASTPrinter::visit(CompoundStmt& compoundStmt)
{
	printf("CompoundStmt (%d statements)\n", compoundStmt.count());
	enter();
	for (auto& stmt: compoundStmt) {
		stmt->accept(*this);
	}
	leave();
}

void ASTPrinter::visit(CondStmt& cond)
{
	printf("CondStmt\n");
	print("condition", cond.condition());
	print("thenStmt", cond.thenStmt());
	print("elseStmt", cond.elseStmt());
}

void ASTPrinter::visit(AssignStmt& assign)
{
	printf("AssignStmt\n");
	enter();
		printf("lhs(var): %s\n", assign.variable()->name().c_str());
	leave();
	print("rhs", assign.expression());
}

void ASTPrinter::visit(CallStmt& call)
{
	printf("CallStmt: %s\n", call.callee()->name().c_str());
    for (int i = 1, e = call.args().size(); i <= e; ++i) {
        char title[256];
        snprintf(title, sizeof(title), "param %d:", i);
        print(title, call.args()[i - 1]);
    }
}

} // namespace x0

