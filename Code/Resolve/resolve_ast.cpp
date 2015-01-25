
#include "resolve_ast.h"

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

Function* Scope::findFun(Id name) {
    // Recursively search upwards through each scope.
    auto scope = this;
    while(scope) {
        // Note: functions are added to a scope before they are processed,
        // so any existing function will be found from here.
        for(auto i : functions) {
            if(i->name == name) return i;
        }
        scope = scope->parent;
    }

    // No function was found.
    return nullptr;
}

}} // namespace athena::resolve
