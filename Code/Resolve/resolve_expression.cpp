
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
		case ast::Expr::Unit:
			return build<EmptyExpr>(types.getUnit());
		case ast::Expr::Multi:
			return resolveMulti(scope, *(ast::MultiExpr*)expr);
		case ast::Expr::Lit:
			return resolveLiteral(scope, ((ast::LitExpr*)expr)->literal);
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
		case ast::Expr::MultiIf:
			return resolveMultiIf(scope, *(ast::MultiIfExpr*)expr);
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
		case ast::Expr::TupleConstruct:
			return resolveAnonConstruct(scope, *(ast::TupleConstructExpr*)expr);
		case ast::Expr::Case:
			return resolveCase(scope, *(ast::CaseExpr*)expr);
		default:
			FatalError("Unsupported expression type.");
	}

	return nullptr;
}

Expr* Resolver::resolveMulti(Scope& scope, ast::MultiExpr& expr) {
	Exprs es;
	ast::walk(expr.exprs, [&](auto i) {es += resolveExpression(scope, i);});
	return build<MultiExpr>(Core::Move(es));
}
	
Expr* Resolver::resolveLiteral(Scope& scope, ast::Literal& literal) {
	return build<LitExpr>(literal, getLiteralType(types, literal));
}

Expr* Resolver::resolveInfix(Scope& scope, ast::InfixExpr& expr) {
	auto& e = reorder(expr);
	return resolveBinaryCall(scope, e.op, *getRV(*resolveExpression(scope, e.lhs)), *getRV(*resolveExpression(scope, e.rhs)));
}

Expr* Resolver::resolvePrefix(Scope& scope, ast::PrefixExpr& expr) {
	return resolveUnaryCall(scope, expr.op, *getRV(*resolveExpression(scope, expr.dst)));
}

Expr* Resolver::resolveBinaryCall(Scope& scope, Id function, ExprRef lt, ExprRef rt) {
	// Check if this can be a primitive operation.
	// Note that primitive operations can be both functions and operators.
	if(auto op = tryPrimitiveBinaryOp(function)) {
		// This means that built-in binary operators cannot be overloaded for any pointer or primitive type.
		if(*op < PrimitiveOp::FirstUnary)
			if(auto e = resolvePrimitiveOp(scope, *op, lt, rt))
				return e;
	}

	// Otherwise, create a normal function call.
	auto args = build<ExprList>(&lt, build<ExprList>(&rt));
	if(auto func = findFunction(scope, function, args)) {
		args->item = implicitCoerce(*args->item, func->arguments[0]->type);
		args->next->item = implicitCoerce(*args->next->item, func->arguments[1]->type);
		return build<AppExpr>(*func, args);
	} else {
		// No need for an error; this is done by findFunction.
		return nullptr;
	}
}

Expr* Resolver::resolveUnaryCall(Scope& scope, Id function, ExprRef target) {
	// Check if this can be a primitive operation.
	// Note that primitive operations can be both functions and operators.
	if(target.type->isPtrOrPrim()) {
		if(auto op = tryPrimitiveUnaryOp(function)) {
			// This means that built-in unary operators cannot be overloaded for any pointer or primitive type.
			if(*op >= PrimitiveOp::FirstUnary)
				if(auto e = resolvePrimitiveOp(scope, *op, target))
					return e;
		}
	}

	// Otherwise, create a normal function call.
	auto args = build<ExprList>(&target);
	if(auto func = findFunction(scope, function, args)) {
		args->item = implicitCoerce(*args->item, func->arguments[0]->type);
		return build<AppExpr>(*func, args);
	} else {
		// No need for an error; this is done by findFunction.
		return nullptr;
	}
}

