
#include "resolve.h"

namespace athena {
namespace resolve {

Function* Resolver::findFunction(ScopeRef scope, ast::ExprRef callee, ExprList* args) {
	if(callee->isVar()) {
		auto name = ((const ast::VarExpr*)callee)->name;
		if(auto fun = findFunction(scope, name, args)) {
			// TODO: Create closure type if the function takes more parameters.
			return fun;
		} else {
			error("no function named '%@' found", context.Find(name).name);
		}
	} else {
		error("not a callable type");
	}

	return nullptr;
}

Function* Resolver::findFunction(ScopeRef scope, Id name, ExprList* args) {
	potentialCallees.Clear();
	bool identifierExists = false;

	// Recursively search upwards through each scope.
	auto s = &scope;
	while(s) {
		// Note: functions are added to a scope before they are processed, so any existing function will be found from here.
		// TODO: Since only the function names are added (not their arguments),
		// TODO: we have to resolve each function before we can know which one to call.
		if(auto fns = s->functions.Get(name)) {
			identifierExists = true;

			// If there are multiple overloads, we have to resolve each one.
			auto fn = *fns;
			while(fn) {
				resolveFunction(*s, *fn);
				if(potentiallyCallable(fn, args))
					potentialCallees += fn;
				fn = fn->sibling;
			}
		}

		s = s->parent;
	}

	// No callable function was found.
	// TODO: Should we return some dummy object here?
	if(!potentialCallees.Count()) {
		auto n = context.Find(name).name;
		if(identifierExists)
			error("no matching function for call to '%@'", n);
		else
			error("use of undeclared identifier '%@'", n);

		return nullptr;
	}

	// Find the best match and return it.
	return findBestMatch(args);
}

bool Resolver::potentiallyCallable(Function* fun, ExprList* args) {
	// For now, check if each argument is compatible.
	auto farg = fun->arguments.begin();
	auto fend = fun->arguments.end();
	auto arg = args;
	while(arg && farg != fend) {
		// If any argument is incompatible, the function is not callable.
		if(!typeCheck.compatible(*arg->item, (*farg)->type)) return false;

		arg = arg->next;
		farg = ++farg;
	}

	// If either iterator has elements left, the argument counts do not match.
	return !(arg || farg != fend);
}

uint Resolver::findImplicitConversionCount(Function* f, ExprList* args) {
	return 0;
}

Function* Resolver::findBestMatch(ExprList* args) {
	// One function is a better match than the other if one of the following is true:
	//  - The call needs less implicit conversions.
	//  - The function is less generic.
	ASSERT(potentialCallees.Count() > 0);
	if(potentialCallees.Count() == 1) return potentialCallees[0];

	Function* bestMatch = potentialCallees[0];
	uint leastConversions = findImplicitConversionCount(bestMatch, args);
	uint sameMatchCount = 0; // The number of functions that match just as well as the best one.

	// For each function, check if it is better than the best one.
	for(uint i = 1; i < potentialCallees.Count(); i++) {
		// TODO: This should be the last match factor that is checked.
		uint convs = findImplicitConversionCount(potentialCallees[i], args);
		if(convs < leastConversions) {
			sameMatchCount = 0;
			bestMatch = potentialCallees[i];
			leastConversions = convs;
		} else if(convs == leastConversions) {
			sameMatchCount++;
		}
	}

	// If multiple functions are the best, the call is ambiguous.
	if(sameMatchCount) error("Call to '%@' is ambiguous", context.Find(potentialCallees[0]->name).name);

	return bestMatch;
}

}} // namespace athena::resolve