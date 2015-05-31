
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

    auto arg = fun.astType->types;
    while(1) {
        if(arg->next) {
            auto a = resolveArgument(scope, arg->item);
            fun.arguments += a;
            arg = arg->next;
        } else {
            fun.type = resolveType(scope, arg->item);
            break;
        }
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

    auto expr = resolveExpression(fun.scope, decl.body);
    if(fun.type) {
        expr = implicitCoerce(*expr, fun.type);
    }
	
	// When the function parameters have been resolved, it is finished enough to be called.
	// This must be done before resolving the expression to support recursive functions.
	fun.astDecl = nullptr;
	fun.name = mangler.mangleId(&fun);
    fun.expression = createRet(*expr);

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

Variable* Resolver::resolveArgument(ScopeRef scope, ast::Type* arg) {
    auto type = resolveType(scope, arg);
    auto var = build<Variable>(0, type, scope, true, true);
    scope.variables += var;
    return var;
}

}} // namespace athena::resolve
