#ifndef Athena_Parser_resolve_h
#define Athena_Parser_resolve_h

#include "parser.h"
#include "resolve_ast.h"

namespace athena {
namespace resolve {

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
	Expr* resolvePrimitiveOp(Scope& scope, PrimitiveOp op, const ast::InfixExpr& expr);
	Expr* resolvePrimitiveOp(Scope& scope, PrimitiveOp op, const ast::PrefixExpr& expr);
	Variable* resolveArgument(ScopeRef scope, Id arg);

	template<class T, class... P>
	T* build(P&&... p) {
		return buffer.New<T>(Core::Forward<P>(p)...);
	}

	Id primitiveOps[(uint)PrimitiveOp::OpCount];
	ast::CompileContext& context;
	ast::Module& source;
	Core::StaticBuffer buffer;
};

}} // namespace athena::resolve

#endif // Athena_Parser_resolve_h