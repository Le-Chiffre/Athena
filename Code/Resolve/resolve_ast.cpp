#include "resolve_ast.h"
#include "resolve.h"

namespace athena {
namespace resolve {

Variable* Scope::findVar(Id name) {
    // Recursively search upwards through each scope.
    // TODO: Make this faster.
    auto scope = this;
    while(scope) {
        for(auto i : variables) {
            if(i->name == name) return i;
        }
        scope = scope->parent;
    }

    // Not found in any scope.
    return nullptr;
}

Variable* Scope::findLocalVar(Id name) {
    for(auto i : variables) {
        if(i->name == name) return i;
    }
    return nullptr;
}

Type* Scope::findType(Id name) {
    // Type names are unique, although a generic type may have specializations.
    // Generic types are handled separately.
    auto scope = this;
    while(scope) {
        // Even if the type name exists, it may not have been resolved yet.
        // This is handled by the caller.
        if(auto t = types.Get(name)) return *t;
        scope = scope->parent;
    }

    return nullptr;
}

}} // namespace athena::resolve