Expr* Resolver::resolveCall(Scope& scope, ast::AppExpr& expr) {
	// If the operand is a field expression we need special handling, since there are several options:
	// - the field operand is an actual field of its target and has a function type, which we call.
	// - the field operand is not a field, and we produce a function call with the target as first parameter.
	if(expr.callee->type == ast::Expr::Field) {
		return resolveField(scope, *(ast::FieldExpr*)expr.callee, expr.args);
	}

	// Special case for calls with one or two parameters - these can map to builtin operations.
	if(expr.callee->type == ast::Expr::Var) {
		auto name = ((ast::VarExpr*)expr.callee)->name;
		if (auto lhs = expr.args) {
			if (auto rhs = expr.args->next) {
				if (!rhs->next) {
					// Two arguments.
					return resolveBinaryCall(scope, name, *getRV(*resolveExpression(scope, lhs->item)), *getRV(*resolveExpression(scope, rhs->item)));
				}
			} else {
				// Single argument.
				return resolveUnaryCall(scope, name, *getRV(*resolveExpression(scope, lhs->item)));
			}
		}
	}

	// Create a list of function arguments.
	auto args = resolveExpressions(scope, expr.args);

	// Find the function to call.
	if(auto fun = findFunction(scope, expr.callee, args)) {
		auto a = args;
		for(auto b : fun->arguments) {
			a->item = implicitCoerce(*a->item, b->type);
			a = a->next;
		}
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
		// Constant variables are rvalues, while mutables are lvalues.
		auto type = var->constant ? var->type : types.getLV(var->type);
		return build<VarExpr>(var, type);
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
	auto& cond = *resolveCondition(scope, expr.cond);
	auto& then = *getRV(*resolveExpression(scope, expr.then));
	auto otherwise = expr.otherwise ? getRV(*resolveExpression(scope, expr.otherwise)) : nullptr;
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

Expr* Resolver::resolveMultiIf(Scope& scope, ast::MultiIfExpr& expr) {
	// Create a chain of ifs.
	if(expr.cases) {
		auto cases = expr.cases;
		IfExpr* list = build<IfExpr>(*resolveExpression(scope, cases->item.cond), *resolveExpression(scope, cases->item.then));
		IfExpr* current = list;
		cases = cases->next;
		while(cases) {
			auto cond = resolveExpression(scope, cases->item.cond);
			if(alwaysTrue(*cond)) {
				current->otherwise = resolveExpression(scope, cases->item.then);
				break;
			} else {
				current->otherwise = build<IfExpr>(*cond, *resolveExpression(scope, cases->item.then));
				current = (IfExpr*)current->otherwise;
			}

			cases = cases->next;
		}

		return list;
	}

	return nullptr;
}

Expr* Resolver::resolveDecl(Scope& scope, ast::DeclExpr& expr) {
	// If the caller knows the type of this variable, it must be the same as the assigned content.
	// If no content is provided, the variable will be unusable until it is explicitly assigned.
	Expr* content = nullptr;
	TypeRef type;
	if(expr.content) {
		content = getRV(*resolveExpression(scope, expr.content));
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
		// Mutable variables that shadow function parameters are automatically initialized.
		if(!content && var->funParam && var->constant) {
			var = build<Variable>(expr.name, var->type, scope, false);
			content = getRV(*resolveVar(scope, expr.name));
			scope.shadows += var;
		} else {
			error("redefinition of '%@'", var->name);
		}
	} else {
		// Create the variable allocation.
		var = build<Variable>(expr.name, type, scope, expr.constant);
		scope.variables += var;
	}

	// If the variable was assigned, we return the assignment expression.
	// Otherwise, just return a dummy expression.
	// This will ensure a compilation error if the declaration is used directly.
	if(content) {
		// Constants are registers, while variables are on the stack.
		if(var->constant)
			return build<AssignExpr>(*var, *content);
		else {
			// Declarations are themselves lvalues; this is needed for correct code generation.
			// If we want to disallow this we need to do so explicitly.
			auto ltype = types.getLV(type);
			return build<StoreExpr>(*build<VarExpr>(var, ltype), *content, ltype);
		}
	} else {
		return build<EmptyDeclExpr>(*var);
	}
}

Expr* Resolver::resolveAssign(Scope& scope, ast::AssignExpr& expr) {
	// Only lvalues can be assigned.
	auto target = resolveExpression(scope, expr.target);
	if(target->type->isLvalue()) {
		auto value = getRV(*resolveExpression(scope, expr.value));
		TypeRef type;

		// Perform an implicit conversion if needed.
		value = implicitCoerce(*value, ((LVType*)target->type)->type);
		if(!value) {
			error("assigning to '' from incompatible type ''");
			//TODO: Implement Type printing.
			//error("assigning to '%@' from incompatible type '%@'", target->type, value->type);
			return nullptr;
		}

		return build<StoreExpr>(*target, *value, target->type);
	} else {
		error("expression is not assignable");
		return nullptr;
	}
}

Expr* Resolver::resolveWhile(Scope& scope, ast::WhileExpr& expr) {
	// The while loop returns nothing, so there is no need to convert the body to an lvalue.
	auto loop = resolveExpression(scope, expr.loop);
	auto cond = resolveCondition(scope, expr.cond);
	return build<WhileExpr>(*cond, *loop, types.getUnit());
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

	// Perform an implicit coercion as the default. Also handles lvalues.
	return implicitCoerce(*resolveExpression(scope, expr.target), resolveType(scope, expr.kind));
}

Expr* Resolver::resolveField(Scope& scope, ast::FieldExpr& expr, ast::ExprList* args) {
	auto target = resolveExpression(scope, expr.target);
	bool constant = target->kind == Expr::Var && ((VarExpr*)target)->var->constant && target->type->isTuple();

	// Check if this is a field or function call expression.
	// For types without named fields, this is always a function call.
	// For types with named fields, this is a field expression if the target is a VarExpr,
	// and the type has a field with that name.
	if(target->type->isTupleOrIndirect() && expr.field->type == ast::Expr::Var) {
		TupleType* tupType;
		if(target->type->isTuple()) {
			tupType = (TupleType*)target->type;
		} else if(target->type->isPointer()) {
			tupType = (TupleType*)((PtrType*)target->type)->type;
		} else {
			ASSERT(target->type->isLvalue());
			tupType = (TupleType*)((LVType*)target->type)->type;
		}

		if(auto f = tupType->findField(((ast::VarExpr*)expr.field)->name)) {
			auto fexpr = build<FieldExpr>(*target, f, constant ? f->type : types.getLV(f->type));
			if(args) {
				// TODO: Indirect calls.
				//return resolveCall(scope, )
				return nullptr;
			} else {
				return fexpr;
			}
		}
	}

	// Generate a function call from the field, with the target as the first parameter.
	//ast::AppExpr app{};
	//return resolveCall(scope);
	return nullptr;
}

Expr* Resolver::resolveConstruct(Scope& scope, ast::ConstructExpr& expr) {
	Type* type = resolveType(scope, expr.type, true);

	if(type->isPrimitive()) {
		// Primitive types other than Bool have a single constructor of the same name.
		// Bool has the built-in constructors True and False.
		if(type->isBool()) {
			auto name = context.Find(expr.type->con).name;

			Literal lit;
			lit.type = Literal::Bool;
			if(name == "True") lit.i = 1;
			else if(name == "False") lit.i = 0;
			else error("not a valid constructor for Bool");

			return build<LitExpr>(lit, types.getBool());
		} else {
			if(!expr.args || expr.args->next) {
				error("primitive types take a single constructor argument");
				return nullptr;
			}

			return implicitCoerce(*resolveExpression(scope, expr.args->item), type);
		}
	}

	if(type->isVariant()) {
		auto con = scope.findConstructor(expr.type->con);
		auto cone = build<ConstructExpr>(type, con);
		auto f = expr.args;
		uint counter = 0;
		while(f) {
			auto e = getRV(*resolveExpression(scope, f->item));
			auto t = con->contents[counter];
			if(!typeCheck.compatible(*e, t)) {
				error("incompatible constructor argument type");
			}

			cone->args += ConstructArg{counter, *implicitCoerce(*e, t)};
			f = f->next;
			counter++;
		}

		if(counter != con->contents.Count()) error("constructor argument count does not match type");
		return cone;
	}

	if(type->isTuple()) {
		auto ttype = (TupleType*)type;
		auto con = build<ConstructExpr>(type);
		auto f = expr.args;
		uint counter = 0;
		while(f) {
			auto e = getRV(*resolveExpression(scope, f->item));
			auto t = ttype->fields[counter].type;
			if(!typeCheck.compatible(*e, t)) {
				error("incompatible constructor argument type");
			}

			con->args += ConstructArg{counter, *implicitCoerce(*e, t)};
			f = f->next;
			counter++;
		}

		if(counter != ttype->fields.Count()) error("constructor argument count does not match type");
		return con;
	}
	
	return nullptr;
}

Expr* Resolver::resolveAnonConstruct(Scope& scope, ast::TupleConstructExpr& expr) {
	// Generate a hash for the fields.
	Core::Hasher h;
	auto f = expr.args;
	auto con = build<ConstructExpr>(types.getUnknown());
	uint index = 0;
	while(f) {
		ASSERT(f->item.defaultValue);
		auto e = getRV(*resolveExpression(scope, f->item.defaultValue));
		h.Add(e->type);
		if(f->item.name) h.Add(f->item.name());

		con->args += ConstructArg{index, *e};

		index++;
		f = f->next;
	}

	// Check if this kind of tuple has been used already.
	TupleType* result = nullptr;
	if(!types.getTuple(h, result)) {
		// Otherwise, create the type.
		new (result) TupleType;
		uint i = 0;
		f = expr.args;
		while(f) {
			auto t = con->args[i].expr.type;
			result->fields += Field{f->item.name ? f->item.name() : 0, i, t, result, nullptr, true};
			f = f->next;
			i++;
		}
	}
	con->type = result;
	return con;
}

Expr* Resolver::resolveCase(Scope& scope, ast::CaseExpr& expr) {
	auto alt = expr.alts;
	if(alt) {
		auto pivot = resolveExpression(scope, expr.pivot);
		
		ScopedExpr* test;
		ScopedExpr* first_test;

		test = build<ScopedExpr>(scope);
		auto pat = resolvePattern(test->scope, *pivot, *alt->item.pattern);
		auto result = resolveExpression(test->scope, alt->item.expr);

		if(alwaysTrue(*pat)) {
			test->contents = build<MultiExpr>(pat, build<MultiExpr>(result));
			test->type = result->type;
			return test;
		} else {
			test->contents = build<IfExpr>(*pat, *result, nullptr, result->type, true);
			test->type = test->contents->type;
			first_test = test;
			alt = alt->next;
		}

		while(alt) {
			auto s = build<ScopedExpr>(scope);
			pat = resolvePattern(s->scope, *pivot, *alt->item.pattern);
			result = resolveExpression(s->scope, alt->item.expr);
			if(alwaysTrue(*pat)) {
				s->contents = build<MultiExpr>(pat, build<MultiExpr>(result));
				s->type = result->type;
				((IfExpr*)test->contents)->otherwise = s;
				return first_test;
			} else {
				s->contents = build<IfExpr>(*pat, *result, nullptr, result->type, true);
				s->type = s->contents->type;
				((IfExpr*)test->contents)->otherwise = s;
				test = s;
				alt = alt->next;
			}
		}

		return first_test;
	} else {
		return build<EmptyExpr>(types.getUnit());
	}
}

Expr* Resolver::resolvePattern(Scope& scope, ExprRef pivot, ast::Pattern& pat) {
	switch(pat.kind) {
		case ast::Pattern::Var: {
			auto var = build<Variable>(((ast::VarPattern&)pat).var, pivot.type, scope, true);
			scope.variables += var;
			return build<MultiExpr>(Exprs{build<AssignExpr>(*var, *getRV(pivot)), createTrue()});
		}
		case ast::Pattern::Lit:
			return createCompare(scope, *getRV(pivot), *resolveLiteral(scope, ((ast::LitPattern&)pat).lit));
		case ast::Pattern::Any:
			return createTrue();
		case ast::Pattern::Tup:
			return nullptr;
		case ast::Pattern::Con: {
			auto l = build<Literal>();
			l->i = scope.findConstructor(((ast::ConPattern&)pat).constructor)->index;
			l->type = Literal::Int;
			return createCompare(scope, *build<FieldExpr>(pivot, -1, types.getInt()), *resolveLiteral(scope, *l));
		}
	}
}

Expr* Resolver::resolveCondition(ScopeRef scope, ast::ExprRef expr) {
	return implicitCoerce(*resolveExpression(scope, expr), types.getBool());
}

ExprList* Resolver::resolveExpressions(Scope& scope, ast::ExprList* list) {
	// Create a list of function arguments.
	ExprList* args = nullptr;
	if(list) {
		auto arg = list;
		args = build<ExprList>(getRV(*resolveExpression(scope, arg->item)));
		auto a = args;
		arg = arg->next;
		while(arg) {
			a->next = build<ExprList>(getRV(*resolveExpression(scope, arg->item)));
			a = a->next;
			arg = arg->next;
		}
	}
	return args;
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

bool Resolver::alwaysTrue(ExprRef expr) {
	// TODO: Perform constant folding.
	auto e = &expr;
	if(expr.kind == Expr::Multi) e = ((MultiExpr*)e)->es.Back();

	return e->kind == Expr::Lit
		   && ((LitExpr*)e)->literal.type == Literal::Bool
		   && ((LitExpr*)e)->literal.i == 1;
}

}} // namespace athena::resolve
