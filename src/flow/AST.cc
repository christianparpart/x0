// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <flow/AST.h>
#include <flow/ASTPrinter.h>
#include <flow/NativeCallback.h>
#include <flow/Signature.h>

#include <fmt/format.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <memory>

namespace xzero::flow {

// {{{ SymbolTable
SymbolTable::SymbolTable(SymbolTable* outer, const std::string& name)
    : symbols_(), outerTable_(outer), name_(name) {}

SymbolTable::~SymbolTable() {
}

Symbol* SymbolTable::appendSymbol(std::unique_ptr<Symbol> symbol) {
  assert(symbol->owner_ == nullptr && "Cannot re-own symbol.");
  symbol->owner_ = this;
  symbols_.push_back(std::move(symbol));
  return symbols_.back().get();
}

void SymbolTable::removeSymbol(Symbol* symbol) {
  auto i = std::find_if(symbols_.begin(), symbols_.end(),
                        [&](auto& x) { return x.get() == symbol; });

  assert(i != symbols_.end() && "Failed removing symbol from symbol table.");

  symbol->owner_ = nullptr;
  symbols_.erase(i);
}

Symbol* SymbolTable::symbolAt(size_t i) const {
  return symbols_[i].get();
}

size_t SymbolTable::symbolCount() const {
  return symbols_.size();
}

Symbol* SymbolTable::lookup(const std::string& name, Lookup method) const {
  // search local
  if (method & Lookup::Self)
    for (auto& symbol : symbols_)
      if (symbol->name() == name) {
        // printf("SymbolTable(%s).lookup: \"%s\" (found)\n", name_.c_str(),
        // name.c_str());
        return symbol.get();
      }

  // search outer
  if (method & Lookup::Outer)
    if (outerTable_) {
      // printf("SymbolTable(%s).lookup: \"%s\" (not found, recurse to
      // parent)\n", name_.c_str(), name.c_str());
      return outerTable_->lookup(name, method);
    }

  // printf("SymbolTable(%s).lookup: \"%s\" -> not found\n", name_.c_str(),
  // name.c_str());
  return nullptr;
}

Symbol* SymbolTable::lookup(const std::string& name, Lookup method,
                            std::list<Symbol*>* result) const {
  assert(result != nullptr);

  // search local
  if (method & Lookup::Self)
    for (auto& symbol : symbols_)
      if (symbol->name() == name)
        result->push_back(symbol.get());

  // search outer
  if (method & Lookup::Outer)
    if (outerTable_)
      outerTable_->lookup(name, method, result);

  return !result->empty() ? result->front() : nullptr;
}
// }}}
// {{{ CallableSym
CallableSym::CallableSym(Type t, const NativeCallback* cb,
                         const SourceLocation& loc)
    : Symbol(t, cb->signature().name(), loc),
      nativeCallback_(cb),
      sig_(nativeCallback_->signature()) {}

CallableSym::CallableSym(const std::string& name, const SourceLocation& loc)
    : Symbol(Type::Handler, name, loc), nativeCallback_(nullptr), sig_() {
  sig_.setName(name);
  sig_.setReturnType(LiteralType::Boolean);
}

const Signature& CallableSym::signature() const {
  if (nativeCallback_)
    return nativeCallback_->signature();

  return sig_;
}

static inline void completeDefaultValue(
    ParamList& args,
    LiteralType type,
    const NativeCallback::DefaultValue& dv,
    const std::string& name) { // {{{
  // printf("completeDefaultValue(type:%s, name:%s)\n", tos(type).c_str(),
  // name.c_str());

  static const SourceLocation loc;

  switch (type) {
    case LiteralType::Boolean:
      args.push_back(
          name,
          std::make_unique<BoolExpr>(std::get<bool>(dv), loc));
      break;
    case LiteralType::Number:
      args.push_back(
          name,
          std::make_unique<NumberExpr>(std::get<FlowNumber>(dv), loc));
      break;
    case LiteralType::String:
      args.push_back(
          name,
          std::make_unique<StringExpr>(std::get<FlowString>(dv), loc));
      break;
    case LiteralType::IPAddress:
      args.push_back(
          name,
          std::make_unique<IPAddressExpr>(std::get<IPAddress>(dv), loc));
      break;
    case LiteralType::Cidr:
      args.push_back(
          name,
          std::make_unique<CidrExpr>(std::get<Cidr>(dv), loc));
      break;
    default:
      fprintf(stderr, "Unsupported type in default completion. Please report me. I am a bug.");
      abort();
      // reportError("Cannot complete named paramter \"%s\" in callee \"%s\".
      // Unsupported type <%s>.",
      //        name.c_str(), this->name().c_str(), tos(type).c_str());
      break;
  }
}  // }}}

bool CallableSym::isDirectMatch(const ParamList& params) const {
  if (params.size() != nativeCallback_->signature().args().size())
    return false;

  for (size_t i = 0, e = params.size(); i != e; ++i) {
    if (params.isNamed() && nativeCallback_->getParamNameAt(i) != params[i].first)
      return false;

    LiteralType expectedType = signature().args()[i];
    LiteralType givenType = params.values()[i]->getType();

    if (givenType != expectedType) {
      return false;
    }
  }
  return true;
}

bool CallableSym::tryMatch(ParamList& params, std::string* errorMessage) const {
  // printf("CallableSym(%s).tryMatch()\n", name().c_str());

  const NativeCallback* native = nativeCallback();

  if (params.empty() && (!native || native->signature().args().empty()))
    return true;

  if (params.isNamed()) {
    if (!native->parametersNamed()) {
      *errorMessage = fmt::format(
          "Callee \"{}\" invoked with named parameters, but no names provided "
          "by runtime.",
          name());
      return false;
    }

    int argc = signature().args().size();
    for (int i = 0; i != argc; ++i) {
      const auto& paramName = native->getParamNameAt(i);
      if (!params.contains(paramName)) {
        const NativeCallback::DefaultValue& defaultValue =
            native->getDefaultParamAt(i);
        if (std::holds_alternative<std::monostate>(defaultValue)) {
          *errorMessage = fmt::format(
              "Callee \"{}\" invoked without required named parameter \"{}\".",
              name(), paramName);
          return false;
        }
        LiteralType type = signature().args()[i];
        completeDefaultValue(params, type, defaultValue, paramName);
      }
    }

    // reorder params (and detect superfluous params)
    std::vector<std::string> superfluous;
    params.reorder(native, &superfluous);

    if (!superfluous.empty()) {
      std::string t;
      for (const auto& s : superfluous) {
        if (!t.empty()) t += ", ";
        t += "\"";
        t += s;
        t += "\"";
      }
      *errorMessage = fmt::format(
          "Superfluous arguments passed to callee \"{}\": {}.", name(), t);
      return false;
    }

    return true;
  } else  // verify params positional
  {
    if (params.size() > signature().args().size()) {
      *errorMessage = fmt::format("Superfluous parameters to callee {}.", signature());
      return false;
    }

    for (size_t i = 0, e = params.size(); i != e; ++i) {
      LiteralType expectedType = signature().args()[i];
      LiteralType givenType = params.values()[i]->getType();
      if (givenType != expectedType) {
        *errorMessage = fmt::format(
            "Type mismatch in positional parameter {}, callee {}.",
            i + 1, signature());
        return false;
      }
    }

    for (size_t i = params.size(), e = signature().args().size(); i != e; ++i) {
      const NativeCallback::DefaultValue& defaultValue = native->getDefaultParamAt(i);
      if (std::holds_alternative<std::monostate>(defaultValue)) {
        *errorMessage = fmt::format(
            "No default value provided for positional parameter {}, callee {}.",
            i + 1, signature());
        return false;
      }

      const std::string& name = native->getParamNameAt(i);
      LiteralType type = native->signature().args()[i];
      completeDefaultValue(params, type, defaultValue, name);
    }

    Signature sig;
    sig.setName(this->name());
    sig.setReturnType(signature().returnType());  // XXX cheetah
    std::vector<LiteralType> argTypes;
    for (const auto& arg : params.values()) {
      argTypes.push_back(arg->getType());
    }
    sig.setArgs(argTypes);

    if (sig != signature()) {
      *errorMessage = fmt::format(
          "Callee parameter type signature mismatch: {} passed, but {} expected.",
          sig, signature());
      return false;
    }

    return true;
  }
}
// }}}
// {{{ ParamList
ParamList& ParamList::operator=(ParamList&& v) {
  isNamed_ = v.isNamed_;
  names_ = std::move(v.names_);
  values_ = std::move(v.values_);

  return *this;
}

ParamList::~ParamList() {
}

void ParamList::push_back(const std::string& name,
                          std::unique_ptr<Expr>&& arg) {
  if (isNamed())
    names_.push_back(name);

  values_.push_back(std::move(arg));
}

void ParamList::push_back(std::unique_ptr<Expr>&& arg) {
  assert(names_.empty() && "Cannot mix unnamed with named parameters.");

  values_.push_back(std::move(arg));
}

void ParamList::replace(size_t index, std::unique_ptr<Expr>&& value) {
  assert(index < values_.size() && "Index out of bounds.");

  values_[index] = std::move(value);
}

bool ParamList::replace(const std::string& name,
                        std::unique_ptr<Expr>&& value) {
  assert(!names_.empty() && "Cannot mix unnamed with named parameters.");
  assert(names_.size() == values_.size());

  for (size_t i = 0, e = names_.size(); i != e; ++i) {
    if (names_[i] == name) {
      values_[i] = std::move(value);
      return true;
    }
  }

  names_.push_back(name);
  values_.push_back(std::move(value));
  return false;
}

bool ParamList::contains(const std::string& name) const {
  for (const auto& arg : names_)
    if (name == arg)
      return true;

  return false;
}

void ParamList::swap(size_t source, size_t dest) {
  assert(source <= size());
  assert(dest <= size());

  // printf("Swapping parameter #%zu (%s) with #%zu (%s).\n", source,
  // names_[source].c_str(), dest, names_[dest].c_str());

  std::swap(names_[source], names_[dest]);
  std::swap(values_[source], values_[dest]);
}

size_t ParamList::size() const {
  return values_.size();
}

bool ParamList::empty() const {
  return values_.empty();
}

std::pair<std::string, Expr*> ParamList::at(size_t offset) const {
  return std::make_pair(isNamed() ? names_[offset] : "", values_[offset].get());
}

void ParamList::reorder(const NativeCallback* native,
                        std::vector<std::string>* superfluous) {
  // dump("ParamList::reorder (before)");

  size_t argc = std::min(native->signature().args().size(), names_.size());

  assert(values_.size() >= argc && "Argument count mismatch.");

  // printf("reorder: argc=%zu, names#=%zu\n", argc, names_.size());
  for (size_t i = 0; i != argc; ++i) {
    const std::string& localName = names_[i];
    const std::string& otherName = native->getParamNameAt(i);
    int nativeIndex = native->findParamByName(localName);

    // printf("reorder: localName=%s, otherName=%s, nindex=%i\n",
    // localName.c_str(), otherName.c_str(), nativeIndex);
    // ASTPrinter::print(values_[i]);

    if (static_cast<size_t>(nativeIndex) == i) {
      // OK: argument at correct position
      continue;
    }

    if (nativeIndex == -1) {
      int other = find(otherName);
      if (other != -1) {
        // OK: found expected arg at [other].
        swap(i, other);
      } else {
        superfluous->push_back(localName);
      }
      continue;
    }

    if (localName != otherName) {
      int other = find(otherName);
      assert(other != -1);
      swap(i, other);
    }
  }

  if (argc < names_.size()) {
    for (size_t i = argc, e = names_.size(); i != e; ++i) {
      superfluous->push_back(names_[i]);
    }
  }

  // dump("ParamList::reorder (after)");
}

void ParamList::dump(const char* title) {
  if (title && *title) {
    printf("%s\n", title);
  }
  for (int i = 0, e = size(); i != e; ++i) {
    printf("%16s: ", names_[i].c_str());
    ASTPrinter::print(values_[i].get());
  }
}

int ParamList::find(const std::string& name) const {
  for (int i = 0, e = names_.size(); i != e; ++i) {
    if (names_[i] == name) return i;
  }
  return -1;
}

SourceLocation ParamList::location() const {
  if (values_.empty()) return SourceLocation();

  return SourceLocation(front()->location().filename, front()->location().begin,
                      back()->location().end);
}
// }}}

std::unique_ptr<Expr> Expr::createDefaultInitializer(LiteralType type) {
  switch (type) {
    case LiteralType::Boolean:
      return std::make_unique<BoolExpr>(false);
    case LiteralType::Number:
      return std::make_unique<NumberExpr>(0);
    case LiteralType::String:
      return std::make_unique<StringExpr>("");
    case LiteralType::IPAddress:
      return std::make_unique<IPAddressExpr>();
    case LiteralType::Cidr:
    case LiteralType::RegExp:
    case LiteralType::Handler:
    case LiteralType::IntArray:
    case LiteralType::StringArray:
    case LiteralType::IPAddrArray:
    case LiteralType::CidrArray:
    default:
      // TODO not implemented
      return nullptr;
  }
}

void VariableSym::visit(ASTVisitor& v) { v.accept(*this); }

HandlerSym* UnitSym::findHandler(const std::string& name) {
  if (class HandlerSym* handler =
          dynamic_cast<class HandlerSym*>(scope()->lookup(name, Lookup::Self)))
    return handler;

  return nullptr;
}

void UnitSym::visit(ASTVisitor& v) { v.accept(*this); }

void HandlerSym::visit(ASTVisitor& v) { v.accept(*this); }

void UnaryExpr::visit(ASTVisitor& v) { v.accept(*this); }

BinaryExpr::BinaryExpr(Opcode op, std::unique_ptr<Expr>&& lhs,
                       std::unique_ptr<Expr>&& rhs)
    : Expr(rhs->location() - lhs->location()),
      operator_(op),
      lhs_(std::move(lhs)),
      rhs_(std::move(rhs)) {}

ArrayExpr::ArrayExpr(SourceLocation& loc,
                     std::vector<std::unique_ptr<Expr>>&& values)
    : Expr(loc), values_(std::move(values)) {}

ArrayExpr::~ArrayExpr() {}

void BinaryExpr::visit(ASTVisitor& v) { v.accept(*this); }

void ArrayExpr::visit(ASTVisitor& v) { v.accept(*this); }

void HandlerRefExpr::visit(ASTVisitor& v) { v.accept(*this); }

void CompoundStmt::push_back(std::unique_ptr<Stmt>&& stmt) {
  location_.update(stmt->location().end);
  statements_.push_back(std::move(stmt));
}

void CompoundStmt::visit(ASTVisitor& v) { v.accept(*this); }

void ExprStmt::visit(ASTVisitor& v) { v.accept(*this); }

void CallExpr::visit(ASTVisitor& v) { v.accept(*this); }

void BuiltinFunctionSym::visit(ASTVisitor& v) { v.accept(*this); }

void VariableExpr::visit(ASTVisitor& v) { v.accept(*this); }

void CondStmt::visit(ASTVisitor& v) { v.accept(*this); }

void AssignStmt::visit(ASTVisitor& v) { v.accept(*this); }

void BuiltinHandlerSym::visit(ASTVisitor& v) { v.accept(*this); }

void HandlerSym::implement(std::unique_ptr<SymbolTable>&& table,
                           std::unique_ptr<Stmt>&& body) {
  scope_ = std::move(table);
  body_ = std::move(body);
}

bool CallExpr::setArgs(ParamList&& args) {
  args_ = std::move(args);
  if (!args_.empty()) {
    location().update(args_.back()->location().end);
  }

  return true;
}

MatchStmt::MatchStmt(const SourceLocation& loc, std::unique_ptr<Expr>&& cond,
                     MatchClass op, std::list<Case>&& cases,
                     std::unique_ptr<Stmt>&& elseStmt)
    : Stmt(loc),
      cond_(std::move(cond)),
      op_(op),
      cases_(std::move(cases)),
      elseStmt_(std::move(elseStmt)) {}

MatchStmt::MatchStmt(MatchStmt&& other)
    : Stmt(other.location()),
      cond_(std::move(other.cond_)),
      op_(std::move(other.op_)),
      cases_(std::move(other.cases_)) {}

MatchStmt& MatchStmt::operator=(MatchStmt&& other) {
  setLocation(other.location());
  cond_ = std::move(other.cond_);
  op_ = std::move(other.op_);
  cases_ = std::move(other.cases_);

  return *this;
}

MatchStmt::~MatchStmt() {}

void MatchStmt::visit(ASTVisitor& v) { v.accept(*this); }

// {{{ type system
LiteralType UnaryExpr::getType() const { return resultType(op()); }

LiteralType BinaryExpr::getType() const { return resultType(op()); }

LiteralType ArrayExpr::getType() const {
  switch (values_.front()->getType()) {
    case LiteralType::Number:
      return LiteralType::IntArray;
    case LiteralType::String:
      return LiteralType::StringArray;
    case LiteralType::IPAddress:
      return LiteralType::IPAddrArray;
    case LiteralType::Cidr:
      return LiteralType::CidrArray;
    default:
      abort();
      return LiteralType::Void;  // XXX error
  }
}

template <>
LiteralType LiteralExpr<util::RegExp>::getType() const {
  return LiteralType::RegExp;
}

template <>
LiteralType LiteralExpr<Cidr>::getType() const {
  return LiteralType::Cidr;
}

template <>
LiteralType LiteralExpr<bool>::getType() const {
  return LiteralType::Boolean;
}

template <>
LiteralType LiteralExpr<IPAddress>::getType() const {
  return LiteralType::IPAddress;
}

template <>
LiteralType LiteralExpr<long long>::getType() const {
  return LiteralType::Number;
}

template <>
LiteralType LiteralExpr<std::string>::getType() const {
  return LiteralType::String;
}

LiteralType CallExpr::getType() const { return callee_->signature().returnType(); }

LiteralType VariableExpr::getType() const {
  return variable_->initializer()->getType();
}

LiteralType HandlerRefExpr::getType() const { return LiteralType::Handler; }
// }}}

}  // namespace xzero::flow
