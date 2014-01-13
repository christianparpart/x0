#include <x0/flow/AST.h>
#include <x0/flow/ASTPrinter.h>
#include <x0/flow/vm/Signature.h>
#include <x0/flow/vm/NativeCallback.h>
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

// {{{ ParamList
ParamList& ParamList::operator=(ParamList&& v)
{
    isNamed_ = v.isNamed_;
    names_ = std::move(v.names_);
    values_ = std::move(v.values_);

    return *this;
}

ParamList::~ParamList()
{
    for (Expr* arg: values_) {
        delete arg;
    }
}

void ParamList::push_back(const std::string& name, std::unique_ptr<Expr>&& arg)
{
    assert(names_.size() == values_.size() && "Cannot mix named with unnamed parameters.");

    names_.push_back(name);
    values_.push_back(arg.release());
}

void ParamList::push_back(std::unique_ptr<Expr>&& arg)
{
    assert(names_.empty() && "Cannot mix unnamed with named parameters.");

    values_.push_back(arg.release());
}

void ParamList::replace(size_t index, std::unique_ptr<Expr>&& value)
{
    assert(index < values_.size() && "Index out of bounds.");

    delete values_[index];
    values_[index] = value.release();
}

bool ParamList::replace(const std::string& name, std::unique_ptr<Expr>&& value)
{
    assert(!names_.empty() && "Cannot mix unnamed with named parameters.");
    for (size_t i = 0, e = names_.size(); i != e; ++i) {
        if (names_[i] == name) {
            delete values_[i];
            values_[i] = value.release();
            return true;
        }
    }

    names_.push_back(name);
    values_.push_back(value.release());
    return false;
}

bool ParamList::contains(const std::string& name) const
{
    for (const auto& arg: names_)
        if (name == arg)
            return true;

    return false;
}

void ParamList::swap(size_t source, size_t dest)
{
    assert(source <= size());
    assert(dest <= size());

    //printf("Swapping parameter #%zu (%s) with #%zu (%s).\n", source, names_[source].c_str(), dest, names_[dest].c_str());

    std::swap(names_[source], names_[dest]);
    std::swap(values_[source], values_[dest]);
}

size_t ParamList::size() const
{
    return values_.size();
}

bool ParamList::empty() const
{
    return values_.empty();
}

std::pair<std::string, Expr*> ParamList::at(size_t offset) const
{
    return std::make_pair(isNamed() ? names_[offset] : "", values_[offset]);
}

void ParamList::reorder(const FlowVM::NativeCallback* native, std::vector<std::string>* superfluous)
{
    //dump("ParamList::reorder (before)");

    size_t argc = std::min(native->signature().args().size(), names_.size());

    assert(values_.size() >= argc && "Argument count mismatch.");

    for (int i = 0; i != argc; ++i) {
        const std::string& localName = names_[i];
        const std::string& otherName = native->getNameAt(i);
        int nativeIndex = native->find(localName);

        if (nativeIndex == i) {
            // OK: argument at correct position
            continue;
        }

        if (nativeIndex == -1) {
            superfluous->push_back(localName);
            int other = find(otherName);
            if (other != -1) {
                // OK: found expected arg at [other].
                swap(i, other);
            }
            continue;
        }

        if (localName != otherName) {
            int other = find(otherName);
            assert(other != -1);
            swap(i, other);
        }
    }

    //dump("ParamList::reorder (after)");
}

void ParamList::dump(const char* title)
{
    if (title && *title) {
        printf("%s\n", title);
    }
    for (int i = 0, e = names_.size(); i != e; ++i) {
        printf("%16s: ", names_[i].c_str());
        ASTPrinter::print(values_[i]);
    }
}

int ParamList::find(const std::string& name) const
{
    for (int i = 0, e = names_.size(); i != e; ++i) {
        if (names_[i] == name)
            return i;
    }
    return -1;
}

FlowLocation ParamList::location() const {
    if (values_.empty())
        return FlowLocation();

    return FlowLocation(
        front()->location().filename,
        front()->location().begin,
        back()->location().end
    );
}
// }}}

void Variable::visit(ASTVisitor& v)
{
	v.accept(*this);
}

