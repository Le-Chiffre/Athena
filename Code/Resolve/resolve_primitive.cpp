
#include "resolve.h"

namespace athena {
namespace resolve {

static const char* primitiveOperatorNames[] = {
	"+", "-", "*", "/", "mod",        // Arithmetic
	"shl", "shr", "and", "or", "xor", // Bitwise
	"==", "!=", ">", ">=", "<", "<=", // Comparison
	"-", "not", "&", "*"			  // Unary
};

static const Byte primitiveOperatorLengths[] = {
	1, 1, 1, 1, 3,    // Arithmatic
	3, 3, 3, 2, 3,	  // Bitwise
	2, 2, 1, 2, 1, 2, // Comparison
	1, 3, 1, 1		  // Unary
};

static const U16 primitiveOperatorPrecedences[] = {
	11, 11, 12, 12, 12,	// Arithmetic
	10, 10, 7, 5, 6,	// Bitwise
	8, 8, 9, 9, 9, 9	// Comparison
};

static const char* primitiveTypeNames[] = {
	"I64", "I32", "I16", "I8",
	"U64", "U32", "U16", "U8",
	"F64", "F32", "F16", "Bool"
};

static const Byte primitiveTypeLengths[] = {
	3, 3, 3, 2,
	3, 3, 3, 2,
	3, 3, 3, 4
};

/// Checks if the two provided types can be compared.
inline bool cmpCompatible(PrimitiveType lhs, PrimitiveType rhs) {
	// Types in the Other category can only be compared with themselves.
	return category(lhs) == PrimitiveTypeCategory::Other
			? lhs == rhs
			: category(lhs) == category(rhs);
}

/// Checks if bitwise operations can be performed on the provided types.
inline bool bitCompatible(PrimitiveType lhs, PrimitiveType rhs) {
	// Bitwise operations can only be applied to integers from the same category, or booleans.
	auto cl = category(lhs);
	auto cr = category(rhs);
	return (cl == cr
			&& cl <= PrimitiveTypeCategory::Unsigned)
			|| (lhs == PrimitiveType::Bool && rhs == PrimitiveType::Bool);
}

/// Checks if arithmatic operations can be performed on the provided types.
inline bool arithCompatible(PrimitiveType lhs, PrimitiveType rhs) {
	// Currently the same as comparison.
	return cmpCompatible(lhs, rhs);
}

void Resolver::initPrimitives() {
	// Make sure each operator exists in the context and add them to the map.
	for(Size i = 0; i < (Size)PrimitiveOp::FirstUnary; i++) {
		primitiveOps[i] = context.addUnqualifiedName(primitiveOperatorNames[i], primitiveOperatorLengths[i]);
		primitiveBinaryMap.add(primitiveOps[i], (PrimitiveOp)i);
	}

	for(auto i = (Size)PrimitiveOp::FirstUnary; i < (Size)PrimitiveOp::OpCount; i++) {
		primitiveOps[i] = context.addUnqualifiedName(primitiveOperatorNames[i], primitiveOperatorLengths[i]);
		primitiveUnaryMap.add(primitiveOps[i], (PrimitiveOp)i);
	}

	// Make sure all precedences are in the context and none have been overwritten.
	for(Size i = 0; i < (Size)PrimitiveOp::FirstUnary; i++) {
		if(context.tryFindOp(primitiveOps[i]))
			error("the precedence of built-in operator %@ cannot be redefined", primitiveOperatorNames[i]);
		context.addOp(primitiveOps[i], primitiveOperatorPrecedences[i], ast::Assoc::Left);
	}

	// Make sure each primitive type exists in the context, and add them to the map.
	for(Size i = 0; i < (Size)PrimitiveType::TypeCount; i++) {
		auto id = context.addUnqualifiedName(primitiveTypeNames[i], primitiveTypeLengths[i]);
		types.primMap.add(id, types.getPrim((PrimitiveType)i));
	}

	// Add the builtin aliases.
	types.primMap.add(context.addUnqualifiedName("Byte"), types.getPrim(PrimitiveType::U8));
	types.primMap.add(context.addUnqualifiedName("Int"), types.getPrim(PrimitiveType::I32));
	types.primMap.add(context.addUnqualifiedName("Float"), types.getPrim(PrimitiveType::F32));
	types.primMap.add(context.addUnqualifiedName("Double"), types.getPrim(PrimitiveType::F64));
}

Expr* Resolver::resolvePrimitiveOp(Scope& scope, PrimitiveOp op, ExprRef lhs, ExprRef rhs) {
	// This is either a pointer or primitive.
	if(lhs.type->isPointer()) {
		auto lt = (PtrType*)lhs.type;
		if(rhs.type->isPointer()) {
			auto rt = (PtrType*)rhs.type;
			if(auto type = getPtrOpType(op, lt, rt)) {
				auto list = build<ExprList>(&lhs, build<ExprList>(&rhs));
				return build<AppPExpr>(op, list, type);
			}
		} else if(rhs.type->isPrimitive()) {
			auto rt = ((PrimType*)rhs.type)->type;
			if(auto type = getPtrOpType(op, lt, rt)) {
				auto list = build<ExprList>(&lhs, build<ExprList>(&rhs));
				return build<AppPExpr>(op, list, type);
			}
		}
	} else if(rhs.type->isPointer()) {
		error("This built-in operator cannot be applied to a primitive and a pointer");
	} else if(lhs.type->isPrimitive() && rhs.type->isPrimitive()) {
		auto left = &lhs;
		auto right = &rhs;

		// If one of the expressions is a literal it can be converted implicitly.
		if(lhs.kind == Expr::Lit) {
			left = literalCoerce(((LitExpr&)lhs).literal, rhs.type);
		} else if(rhs.kind == Expr::Lit) {
			right = literalCoerce(((LitExpr&)rhs).literal, lhs.type);
		}

		auto lt = ((const PrimType*)left->type)->type;
		auto rt = ((const PrimType*)right->type)->type;

		if(auto type = getBinaryOpType(op, lt, rt, left, right)) {
			auto list = build<ExprList>(left, build<ExprList>(right));
			return build<AppPExpr>(op, list, type);
		}
	}
	return nullptr;
}

Expr* Resolver::resolvePrimitiveOp(Scope& scope, PrimitiveOp op, resolve::ExprRef dst) {
	// The type is either a pointer or primitive.
	if(dst.type->isPointer()) {
		if(op == PrimitiveOp::Deref) {
			// First we convert the pointer itself to an rvalue, since it may be stored on the stack.
			// Then we create an lvalue for the memory the pointer points to, which can be read or assigned.
			return build<LoadExpr>(*getRV(dst), types.getLV(((PtrType*)dst.type)->type));
		} else {
			// Currently no unary operators are defined for pointers.
			error("this built-in operator cannot be applied to pointer types");
		}
	} else {
		auto type = ((const PrimType*)dst.type)->type;
		if(auto rtype = getUnaryOpType(op, type)) {
			auto list = build<ExprList>(&dst);
			return build<AppPExpr>(op, list, rtype);
		}
	}
	return nullptr;
}

Type* Resolver::getBinaryOpType(PrimitiveOp op, PrimitiveType lhs, PrimitiveType rhs, Expr*& left, Expr*& right) {
	auto type = types.getPrim(largest(lhs, rhs));
	left = implicitCoerce(*left, type);
	right = implicitCoerce(*right, type);

	if(op < PrimitiveOp::FirstBit) {
		// Arithmetic operators return the largest type.
		if(arithCompatible(lhs, rhs)) {
			return type;
		} else {
			error("arithmetic operator on incompatible primitive types");
		}
	} else if(op < PrimitiveOp::FirstCompare) {
		// Bitwise operators return the largest type.
		if(bitCompatible(lhs, rhs)) {
			return type;
		} else {
			error("bitwise operator on incompatible primitive types");
		}
	} else if(op < PrimitiveOp::FirstUnary) {
		// Comparison operators always return Bool.
		if(cmpCompatible(lhs, rhs)) {
			return types.getBool();
		} else {
			error("comparison between incompatible primitive types");
		}
	}

	return nullptr;
}

Type* Resolver::getPtrOpType(PrimitiveOp op, PtrType* ptr, PrimitiveType prim) {
	// Only addition and subtraction are defined.
	if(prim < PrimitiveType::FirstFloat) {
		if(op == PrimitiveOp::Add || op == PrimitiveOp::Sub) {
			return ptr;
		} else {
			error("unsupported operation on pointer and integer type");
		}
	} else {
		error("this primitive type cannot be applied to a pointer");
	}

	return nullptr;
}

Type* Resolver::getPtrOpType(PrimitiveOp op, PtrType* lhs, PtrType* rhs) {
	// Supported ptr-ptr operations: comparison and difference (for the same pointer types).
	if(lhs == rhs) {
		if(op >= PrimitiveOp::FirstCompare && op < PrimitiveOp::FirstUnary) {
			return types.getBool();
		} else if(op == PrimitiveOp::Sub) {
			// TODO: Return int_ptr instead.
			return types.getInt();
		} else {
			error("unsupported operation on two pointer types");
		}
	} else {
		error("cannot apply operator on differing pointer types");
	}

	return nullptr;
}

Type* Resolver::getUnaryOpType(PrimitiveOp op, PrimitiveType type) {
	if(op == PrimitiveOp::Neg) {
		// Returns the same type.
		if(category(type) <= PrimitiveTypeCategory::Float) {
			return types.getPrim(type);
		} else {
			error("cannot negate this primitive type");
		}
	} else if(op == PrimitiveOp::Not) {
		// Returns the same type.
		if(category(type) <= PrimitiveTypeCategory::Unsigned || type == PrimitiveType::Bool) {
			return types.getPrim(type);
		} else {
			error("the not-operation can only be applied to booleans and integers");
		}
	} else {
		assert("Not a unary operator or unsupported!" == 0);
	}
	return nullptr;
}

}} // namespace athena::resolve
