#include <x0/flow2/AST.h>
#include <algorithm>

namespace x0 {

// {{{ SymbolTable
SymbolTable::SymbolTable(SymbolTable* outer) :
	symbols_(),
	parents_(),
	outerTable_(outer)
{
}

SymbolTable::~SymbolTable()
{
	for (auto parent: parents_)
		delete parent;

	for (auto symbol: symbols_)
		delete symbol;
}

SymbolTable* SymbolTable::appendParent(SymbolTable* table)
{
	parents_.push_back(table);
	return table;
}

SymbolTable* SymbolTable::parentAt(size_t i) const
{
	return parents_[i];
}

void SymbolTable::removeParent(SymbolTable* table)
{
}

size_t SymbolTable::parentCount() const
{
	return parents_.size();
}

Symbol* SymbolTable::appendSymbol(Symbol* symbol)
{
	symbols_.push_back(symbol);
	return symbol;
}

void SymbolTable::removeSymbol(Symbol* symbol)
{
	auto i = std::find(symbols_.begin(), symbols_.end(), symbol);

	if (i != symbols_.end())
		symbols_.erase(i);
}

Symbol* SymbolTable::symbolAt(size_t i) const
{
	return symbols_[i];
}

size_t SymbolTable::symbolCount() const
{
	return symbols_.size();
}

Symbol* SymbolTable::lookup(const std::string& name, Lookup method) const
{
	// search local
	if (method & Lookup::Self)
		for (auto symbol: symbols_)
			if (symbol->name() == name)
				return symbol;

	// search parents
	if (method & Lookup::Parents)
		for (auto parent: parents_)
			if (Symbol* result = parent->lookup(name, method))
				return result;

	// search outer
	if (method & Lookup::Outer)
		if (outerTable_)
			return outerTable_->lookup(name, method);

	return nullptr;
}
// }}}

void Variable::accept(ASTVisitor& v)
{
	v.visit(*this);
}

void Unit::accept(ASTVisitor& v) {
	v.visit(*this);
}

void Handler::accept(ASTVisitor& v) {
	v.visit(*this);
}

void UnaryExpr::accept(ASTVisitor& v) {
	v.visit(*this);
}

BinaryExpr::BinaryExpr(FlowToken op, std::unique_ptr<Expr>&& lhs, std::unique_ptr<Expr>&& rhs) :
	Expr(rhs->location() - lhs->location()),
	operator_(op),
	lhs_(std::move(lhs)),
	rhs_(std::move(rhs))
{
}

void BinaryExpr::accept(ASTVisitor& v) {
	v.visit(*this);
}

void ListExpr::push_back(std::unique_ptr<Expr> expr) {
	location_.update(expr->location().end);
	list_.push_back(std::move(expr));
}

void ListExpr::accept(ASTVisitor& v) {
	v.visit(*this);
}

void HandlerRefExpr::accept(ASTVisitor& v) {
	v.visit(*this);
}

void CompoundStmt::push_back(std::unique_ptr<Stmt>&& stmt)
{
	location_.update(stmt->location().end);
	statements_.push_back(std::move(stmt));
}

void CompoundStmt::accept(ASTVisitor& v) {
	v.visit(*this);
}

void ExprStmt::accept(ASTVisitor& v) {
	v.visit(*this);
}

void HandlerCallStmt::accept(ASTVisitor& v) {
	v.visit(*this);
}

void FunctionCallExpr::accept(ASTVisitor& v) {
	v.visit(*this);
}

void BuiltinHandlerCallStmt::accept(ASTVisitor& v) {
	v.visit(*this);
}

void BuiltinFunction::accept(ASTVisitor& v) {
	v.visit(*this);
}

void VariableExpr::accept(ASTVisitor& v) {
	v.visit(*this);
}

} // namespace x0