Handler* Unit::findHandler(const std::string& name)
{
	if (class Handler* handler = dynamic_cast<class Handler*>(scope()->lookup(name, Lookup::Self)))
		return handler;

	return nullptr;
}

void Unit::visit(ASTVisitor& v) {
	v.accept(*this);
}

void Handler::visit(ASTVisitor& v) {
	v.accept(*this);
}

void UnaryExpr::visit(ASTVisitor& v) {
	v.accept(*this);
}

BinaryExpr::BinaryExpr(FlowVM::Opcode op, std::unique_ptr<Expr>&& lhs, std::unique_ptr<Expr>&& rhs) :
	Expr(rhs->location() - lhs->location()),
	operator_(op),
	lhs_(std::move(lhs)),
	rhs_(std::move(rhs))
{
}

void BinaryExpr::visit(ASTVisitor& v) {
	v.accept(*this);
}

void HandlerRefExpr::visit(ASTVisitor& v) {
	v.accept(*this);
}

void CompoundStmt::push_back(std::unique_ptr<Stmt>&& stmt)
{
	location_.update(stmt->location().end);
	statements_.push_back(std::move(stmt));
}

void CompoundStmt::visit(ASTVisitor& v) {
	v.accept(*this);
}

void ExprStmt::visit(ASTVisitor& v) {
	v.accept(*this);
}

void HandlerCall::visit(ASTVisitor& v) {
	v.accept(*this);
}

void FunctionCall::visit(ASTVisitor& v) {
	v.accept(*this);
}

void BuiltinFunction::visit(ASTVisitor& v) {
	v.accept(*this);
}

void VariableExpr::visit(ASTVisitor& v) {
	v.accept(*this);
}

void CondStmt::visit(ASTVisitor& v) {
	v.accept(*this);
}

void AssignStmt::visit(ASTVisitor& v) {
	v.accept(*this);
}

void BuiltinHandler::visit(ASTVisitor& v) {
	v.accept(*this);
}

void Handler::implement(std::unique_ptr<SymbolTable>&& table, std::unique_ptr<Stmt>&& body)
{
    scope_ = std::move(table);
    body_ = std::move(body);
}

bool HandlerCall::setArgs(ParamList&& args)
{
    args_ = std::move(args);
    if (!args_.empty()) {
        location().update(args_.back()->location().end);
    }

    return true;
}

MatchStmt::MatchStmt(const FlowLocation& loc, std::unique_ptr<Expr>&& cond, FlowVM::MatchClass op, std::list<MatchCase>&& cases, std::unique_ptr<Stmt>&& elseStmt) :
    Stmt(loc),
    cond_(std::move(cond)),
    op_(op),
    cases_(std::move(cases)),
    elseStmt_(std::move(elseStmt))
{
}

MatchStmt::MatchStmt(MatchStmt&& other) :
    Stmt(other.location()),
    cond_(std::move(other.cond_)),
    op_(std::move(other.op_)),
    cases_(std::move(other.cases_))
{
}

MatchStmt& MatchStmt::operator=(MatchStmt&& other)
{
    setLocation(other.location());
    cond_ = std::move(other.cond_);
    op_ = std::move(other.op_);
    cases_ = std::move(other.cases_);

    return *this;
}

MatchStmt::~MatchStmt()
{
}

void MatchStmt::visit(ASTVisitor& v)
{
    v.accept(*this);
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

template<> X0_API FlowType LiteralExpr<RegExp>::getType() const
{
    return FlowType::RegExp;
}

template<> X0_API FlowType LiteralExpr<Cidr>::getType() const
{
    return FlowType::Cidr;
}

template<> X0_API FlowType LiteralExpr<bool>::getType() const
{
    return FlowType::Boolean;
}

template<> X0_API FlowType LiteralExpr<IPAddress>::getType() const
{
    return FlowType::IPAddress;
}

template<> X0_API FlowType LiteralExpr<long long>::getType() const
{
    return FlowType::Number;
}

template<> X0_API FlowType LiteralExpr<std::string>::getType() const
{
    return FlowType::String;
}

FlowType FunctionCall::getType() const
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
