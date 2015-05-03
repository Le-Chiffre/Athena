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

}} // namespace athena::resolve
