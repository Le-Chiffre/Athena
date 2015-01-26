#include "resolve.h"

namespace athena {
namespace resolve {

static const char* primitiveOperatorNames[] = {
	"+", "-", "*", "/", "mod",        // Arithmetic
	"shl", "shr", "and", "or", "xor", // Bitwise
	"==", "!=", ">", ">=", "<", "<=", // Comparison
	"-", "not" 						  // Unary
};

static const byte primitiveOperatorLengths[] = {
	1, 1, 1, 1, 3,    // Arithmatic
	3, 3, 3, 2, 3,	  // Bitwise
	2, 2, 1, 2, 1, 2, // Comparison
	1, 3			  // Unary
};

static const uint16 primitiveOperatorPrecedences[] = {
	11, 11, 12, 12, 12,	// Arithmetic
	10, 10, 7, 5, 6,	// Bitwise
	8, 8, 9, 9, 9, 9	// Comparison
};

static const char* primitiveTypeNames[] = {
	"I64", "I32", "I16", "I8",
	"U64", "U32", "U16", "U8",
	"F64", "F32", "F16", "Bool"
};

static const byte primitiveTypeLengths[] = {
	3, 3, 3, 2,
	3, 3, 3, 2,
	3, 3, 3, 4
};

inline TypeRef getLiteralType(TypeManager& m, const Literal& l) {
    switch(l.type) {
        case Literal::Int: return m.getInt();
        case Literal::Float: return m.getFloat();
        case Literal::Char: return m.getU8();
        case Literal::String: return m.getString();
		default: FatalError("Unknown literal type."); return nullptr;
    }
}

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

Resolver::Resolver(ast::CompileContext& context, ast::Module& source) :
	context(context), source(source), buffer(4*1024*1024) {}

void Resolver::initPrimitives() {
	// Make sure each operator exists in the context and add them to the map.
	for(uint i = 0; i < (uint)PrimitiveOp::OpCount; i++) {
		primitiveOps[i] = context.AddUnqualifiedName(primitiveOperatorNames[i], primitiveOperatorLengths[i]);
		primitiveMap.Add(primitiveOps[i], (PrimitiveOp)i);
	}

	// Make sure all precedences are in the context and none have been overwritten.
	for(uint i = 0; i < (uint)PrimitiveOp::FirstUnary; i++) {
		if(context.TryFindOp(primitiveOps[i]))
			error("the precedence of built-in operator %@ cannot be redefined", primitiveOperatorNames[i]);
		context.AddOp(primitiveOps[i], primitiveOperatorPrecedences[i], ast::Assoc::Left);
	}

	// Make sure each primitive type exists in the context, and add them to the map.
	for(uint i = 0; i < (uint)PrimitiveType::TypeCount; i++) {
		auto id = context.AddUnqualifiedName(primitiveTypeNames[i], primitiveOperatorLengths[i]);
		types.primMap.Add(id, types.getPrim((PrimitiveType)i));
	}
}

Module* Resolver::resolve() {
	initPrimitives();
	auto module = build<Module>();
	module->name = source.name;

    /*
     * We need to do two passes here.
     * In the first pass we add each declared identifier to the appropriate list in its scope.
     * This makes sure that every dependent identifier can be found in the second pass,
     * where we resolve the content of each declared identifier.
     */

    // Perform the declaration pass.
    for(auto decl : source.declarations) {
		switch(decl->kind) {
			case ast::Decl::Function:
				module->functions += build<Function>(((ast::FunDecl*)decl)->name);
				break;
			case ast::Decl::Type:
				break;
			case ast::Decl::Data:
				break;
		}
    }

    // Perform the resolve pass.
    uint f = 0;
	uint t = 0;
	uint d = 0;
	for(auto decl : source.declarations) {
		switch(decl->kind) {
			case ast::Decl::Function:
				resolveFunction(*module->functions[f], *(ast::FunDecl*)decl);
				f++;
				break;
			case ast::Decl::Type:
				t++;
				break;
			case ast::Decl::Data:
				d++;
				break;
		}
	}
	
	return module;
}

bool Resolver::resolveFunction(Function& fun, ast::FunDecl& decl) {
	ASSERT(fun.name == decl.name);
	auto arg = decl.args;
	while(arg) {
		auto a = resolveArgument(fun, arg->item);
		fun.arguments += a;
		fun.variables += a;
		arg = arg->next;
	}
	fun.expression = resolveExpression(fun, decl.body);
	return true;
}

Expr* Resolver::resolveExpression(Scope& scope, ast::ExprRef expr) {
	switch(expr->type) {
        case ast::Expr::Lit:
            return resolveLiteral(scope, *(ast::LitExpr*)expr);
		case ast::Expr::Infix:
			return resolveInfix(scope, *(ast::InfixExpr*)expr);
		case ast::Expr::Prefix:
			return resolvePrefix(scope, *(ast::PrefixExpr*)expr);
		case ast::Expr::App:
			return resolveCall(scope, *(ast::AppExpr*)expr);
        case ast::Expr::Var:
            // This can be either a variable or a function call without parameters.
            return resolveVar(scope, ((ast::VarExpr*)expr)->name);
        case ast::Expr::If:
            return resolveIf(scope, *(ast::IfExpr*)expr);
		case ast::Expr::Decl:
			return resolveDecl(scope, *(ast::DeclExpr*)expr);
		case ast::Expr::Assign:
			return resolveAssign(scope, *(ast::AssignExpr*)expr);
		case ast::Expr::While:
			return resolveWhile(scope, *(ast::WhileExpr*)expr);
		case ast::Expr::Nested:
			return resolveExpression(scope, ((ast::NestedExpr*)expr)->expr);
		case ast::Expr::Coerce:
			return resolveCoerce(scope, *(ast::CoerceExpr*)expr);
		case ast::Expr::Field:
			return resolveField(scope, *(ast::FieldExpr*)expr);
		case ast::Expr::Construct:
			return resolveConstruct(scope, *(ast::ConstructExpr*)expr);
		default:
			FatalError("Unsupported expression type.");
	}
	
	return nullptr;
}

Expr* Resolver::resolveLiteral(Scope& scope, ast::LitExpr& expr) {
    return build<LitExpr>(expr.literal, getLiteralType(types, expr.literal));
}

Expr* Resolver::resolveInfix(Scope& scope, ast::InfixExpr& expr) {
	auto& e = reorder(expr);
	ast::VarExpr var(e.op);
	return resolveBinaryCall(scope, &var, e.lhs, e.rhs);
}

Expr* Resolver::resolvePrefix(Scope& scope, ast::PrefixExpr& expr) {
	ast::VarExpr var(expr.op);
	return resolveUnaryCall(scope, &var, expr.dst);
}

Expr* Resolver::resolveBinaryCall(Scope& scope, ast::ExprRef function, ast::ExprRef lhs, ast::ExprRef rhs) {
	auto lt = resolveExpression(scope, lhs);
	auto rt = resolveExpression(scope, rhs);

	// Check if this can be a primitive operation.
	// Note that primitive operations can be both functions and operators.
	if((lt->type->isPrimitive() && rt->type->isPrimitive()) || (lt->type->isPointer() && rt->type->isPointer())) {
		if(auto op = tryPrimitiveOp(function)) {
			// This means that built-in binary operators cannot be overloaded for any pointer or primitive type.
			if(*op < PrimitiveOp::FirstUnary)
				return resolvePrimitiveOp(scope, *op, *lt, *rt);
		}
	}

	// Otherwise, create a normal function call.
	auto args = build<ExprList>(lt, build<ExprList>(rt));
	if(auto func = findFunction(scope, function, args)) {
		return build<AppExpr>(*func, args);
	} else {
		// No need for an error; this is done by findFunction.
		return nullptr;
	}
}

Expr* Resolver::resolveUnaryCall(Scope& scope, ast::ExprRef function, ast::ExprRef dst) {
	auto target = resolveExpression(scope, dst);

	// Check if this can be a primitive operation.
	// Note that primitive operations can be both functions and operators.
	if(target->type->isPtrOrPrim()) {
		if(auto op = tryPrimitiveOp(function)) {
			// This means that built-in unary operators cannot be overloaded for any pointer or primitive type.
			if(*op >= PrimitiveOp::FirstUnary)
				return resolvePrimitiveOp(scope, *op, *target);
		}
	}

	// Otherwise, create a normal function call.
	auto args = build<ExprList>(target);
	if(auto func = findFunction(scope, function, args)) {
		return build<AppExpr>(*func, args);
	} else {
		// No need for an error; this is done by findFunction.
		return nullptr;
	}
}
	
Expr* Resolver::resolveCall(Scope& scope, ast::AppExpr& expr) {
	// Special case for calls with one or two parameters - these can map to builtin operations.
	if(auto lhs = expr.args) {
		if(auto rhs = expr.args->next) {
			if(!rhs->next) {
				// Two arguments.
				resolveBinaryCall(scope, expr.callee, lhs->item, rhs->item);
			}
		} else {
			// Single argument.
			resolveUnaryCall(scope, expr.callee, lhs->item);
		}
	}

	// Create a list of function arguments.
	ExprList* args = nullptr;
	if(expr.args) {
		auto arg = expr.args;
		args = build<ExprList>(resolveExpression(scope, arg->item));
		auto a = args;
		arg = arg->next;
		while(arg) {
			a->next = build<ExprList>(resolveExpression(scope, arg->item));
			a = a->next;
			arg = arg->next;
		}
	}

	// Find the function to call.
	if(auto fun = findFunction(scope, expr.callee, args)) {
		return build<AppExpr>(*fun, args);
	}

	// No need for errors - each failure above this would print an error.
	return nullptr;
}

Expr* Resolver::resolveVar(Scope& scope, Id name) {
    // This can be either a variable read or a function call.
    // This is resolved by looking through the current scope.
    if(auto var = scope.findVar(name)) {
        // This is a variable being read.
        return build<VarExpr>(var);
    } else {
        // No local or global variable was found.
        // Check if this is a function instead.
        if(auto fun = scope.findFun(name)) {
            // This is a function being called with zero arguments.
			// TODO: Create a closure if the function actually takes more arguments.
            return build<AppExpr>(*fun, nullptr);
        } else {
            // No variable or function was found; we are out of luck.
			error("could not find a function or variable named '%@'", context.Find(name).name);
            return nullptr;
        }
    }
}

Expr* Resolver::resolveIf(Scope& scope, ast::IfExpr& expr) {
	auto cond = *resolveExpression(scope, expr.cond);
	auto then = *resolveExpression(scope, expr.then);
    auto otherwise = expr.otherwise ? resolveExpression(scope, expr.otherwise) : nullptr;
	bool useResult = false;

	// Find the type of the expression.
	// If-expressions without an else-part can fail and never return a value.
	// If there is an else-part then both branches must return the same type.
	auto type = types.getUnit();
	if(otherwise) {
		if(then.type->isKnown() && otherwise->type->isKnown()) {
			if(then.type == otherwise->type) {
				type = then.type;
				useResult = true;
			} else {
				// TODO: Only generate this error if the result is actually used.
				error("the then and else branches of an if-expression must return the same type");
				return nullptr;
			}
		} else {
			type = types.getUnknown();
		}
	}

    return build<IfExpr>(cond, then, otherwise, type, useResult);
}

Expr* Resolver::resolveDecl(Scope& scope, ast::DeclExpr& expr) {
	// If the caller knows the type of this variable, it must be the same as the assigned content.
	// If no content is provided, the variable will be unusable until it is explicitly assigned.
	Expr* content = nullptr;
	TypeRef type;
	if(expr.content) {
		content = resolveExpression(scope, expr.content);
		type = content->type;
	} else {
		type = types.getUnknown();
		if(expr.constant) {
			error("constant variables must be initialized at their declaration");
		}
	}

	// Make sure this scope doesn't already have a variable with the same name.
	Variable* var;
	if((var = scope.findVar(expr.name))) {
		error("redefinition of '%@'", var->name);
	} else {
		// Create the variable allocation.
		var = build<Variable>(expr.name, type, scope, expr.constant);
		scope.variables += var;
	}

	// If the variable was assigned, we return the assignment expression.
	// Otherwise, just return a dummy expression.
	// This will ensure a compilation error if the declaration is used directly.
	if(content)
		return build<AssignExpr>(*var, *content);
	else
		return build<EmptyDeclExpr>(*var);
}

Expr* Resolver::resolveAssign(Scope& scope, ast::AssignExpr& expr) {
	// Make sure the type can be assigned to.
	auto target = resolveExpression(scope, expr.target);
	if(target->kind == Expr::Var) {
		auto value = resolveExpression(scope, expr.value);

		// Perform an implicit conversion if needed.
		if(target->type != value->type) {
			value = implicitCoerce(*value, target->type);
			if(!value) {
				error("assigning to '' from incompatible type ''");
				//TODO: Implement Type printing.
				//error("assigning to '%@' from incompatible type '%@'", target->type, value->type);
				return nullptr;
			}
		}

		return build<AssignExpr>(*((VarExpr*)target)->var, *value);
	} else if(target->kind == Expr::Field) {
		// TODO: Assign to field.
		return &emptyExpr;
	} else {
		error("expression is not assignable");
	}
	
	return nullptr;
}

Expr* Resolver::resolveWhile(Scope& scope, ast::WhileExpr& expr) {
	auto cond = resolveExpression(scope, expr.cond);
    if(cond->type == types.getBool()) {
        auto loop = resolveExpression(scope, expr.loop);
        return build<WhileExpr>(*cond, *loop, types.getUnit());
    } else {
        error("while loop condition must resolve to boolean type");
        return nullptr;
    }
}

Expr* Resolver::resolveCoerce(Scope& scope, ast::CoerceExpr& expr) {
	// Note that the coercion expression on AST level is different from resolved coercions.
	// The functionality of a coercion depends on what it is applied to:
	//  - Coercing an int or float literal will convert the value to any compatible type, even if that type is smaller.
	//    This conversion should be done on compile time and exists to make it possible to initialize small data types.
	//    "let i = 0: U8" would fail to compile without literal coercion.
	//  - Coercing a declaration assigns a definite type to the declared variable.
	//    "var x: Float" makes sure that x will be of type Float when it is initialized later.
	//  - Coercing a call-expression tells the compiler the resulting type of the call.
	//    This is useful when the type of a function call could not be resolved otherwise,
	//    such as a generic function whose return type is independent from its arguments.
	//    explicitly coercing such a call will allow it to be compiled.
	//    This use case is actually quite common and used for casting: "let x = 12345678, y = truncate x: U8"
	//    If the function is not generic, an implicit conversion is applied.
	//  - Finally, for any other use case other than the ones above, coercion acts like an implicit conversion.
	//    This means that in order to convert to some incompatible type, you still have to convert explicitly.
	if(expr.target->isLiteral()) {
		// Perform a literal coercion. This discards the coerce-expression.
		return literalCoerce(((ast::LitExpr*)expr.target)->literal, resolveType(scope, expr.kind));
	} else if(expr.target->isDecl()) {
		// Perform a declaration coercion.
		// This type of coercion is only valid if the declaration is still empty (has no type).
		// Otherwise, it is treated as a normal coercion of an existing variable.
		// Note that this works with code like "var x = 1: U8", since the coercion is applied to the literal.
		// This particular branch only executes for code like "var x: U8".
		auto decl = (EmptyDeclExpr*)resolveDecl(scope, *(ast::DeclExpr*)expr.target);
		ASSERT(decl->kind == Expr::EmptyDecl);
		ASSERT(!decl->type->isKnown());

		// Set the type of the referenced variable.
		decl->type = resolveType(scope, expr.kind);
		decl->var.type = decl->type;
		return decl;
	} else if(expr.target->isCall()) {
		// TODO: Generic function stuff.
		// For now, just do the default.
	}

	// Perform an implicit coercion as the default.
	return implicitCoerce(*resolveExpression(scope, expr.target), resolveType(scope, expr.kind));
}

Expr* Resolver::resolveField(Scope& scope, ast::FieldExpr& expr) {
	return nullptr;
}

Expr* Resolver::resolveConstruct(Scope& scope, ast::ConstructExpr& expr) {
	return nullptr;
}

Expr* Resolver::resolvePrimitiveOp(Scope& scope, PrimitiveOp op, resolve::ExprRef lhs, resolve::ExprRef rhs) {
	// This is either a pointer or primitive.
	if(lhs.type->isPointer()) {
		auto lt = (const PtrType*)lhs.type;
		if(rhs.type->isPointer()) {
			auto rt = (const PtrType*)lhs.type;
			if(auto type = getPtrOpType(op, lt, rt)) {
				auto list = build<ExprList>(&lhs, build<ExprList>(&rhs));
				return build<AppPExpr>(op, list, type);
			}
		} else {
			auto rt = ((const PrimType*)rhs.type)->type;
			if(auto type = getPtrOpType(op, lt, rt)) {
				auto list = build<ExprList>(&lhs, build<ExprList>(&rhs));
				return build<AppPExpr>(op, list, type);
			}
		}
	} else if(rhs.type->isPointer()) {
		error("This built-in operator cannot be applied to a primitive and a pointer");
	} else {
		auto lt = ((const PrimType*)lhs.type)->type;
		auto rt = ((const PrimType*)rhs.type)->type;
		if(auto type = getBinaryOpType(op, lt, rt)) {
			auto list = build<ExprList>(&lhs, build<ExprList>(&rhs));
			return build<AppPExpr>(op, list, type);
		}
	}
	return nullptr;
}
	
Expr* Resolver::resolvePrimitiveOp(Scope& scope, PrimitiveOp op, resolve::ExprRef dst) {
	// The type is either a pointer or primitive.
	if(dst.type->isPointer()) {
		// Currently no unary operators are defined for pointers.
		error("This built-in operator cannot be applied to pointer types");
	} else {
		auto type = ((const PrimType*)dst.type)->type;
		if(auto rtype = getUnaryOpType(op, type)) {
			auto list = build<ExprList>(&dst);
			return build<AppPExpr>(op, list, rtype);
		}
	}
	return nullptr;
}
	
Variable* Resolver::resolveArgument(ScopeRef scope, ast::Arg& arg) {
	auto type = arg.type ? resolveType(scope, arg.type) : types.getUnknown();
	return build<Variable>(arg.name, type, scope, arg.constant);
}

TypeRef Resolver::resolveType(ScopeRef scope, ast::TypeRef type) {
	// Check if this is a primitive type.
	if(type->kind == ast::Type::Unit) {
		return types.getUnit();
	} else if(type->kind == ast::Type::Ptr) {
		ast::Type t{ast::Type::Con, type->con};
		return types.getPtr(resolveType(scope, &t));
	} else {
		// Check if this is a primitive type.
		if(auto t = types.primMap.Get(type->con)) {
			return *t;
		} else {
			return types.getUnknown();
		}
	}
}

const Type* Resolver::getBinaryOpType(PrimitiveOp op, PrimitiveType lhs, PrimitiveType rhs) {
    if(op < PrimitiveOp::FirstBit) {
        // Arithmetic operators return the largest type.
        if(arithCompatible(lhs, rhs)) {
            return types.getPrim(Core::Min(lhs, rhs));
        } else {
            error("arithmetic operator on incompatible primitive types");
        }
    } else if(op < PrimitiveOp::FirstCompare) {
        // Bitwise operators return the largest type.
		if(bitCompatible(lhs, rhs)) {
            return types.getPrim(Core::Min(lhs, rhs));
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

const Type* Resolver::getPtrOpType(PrimitiveOp op, const PtrType* ptr, PrimitiveType prim) {
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

const Type* Resolver::getPtrOpType(PrimitiveOp op, const PtrType* lhs, const PtrType* rhs) {
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

const Type* Resolver::getUnaryOpType(PrimitiveOp op, PrimitiveType type) {
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
        DebugError("Not a unary operator or unsupported!");
    }
    return nullptr;
}

PrimitiveOp* Resolver::tryPrimitiveOp(ast::ExprRef callee) {
	if(callee->isVar()) {
		return primitiveMap.Get(((const ast::VarExpr&)callee).name);
	} else {
		return nullptr;
	}
}

CoerceExpr* Resolver::implicitCoerce(ExprRef src, TypeRef dst) {
	// Only primitive types can be implicitly converted:
	//  - floating point types can be converted to a larger type.
	//  - integer types can be converted to a larger type.
	//  - pointer types can be converted to Bool.
	//  - Bool can be converted to an integer type.
	// Special case: literals can be converted into any type of the same category.
	if(src.type->isPrimitive() && dst->isPrimitive()) {
		auto s = ((PrimType*)src.type)->type;
		auto d = ((PrimType*)dst)->type;
		if(category(s) == category(d) && (src.kind == Expr::Lit || d >= s)) {
			return build<CoerceExpr>(src, dst);
		} else {
			error("a primitive type can only be implicitly converted to a larger type");
		}
	} else if(src.type->isPointer()) {
		if(dst->isBool()) {
			return build<CoerceExpr>(src, dst);
		} else {
			error("pointer types can only be implicitly converted to Bool");
		}
	} else if(src.type->isBool() && dst->isPrimitive()) {
		if(((PrimType*)dst)->type < PrimitiveType::FirstFloat) {
			return build<CoerceExpr>(src, dst);
		} else {
			error("booleans can only be implicitly converted to integer types");
		}
	} else {
		error("only primitive types or pointers can be implicitly converted");
	}

	return nullptr;
}

LitExpr* Resolver::literalCoerce(const ast::Literal& lit, TypeRef dst) {
	// Literal conversion rules:
	//  - Integer and Float literals can be converted to any other integer or float
	//    (warnings are issued if the value would not fit).
	Literal literal = lit;
	if(dst->isPrimitive()) {
		auto ptype = ((const PrimType*)dst)->type;
		if(lit.type == ast::Literal::Int) {
			if(ptype < PrimitiveType::FirstFloat) {
				// No need to change the literal.
			} else if(ptype < PrimitiveType::FirstOther) {
				literal.f = lit.i;
				literal.type = ast::Literal::Float;
			}
		} else if(lit.type == ast::Literal::Float) {
			if(ptype < PrimitiveType::FirstFloat) {
				literal.i = lit.f;
				literal.type = ast::Literal::Int;
			} else if(ptype < PrimitiveType::FirstOther) {
				// No need to change the literal.
			}
		} else {
			error("cannot convert this literal to the target type");
		}
	} else {
		error("literals can only be converted to primitive types");
	}

	// We always return a valid value to simplify the resolver.
	// If an error occurred the code generator will not be invoked.
	return build<LitExpr>(literal, dst);
}

Function* Resolver::findFunction(ScopeRef scope, ast::ExprRef callee, ExprList* args) {
	if(callee->isVar()) {
		auto name = ((const ast::VarExpr&)callee).name;
		if(auto fun = scope.findFun(name)) {
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

ast::InfixExpr& Resolver::reorder(ast::InfixExpr& expr) {
	auto e = &expr;
	auto res = e;
	uint lowest = context.FindOp(e->op).precedence;

	while(e->rhs->isInfix()) {
		auto rhs = (ast::InfixExpr*)e->rhs;
		auto first = context.FindOp(e->op);
		auto second = context.FindOp(rhs->op);

		// Reorder if needed.
		if(first.precedence > second.precedence ||
		  (first.precedence == second.precedence &&
		  (first.associativity == ast::Assoc::Left || second.associativity == ast::Assoc::Left))) {
			e->rhs = rhs->lhs;
			rhs->lhs = e;
			if(second.precedence < lowest) {
				res = rhs;
				lowest = second.precedence;
			}
		}

		e = rhs;
	}
	return *res;
}

nullptr_t Resolver::error(const char* text) {
	Core::LogError(text);
	return nullptr;
}

template<class P, class... Ps>
nullptr_t Resolver::error(const char* text, P first, Ps... more) {
	Core::LogError(text, first, more...);
	return nullptr;
}

}} // namespace athena::resolve
