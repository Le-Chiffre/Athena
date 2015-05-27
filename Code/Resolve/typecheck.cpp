
#include "typecheck.h"

namespace athena {
namespace resolve {

inline void error(Maybe<Diagnostics*> diag, const char* text) {
	if(diag) {
		diag()->report(SourceLocation{0}, 0);
	}
}

bool TypeCheck::implicitCoerce(TypeRef src, TypeRef dst, Maybe<Diagnostics*> diag) {
	// Lvalue types are semantically equivalent and can always be implicitly converted.
	if(src->isLvalue()) {
		return compatible(((LVType*)src)->type, dst);
	}

	// Only primitive types can be implicitly converted:
	//  - floating point types can be converted to a larger type.
	//  - integer types can be converted to a larger type.
	//  - pointer types can be converted to Bool.
	//  - Bool can be converted to an integer type.
	// Special case: literals can be converted into any type of the same category.
	if(src->isPrimitive() && dst->isPrimitive()) {
		auto s = ((PrimType*)src)->type;
		auto d = ((PrimType*)dst)->type;
		if(category(s) == category(d) && d <= s) {
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
	} else if(src->isTuple() && dst->isTuple()) {
		// Tuples can be implicitly converted to more defined tuples.
		auto s = (TupleType*)src;
		auto d = (TupleType*)dst;
		if(s->fields.Count() != d->fields.Count()) return false;

		for(uint i=0; i<s->fields.Count(); i++) {
			if(s->fields[i].name && s->fields[i].name != d->fields[i].name) return false;
			if(!compatible(s->fields[i].type, d->fields[i].type)) return false;
		}

		return true;
	} else {
		error(diag, "only primitive types or pointers can be implicitly converted");
	}

	return false;
}

bool TypeCheck::literalCoerce(const ast::Literal& lit, TypeRef dst, Literal& literal, Maybe<Diagnostics*> diag) {
	// Literal conversion rules:
	//  - Integer and Float literals can be converted to any other integer or float
	//    (warnings should be issued if the value would not fit).
	literal = lit;
	if(dst->isPrimitive()) {
		auto ptype = ((const PrimType*)dst)->type;
		if(lit.type == ast::Literal::Int && ptype < PrimitiveType::FirstOther) {
			if(ptype < PrimitiveType::FirstFloat) {
				// No need to change the literal.
			} else if(ptype < PrimitiveType::FirstOther) {
				literal.f = lit.i;
				literal.type = ast::Literal::Float;
			}
		} else if(lit.type == ast::Literal::Float && ptype < PrimitiveType::FirstOther) {
			if(ptype < PrimitiveType::FirstFloat) {
				literal.i = lit.f;
				literal.type = ast::Literal::Int;
			} else if(ptype < PrimitiveType::FirstOther) {
				// No need to change the literal.
			}
		} else {
			error(diag, "cannot convert this literal to the target type");
			return false;
		}
	} else {
		error(diag, "literals can only be converted to primitive types");
		return false;
	}

	return true;
}

bool TypeCheck::literalCoerce(const Literal& lit, TypeRef dst, Maybe<Diagnostics*> diag) {
	// Literal conversion rules:
	//  - Integer and Float literals can be converted to any other integer or float
	//    (warnings should be issued if the value would not fit).
	if(dst->isPrimitive()) {
		auto ptype = ((const PrimType*)dst)->type;
		if(lit.type == ast::Literal::Int && ptype < PrimitiveType::FirstOther) {
			return true;
		} else if(lit.type == ast::Literal::Float && ptype < PrimitiveType::FirstOther) {
			return true;
		} else {
			error(diag, "cannot convert this literal to the target type");
			return false;
		}
	} else {
		error(diag, "literals can only be converted to primitive types");
		return false;
	}
}

}} // namespace athena::resolve
