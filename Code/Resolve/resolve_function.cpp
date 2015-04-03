
#include "resolve.h"

namespace athena {
namespace resolve {

bool Resolver::resolveFunction(Scope& scope, Function& fun) {
    if(!fun.astDecl) return true;

    auto& decl = *fun.astDecl;
    ASSERT(fun.name == decl.name);

    fun.scope.parent = &scope;
    if(decl.args) {
        auto arg = decl.args->fields;
        while (arg) {
            auto a = resolveArgument(fun.scope, arg->item);
            fun.arguments += a;
            arg = arg->next;
        }
    }
	
	// When the function parameters have been resolved, it is finished enough to be called.
	// This must be done before resolving the expression to support recursive functions.
	fun.astDecl = nullptr;
	fun.name = mangler.mangleId(&fun);
	
	// For multi-expressions, we return the last expression in the list.
	if(decl.body->type == ast::Expr::Multi) {
		fun.expression = resolveMultiWithRet(fun.scope, *(ast::MultiExpr*)decl.body);
	} else {
		fun.expression = build<RetExpr>(*resolveExpression(fun.scope, decl.body));
	}

    return true;
}

Variable* Resolver::resolveArgument(ScopeRef scope, ast::TupleField& arg) {
    auto type = arg.type ? resolveType(scope, arg.type) : types.getUnknown();
    auto var = build<Variable>(arg.name ? arg.name() : 0, type, scope, true, true);
    scope.variables += var;
    return var;
}

}} // namespace athena::resolve
