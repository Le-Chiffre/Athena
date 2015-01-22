#ifndef Athena_Parser_resolve_h
#define Athena_Parser_resolve_h

#include "parser.h"
#include "resolve_ast.h"

namespace athena {
namespace resolve {

struct TypeCheck {
	bool compatible(resolve::ExprRef a, resolve::ExprRef b) {
		return &a.type == &b.type;
	}
};

struct Resolver {
	Resolver(ast::CompileContext& context, ast::Module& source) : context(context), source(source), buffer(4*1024*1024) {}

	Module* resolve();
	bool resolveFunction(Function& fun, ast::FunDecl& decl);
	Expr* resolveExpression(Scope& scope, ast::ExprRef expr);
	Expr* resolveInfix(Scope& scope, const ast::InfixExpr& expr);
	Expr* resolvePrefix(Scope& scope, const ast::PrefixExpr& expr);
	Expr* resolveCall(Scope& scope, const ast::AppExpr& expr);
    Expr* resolveVar(Scope& scope, Id var);
    Expr* resolveIf(Scope& scope, const ast::IfExpr& expr);
	Expr* resolveDecl(Scope& scope, const ast::DeclExpr& expr);
	Expr* resolveAssign(Scope& scope, const ast::AssignExpr& expr);
	Expr* resolveWhile(Scope& scope, const ast::WhileExpr& expr);
	Expr* resolvePrimitiveOp(Scope& scope, PrimitiveOp op, const ast::InfixExpr& expr);
	Expr* resolvePrimitiveOp(Scope& scope, PrimitiveOp op, const ast::PrefixExpr& expr);
	Variable* resolveArgument(ScopeRef scope, Id arg);

	nullptr_t error(const char*);

	template<class P, class...Ps>
	nullptr_t error(const char*, P, Ps...);

	template<class T, class... P>
	T* build(P&&... p) {
		return buffer.New<T>(Core::Forward<P>(p)...);
	}

	Id primitiveOps[(uint)PrimitiveOp::OpCount];
	ast::CompileContext& context;
	ast::Module& source;
	Core::StaticBuffer buffer;
	TypeCheck typeCheck;
};

}} // namespace athena::resolve

#endif // Athena_Parser_resolve_h