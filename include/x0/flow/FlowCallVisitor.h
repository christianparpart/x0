/* <flow/FlowCallVisitor.h>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://redmine.xzero.io/projects/flow
 *
 * (c) 2009-2013 Christian Parpart <trapni@gmail.com>
 */

#ifndef sw_flow_Flow_h
#define sw_flow_Flow_h

#include <x0/flow/ASTVisitor.h>
#include <x0/Api.h>
#include <vector>

namespace x0 {

class ASTNode;

class X0_API FlowCallVisitor :
    public ASTVisitor
{
private:
    std::vector<FunctionCall*> functionCalls_;
    std::vector<HandlerCall*> handlerCalls_;

public:
    explicit FlowCallVisitor(ASTNode* root = nullptr);
    ~FlowCallVisitor();

    void visit(ASTNode* root);

    void clear() { functionCalls_.clear(); handlerCalls_.clear(); }

    const std::vector<FunctionCall*>& functionCalls() const { return functionCalls_; }
    const std::vector<HandlerCall*>& handlerCalls() const { return handlerCalls_; }

protected:
    // symbols
    virtual void accept(Unit& symbol);
    virtual void accept(Variable& variable);
    virtual void accept(Handler& handler);
    virtual void accept(BuiltinFunction& symbol);
    virtual void accept(BuiltinHandler& symbol);

    // expressions
    virtual void accept(UnaryExpr& expr);
    virtual void accept(BinaryExpr& expr);
    virtual void accept(FunctionCall& expr);
    virtual void accept(VariableExpr& expr);
    virtual void accept(HandlerRefExpr& expr);

    virtual void accept(StringExpr& expr);
    virtual void accept(NumberExpr& expr);
    virtual void accept(BoolExpr& expr);
    virtual void accept(RegExpExpr& expr);
    virtual void accept(IPAddressExpr& expr);
    virtual void accept(CidrExpr& cidr);

    // statements
    virtual void accept(ExprStmt& stmt);
    virtual void accept(CompoundStmt& stmt);
    virtual void accept(CondStmt& stmt);
    virtual void accept(AssignStmt& stmt);
    virtual void accept(HandlerCall& stmt);
};

} // namespace x0

#endif
