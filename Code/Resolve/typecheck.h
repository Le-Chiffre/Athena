#ifndef Athena_Resolve_typecheck_h
#define Athena_Resolve_typecheck_h

#include "resolve_ast.h"
#include "../General/diagnostic.h"

namespace athena {
namespace resolve {

using Core::Maybe;

struct TypeCheck {
	bool compatible(resolve::ExprRef a, resolve::ExprRef b) {
		return &a.type == &b.type;
	}

	/// Returns true if the source type can be implicitly converted to the target type.
	/// @param diag The diagnostics engine to use if an error should be produced.
	bool implicitCoerce(TypeRef source, TypeRef target, Maybe<Diagnostics*> diag);
};

}} // namespace athena::resolve

#endif // Athena_Resolve_typecheck_h