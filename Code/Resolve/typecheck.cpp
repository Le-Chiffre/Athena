
#include "typecheck.h"

namespace athena {
namespace resolve {

inline void error(Maybe<Diagnostics*> diag, const char* text) {
	if(diag) {
		diag()->report(SourceLocation{0}, 0);
	}
}

bool TypeCheck::implicitCoerce(TypeRef src, TypeRef dst, Maybe<Diagnostics*> diag) {
	// Only primitive types can be implicitly converted:
	//  - floating point types can be converted to a larger type.
	//  - integer types can be converted to a larger type.
	//  - pointer types can be converted to Bool.
	//  - Bool can be converted to an integer type.
	// Special case: literals can be converted into any type of the same category.
	if(src->isPrimitive() && dst->isPrimitive()) {
		auto s = ((PrimType*)src->kind)->type;
		auto d = ((PrimType*)dst)->type;
		if(category(s) == category(d) && (src->kind == Expr::Lit || d >= s)) {
			return true;
		} else {
			error(diag, "a primitive type can only be implicitly converted to a larger type");
		}
	} else if(src->isPointer()) {
		if(dst->isBool()) {
			return true;
		} else {
			error(diag, "pointer types can only be implicitly converted to Bool");
		}
	} else if(src->isBool() && dst->isPrimitive()) {
		if(((PrimType*)dst)->type < PrimitiveType::FirstFloat) {
			return true;
		} else {
			error(diag, "booleans can only be implicitly converted to an integer types");
		}
	} else {
		error(diag, "only primitive types or pointers can be implicitly converted");
	}

	return false;
}

}} // namespace athena::resolve
