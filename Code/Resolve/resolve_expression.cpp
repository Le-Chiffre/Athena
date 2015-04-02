
#include "resolve.h"

namespace athena {
namespace resolve {

inline TypeRef getLiteralType(TypeManager& m, const Literal& l) {
	switch(l.type) {
		case Literal::Int: return m.getInt();
		case Literal::Float: return m.getFloat();
		case Literal::Char: return m.getU8();
		case Literal::String: return m.getString();
		default: FatalError("Unknown literal type."); return nullptr;
	}
}

Expr* Resolver::resolveExpression(Scope& scope, ast::ExprRef expr) {
	switch(expr->type) {
		case ast::Expr::Multi:
			return resolveMulti(scope, *(ast::MultiExpr*)expr);
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

Expr* Resolver::resolveMulti(Scope& scope, ast::MultiExpr& expr) {
	MultiExpr* start = build<MultiExpr>(resolveExpression(scope, expr.exprs->item));
	auto current = start;
	auto e = expr.exprs->next;
	while(e) {
		current->next = build<MultiExpr>(resolveExpression(scope, e->item));
		current = current->next;
		e = e->next;
	}
	return start;
}
	
Expr* Resolver::resolveMultiWithRet(Scope& scope, ast::MultiExpr& expr) {
	MultiExpr* start = build<MultiExpr>(resolveExpression(scope, expr.exprs->item));
	auto current = start;
	auto e = expr.exprs->next;
	while(1) {
		// If this is the last expression, insert a return.
		if(e->next) {
			current->next = build<MultiExpr>(resolveExpression(scope, e->item));
			current = current->next;
			e = e->next;
		} else {
			current->next = build<MultiExpr>(build<RetExpr>(*resolveExpression(scope, e->item)));
			break;
		}
	}
	return start;
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
				return resolveBinaryCall(scope, expr.callee, lhs->item, rhs->item);
			}
		} else {
			// Single argument.
			return resolveUnaryCall(scope, expr.callee, lhs->item);
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
		if(auto fun = findFunction(scope, name, nullptr)) {
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
	auto& cond = *resolveExpression(scope, expr.cond);
	auto& then = *resolveExpression(scope, expr.then);
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
	if((var = scope.findLocalVar(expr.name))) {
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

}} // namespace athena::resolve
