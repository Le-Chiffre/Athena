
#include "resolve.h"

namespace athena {
namespace resolve {

bool Resolver::resolveFunctionDecl(Scope& scope, FunctionDecl& fun) {
    if(fun.hasImpl) return resolveFunction(scope, (Function&)fun);
    if(fun.isForeign) return resolveForeignFunction(scope, (ForeignFunction&)fun);

    return false;
}

bool Resolver::resolveForeignFunction(Scope& scope, ForeignFunction& fun) {
    if(!fun.astType) return true;

    fun.type = resolveType(scope, fun.astType->returnType);
    auto arg = fun.astType->params;
    while(arg) {
        auto a = resolveArgument(scope, arg->item);
        fun.arguments += a;
        arg = arg->next;
    }

    fun.astType = nullptr;
    return true;
}

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

    if(decl.ret) {
        fun.type = resolveType(scope, decl.ret);
    }
	
	// When the function parameters have been resolved, it is finished enough to be called.
	// This must be done before resolving the expression to support recursive functions.
	fun.astDecl = nullptr;
	fun.name = mangler.mangleId(&fun);
	
	// For multi-expressions, we return the last expression in the list.
	if(decl.body->type == ast::Expr::Multi) {
		fun.expression = resolveMultiWithRet(fun.scope, *(ast::MultiExpr*)decl.body);
	} else {
		fun.expression = createRet(*resolveExpression(fun.scope, decl.body));
	}

    // If no type was defined or inferred before, we simply take the type of the last expression.
    if(!fun.type) {
        fun.type = fun.expression->type;
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
