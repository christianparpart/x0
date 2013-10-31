#pragma once

#include <x0/flow2/FlowValue.h>
#include <x0/flow2/ASTVisitor.h>
#include <x0/Api.h>
#include <unordered_map>
#include <deque>

namespace x0 {

class FlowBackend;
class ASTNode;

class X0_API FlowEngine : public ASTVisitor {
public:
	explicit FlowEngine(FlowBackend* backend);
	virtual ~FlowEngine();

	// loads unit into engine
	bool compile(Unit* unit);

	void dump();

	bool run(Handler* handler, void* userdata);

private:
	std::unordered_map<Handler*, uint8_t*> handlerEntryPoints_;
	uint8_t* program_;
	uint8_t* pc_;

	void istore();
	void iload();

private:
	std::vector<FlowValue> data_;

	FlowBackend* backend_;
	void* userdata_;
	ASTNode* currentNode_;
	FlowValue* result_;

	// {{{ byte code generator
	// symbols
	virtual void visit(Unit& symbol);
	virtual void visit(Variable& variable);
	virtual void visit(Handler& handler);
	virtual void visit(BuiltinFunction& symbol);
	virtual void visit(BuiltinHandler& symbol);

	// expressions
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

	// statements
	virtual void visit(ExprStmt& stmt);
	virtual void visit(CompoundStmt& stmt);
	virtual void visit(CondStmt& stmt);
	virtual void visit(AssignStmt& stmt);
	virtual void visit(CallStmt& stmt);
	// }}}
};

} // namespace x0 
