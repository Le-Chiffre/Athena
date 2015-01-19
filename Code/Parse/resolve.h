#ifndef Athena_Parser_resolve_h
#define Athena_Parser_resolve_h

#include "parser.h"
#include "resolve_ast.h"

namespace athena {
namespace resolve {

struct Resolver {
	Resolver(ast::CompileContext& context, ast::Module& source) : context(context), source(source), buffer(4*1024*1024) {}

	Module* resolve();
	Function* resolveFunction(ast::FunDecl& decl);
	Expr* resolveExpression(ast::ExprRef expr);
	Variable* resolveArgument(ScopeRef scope, Id arg);

	template<class T, class... P>
	T* build(P&&... p) {
		return buffer.New<T>(Core::Forward<P>(p)...);
	}

	ast::CompileContext& context;
	ast::Module& source;
	Core::StaticBuffer buffer;
};

}} // namespace athena::resolve

#endif // Athena_Parser_resolve_h