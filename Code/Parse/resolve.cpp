#include "resolve.h"

namespace athena {
namespace resolve {

Module* Resolver::resolve() {
	auto module = build<Module>();
	module->name = source.name;

	for(auto decl : source.declarations) {
		if(decl->type == ast::Decl::Function) {
			if(auto f = resolveFunction(*(ast::FunDecl*)decl)) {
				module->functions += f;
			}
		}
	}
}

Function* Resolver::resolveFunction(ast::FunDecl& decl) {
	auto f = build<Function>();
	f->name = decl.name;
	auto arg = decl.args;
	while(arg) {
		auto a = resolveArgument(*f, arg->item);
		f->arguments += a;
		f->variables += a;
		arg = arg->next;
	}
	f->expression = resolveExpression(decl.body);
}

Expr* Resolver::resolveExpression(ast::ExprRef expr) {

}

Variable* Resolver::resolveArgument(ScopeRef scope, Id arg) {
	return build<Variable>(arg, scope);
}

}} // namespace athena::resolve
