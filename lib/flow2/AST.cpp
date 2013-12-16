#include <x0/flow2/AST.h>
#include <x0/flow2/vm/Signature.h>
#include <x0/Buffer.h>
#include <algorithm>

namespace x0 {

// {{{ SymbolTable
SymbolTable::SymbolTable(SymbolTable* outer, const std::string& name) :
	symbols_(),
	outerTable_(outer),
    name_(name)
{
}

SymbolTable::~SymbolTable()
{
	for (auto symbol: symbols_)
		delete symbol;
}

Symbol* SymbolTable::appendSymbol(std::unique_ptr<Symbol> symbol)
{
    assert(symbol->owner_ == nullptr && "Cannot re-own symbol.");
    symbol->owner_ = this;
	symbols_.push_back(symbol.get());
	return symbol.release();
}

void SymbolTable::removeSymbol(Symbol* symbol)
{
    auto i = std::find(symbols_.begin(), symbols_.end(), symbol);

    assert(i != symbols_.end() && "Failed removing symbol from symbol table.");

    symbols_.erase(i);
    symbol->owner_ = nullptr;
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
			if (symbol->name() == name) {
                //printf("SymbolTable(%s).lookup: \"%s\" (found)\n", name_.c_str(), name.c_str());
				return symbol;
            }

	// search outer
	if (method & Lookup::Outer)
		if (outerTable_) {
            //printf("SymbolTable(%s).lookup: \"%s\" (not found, recurse to parent)\n", name_.c_str(), name.c_str());
			return outerTable_->lookup(name, method);
        }

    //printf("SymbolTable(%s).lookup: \"%s\" -> not found\n", name_.c_str(), name.c_str());
	return nullptr;
}
// }}}

void Variable::accept(ASTVisitor& v)
{
	v.visit(*this);
}

Handler* Unit::findHandler(const std::string& name)
{
	if (class Handler* handler = dynamic_cast<class Handler*>(scope()->lookup(name, Lookup::Self)))
		return handler;

	return nullptr;
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

BinaryExpr::BinaryExpr(FlowVM::Opcode op, std::unique_ptr<Expr>&& lhs, std::unique_ptr<Expr>&& rhs) :
	Expr(rhs->location() - lhs->location()),
	operator_(op),
	lhs_(std::move(lhs)),
	rhs_(std::move(rhs))
{
}

void BinaryExpr::accept(ASTVisitor& v) {
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

void CallStmt::accept(ASTVisitor& v) {
	v.visit(*this);
}

void FunctionCallExpr::accept(ASTVisitor& v) {
	v.visit(*this);
}

void BuiltinFunction::accept(ASTVisitor& v) {
	v.visit(*this);
}

void VariableExpr::accept(ASTVisitor& v) {
	v.visit(*this);
}

void CondStmt::accept(ASTVisitor& v) {
	v.visit(*this);
}

void AssignStmt::accept(ASTVisitor& v) {
	v.visit(*this);
}

void BuiltinHandler::accept(ASTVisitor& v) {
	v.visit(*this);
}

void Handler::implement(std::unique_ptr<SymbolTable>&& table, std::unique_ptr<Stmt>&& body)
{
    scope_ = std::move(table);
    body_ = std::move(body);
}

bool CallStmt::setArgs(ExprList&& args)
{
    args_ = std::move(args);
    if (!args_.empty()) {
        location().update(args_.back()->location().end);
    }

    return true;
}

// {{{ type system
FlowType UnaryExpr::getType() const
{
    return resultType(op());
}

FlowType BinaryExpr::getType() const
{
    return resultType(op());
}

template<> FlowType LiteralExpr<RegExp>::getType() const
{
    return FlowType::RegExp;
}

template<> FlowType LiteralExpr<Cidr>::getType() const
{
    return FlowType::Cidr;
}

template<> FlowType LiteralExpr<bool>::getType() const
{
    return FlowType::Boolean;
}

template<> FlowType LiteralExpr<IPAddress>::getType() const
{
    return FlowType::IPAddress;
}

template<> FlowType LiteralExpr<long long>::getType() const
{
    return FlowType::Number;
}

template<> FlowType LiteralExpr<std::string>::getType() const
{
    return FlowType::String;
}

FlowType FunctionCallExpr::getType() const
{
    return callee_->signature().returnType();
}

FlowType VariableExpr::getType() const
{
    return variable_->initializer()->getType();
}

FlowType HandlerRefExpr::getType() const
{
    return FlowType::Handler;
}
// }}}

} // namespace x0
