#ifndef Athena_Resolve_typecheck_h
#define Athena_Resolve_typecheck_h

#include "resolve_ast.h"
#include "../General/compiler.h"

namespace athena {
namespace resolve {

struct TypeCheck {
	bool compatible(ExprRef src, ExprRef dst) {
		return compatible(src.type, dst.type);
	}

	bool compatible(ExprRef src, Type* dst) {
		if(compatible(src.type, dst)) {
			return true;
		} else if(auto l = findLiteral(src)) {
			// Literals have special conversion rules.
			return literalCoerce(l->literal, dst, Nothing());
		} else {
			return false;
		}
	}

	bool compatible(Type* src, Type* dst) {
		return src == dst || implicitCoerce(src, dst, Nothing());
	}

	/// Returns true if the source type can be implicitly converted to the target type.
	/// @param diag The diagnostics engine to use if an error should be produced.
	bool implicitCoerce(Type* source, Type* target, Maybe<Diagnostics*> diag);

	/// Checks if the source literal can be implicitly converted to the target type.
	/// @param diag The diagnostics engine to use if an error should be produced.
	/// @return True if the literal can be converted.
	bool literalCoerce(const ast::Literal& lit, Type* dst, Literal& target, Maybe<Diagnostics*> diag);
	bool literalCoerce(const Literal& lit, Type* dst, Maybe<Diagnostics*> diag);
};

}} // namespace athena::resolve

#endif // Athena_Resolve_typecheck_h