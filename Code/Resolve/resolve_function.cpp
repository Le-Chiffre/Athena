
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
    fun.scope.function = &fun;
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

    // Resolve locally defined functions.
    auto local = decl.locals;
    while(local) {
        // Local functions cannot be overloaded.
        auto name = local->item->name;
        FunctionDecl** f;
        if(fun.scope.functions.AddGet(name, f)) {
            error("local functions cannot be overloaded");
        }
        *f = build<Function>(name, local->item);
        local = local->next;
    }

    fun.scope.functions.Iterate([this, &fun](Id name, FunctionDecl* f) {
        resolveFunctionDecl(fun.scope, *f);
    });

    // The function has either a normal body or a set of patterns.
    Expr* body;
    if(decl.body) {
        body = resolveExpression(fun.scope, decl.body, true);
    } else {
        ASSERT(decl.cases != nullptr);
        body = resolveFunctionCases(scope, fun, decl.cases);
    }

    if(fun.type) body = implicitCoerce(*body, fun.type);
    fun.expression = createRet(*body);

	// When the function parameters have been resolved, it is finished enough to be called.
	// This must be done before resolving the expression to support recursive functions.
	fun.astDecl = nullptr;
	fun.name = mangler.mangleId(&fun);

    // If no type was defined or inferred before, we simply take the type of the last expression.
    if(!fun.type) {
        fun.type = fun.expression->type;
    }

    return true;
}

Expr* Resolver::resolveFunctionCases(Scope& scope, Function& fun, ast::FunCaseList* cases) {
    if(cases) {
        IfConds conds;
        uint i = 0;
        auto pat = cases->item->patterns;
        auto s = build<ScopedExpr>(scope);
        while(pat) {
            if(fun.arguments.Count() <= i) {
                error("pattern count must match with the number of arguments");
                return nullptr;
            }
            Variable* arg = fun.arguments[i];
            auto pivot = build<VarExpr>(arg, arg->type);
            resolvePattern(s->scope, *pivot, *pat->item, conds);
            pat = pat->next;
            i++;
        }

        auto body = resolveExpression(s->scope, cases->item->body, true);
        s->contents = createIf(Move(conds), *body, resolveFunctionCases(scope, fun, cases->next), true, CondMode::And);
        s->type = body->type;
        return s;
    } else {
        return nullptr;
    }
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
