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

Function* Scope::findFun(Id name, ExprList* types) {
	// Create a list of all functions with this name.
	Core::Array<Function*> foundFunctions{16};

    // Recursively search upwards through each scope.
    auto scope = this;
    while(scope) {
        // Note: functions are added to a scope before they are processed, so any existing function will be found from here.
		// TODO: Since only the function names are added (not their arguments),
		// TODO: we have to resolve each function before we can know which one to call.
		if(auto fns = functions.Get(name)) {
			// If there are multiple overloads, we have to resolve each one.

		}

        scope = scope->parent;
    }

    // No function was found.
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
