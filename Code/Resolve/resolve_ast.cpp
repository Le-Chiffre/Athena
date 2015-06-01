#include "resolve_ast.h"
#include "resolve.h"

namespace athena {
namespace resolve {

Variable* Scope::findVar(Id name) {
    // Recursively search upwards through each scope.
    // TODO: Make this faster.
    auto scope = this;
    while(scope) {
        for(auto i : scope->shadows) {
            if(i->name == name) return i;
        }

        for(auto i : scope->variables) {
            if(i->name == name) return i;
        }
        scope = scope->parent;
    }

    // Not found in any scope.
    return nullptr;
}

Variable* Scope::findLocalVar(Id name) {
    for(auto i : shadows) {
        if(i->name == name) return i;
    }

    for(auto i : variables) {
        if(i->name == name) return i;
    }
    return nullptr;
}

template<class T>
inline T* findHelper(Scope* scope, Core::NumberMap<T, Id> Scope::*map, Id name) {
    // Type names are unique, although a generic type may have specializations.
    // Generic types are handled separately.
    while(scope) {
        // Even if the type name exists, it may not have been resolved yet.
        // This is handled by the caller.
        if(auto t = (scope->*map).Get(name)) return t;
        scope = scope->parent;
    }

    return nullptr;
}

TypeRef Scope::findType(Id name) { auto t = findHelper(this, &Scope::types, name); return t ? *t : nullptr; }
VarConstructor* Scope::findConstructor(Id name) { return findHelper(this, &Scope::constructors, name); }

bool Scope::hasVariables() {
    for(auto v : shadows) {
        if(v->isVar()) return true;
    }

    for(auto v : variables) {
        if(v->isVar()) return true;
    }

    return false;
}

bool succeedsAlways(const IfConds& conds, CondMode mode) {
    if(conds.Count()) {
        if(mode == resolve::CondMode::And) {
            // In and-mode, the chain can fail if there is at least one condition.
            for(auto e : conds) {
                if(e.cond) {
                    return false;
                }
            }
            return true;
        } else {
            // In or-mode, the chain will succeed if there is at least one missing condition.
            for(auto e : conds) {
                if(!e.cond) return true;
            }
            return false;
        }
    } else return true;
}

IfExpr::IfExpr(IfConds&& conds, ExprRef then, const Expr* otherwise, TypeRef type, bool ret, CondMode mode) :
        Expr(If, type), conds(conds), then(then), otherwise(otherwise), mode(mode), returnResult(ret) {
    alwaysTrue = succeedsAlways(conds, mode);
}

IfExpr::IfExpr(IfConds&& conds, ExprRef then, const Expr* otherwise, TypeRef type, bool ret, CondMode mode, bool alwaysTrue) :
        Expr(If, type), conds(conds), then(then), otherwise(otherwise), mode(mode), returnResult(ret), alwaysTrue(alwaysTrue) {
}

}} // namespace athena::resolve
