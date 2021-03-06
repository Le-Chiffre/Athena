
#include "resolve.h"

namespace athena {
namespace resolve {

inline Type* getLiteralType(TypeManager& m, const Literal& l) {
	switch(l.type) {
		case Literal::Int: return m.getInt();
		case Literal::Float: return m.getFloat();
		case Literal::Char: return m.getU8();
		case Literal::String: return m.getString();
		case Literal::Bool: return m.getBool();
		default: assert("Unknown literal type." == 0); return nullptr;
	}
}

Expr* Resolver::resolveExpression(Scope& scope, ast::ExprRef expr, bool used) {
	switch(expr->type) {
		case ast::Expr::Unit:
			return build<EmptyExpr>(types.getUnit());
		case ast::Expr::Multi:
			return resolveMulti(scope, *(ast::MultiExpr*)expr, used);
		case ast::Expr::Lit:
			return resolveLiteral(scope, ((ast::LitExpr*)expr)->literal);
		case ast::Expr::App:
			return resolveCall(scope, *(ast::AppExpr*)expr);
		case ast::Expr::Lam:
			return resolveLambda(scope, *(ast::LamExpr*)expr);
		case ast::Expr::Infix:
			return resolveInfix(scope, *(ast::InfixExpr*)expr);
		case ast::Expr::Prefix:
			return resolvePrefix(scope, *(ast::PrefixExpr*)expr);
		case ast::Expr::Var:
			// This can be either a variable or a function call without parameters.
			return resolveVar(scope, ((ast::VarExpr*)expr)->name);
		case ast::Expr::If:
			return resolveIf(scope, *(ast::IfExpr*)expr, used);
		case ast::Expr::MultiIf:
			return resolveMultiIf(scope, ((ast::MultiIfExpr*)expr)->cases, used);
		case ast::Expr::Decl:
			return resolveDecl(scope, *(ast::DeclExpr*)expr);
		case ast::Expr::Assign:
			return resolveAssign(scope, *(ast::AssignExpr*)expr);
		case ast::Expr::While:
			return resolveWhile(scope, *(ast::WhileExpr*)expr);
		case ast::Expr::Nested:
			return resolveExpression(scope, ((ast::NestedExpr*)expr)->expr, used);
		case ast::Expr::Coerce:
			return resolveCoerce(scope, *(ast::CoerceExpr*)expr);
		case ast::Expr::Field:
			return resolveField(scope, *(ast::FieldExpr*)expr);
		case ast::Expr::Construct:
			return resolveConstruct(scope, *(ast::ConstructExpr*)expr);
		case ast::Expr::TupleConstruct:
			return resolveAnonConstruct(scope, *(ast::TupleConstructExpr*)expr);
		case ast::Expr::Case:
			return resolveCase(scope, *(ast::CaseExpr*)expr, used);
		default:
			assert("Unsupported expression type." == 0);
	}

	return nullptr;
}

Expr* Resolver::resolveMulti(Scope& scope, ast::MultiExpr& expr, bool used) {
	Exprs es;
	auto e = expr.exprs;
	while(e) {
		// Expressions that are part of a statement list are never used, unless they are the last in the list.
		es << this->resolveExpression(scope, e->item, e->next ? false : used);
		e = e->next;
	}
	return build<MultiExpr>(std::move(es));
}

Expr* Resolver::resolveLiteral(Scope& scope, ast::Literal& literal) {
	return build<LitExpr>(literal, getLiteralType(types, literal));
}

Expr* Resolver::resolveInfix(Scope& scope, ast::InfixExpr& expr) {
	ast::InfixExpr* e;
	if(expr.ordered) e = &expr;
	else e = reorder(expr);

	return resolveBinaryCall(scope, e->op,
							 *getRV(*resolveExpression(scope, e->lhs, true)),
							 *getRV(*resolveExpression(scope, e->rhs, true)));
}

Expr* Resolver::resolvePrefix(Scope& scope, ast::PrefixExpr& expr) {
	return resolveUnaryCall(scope, expr.op, *getRV(*resolveExpression(scope, expr.dst, true)));
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

	auto args = list(&lt, list(&rt));

	// If one of the arguments has an incomplete type, create a generic call.
	if(!lt.type->resolved || !rt.type->resolved) {
		constrain(lt.type, FunConstraint(function, 0));
		constrain(rt.type, FunConstraint(function, 1));
		return build<GenAppExpr>(function, args, build<GenType>(0));
	}

	// Otherwise, create a normal function call.
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

	auto args = list(&target);

	// If the argument has an incomplete type, create a generic call.
	if(!target.type->resolved) {
		constrain(target.type, FunConstraint(function, 0));
		return build<GenAppExpr>(function, args, build<GenType>(0));
	}

	// Otherwise, create a normal function call.
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
	if(expr.callee->isField()) {
		return resolveField(scope, *(ast::FieldExpr*)expr.callee, expr.args);
	}

	// Special case for calls with one or two parameters - these can map to builtin operations.
	if(expr.callee->isVar()) {
		auto name = ((ast::VarExpr*)expr.callee)->name;
		if (auto lhs = expr.args) {
			if (auto rhs = expr.args->next) {
				if (!rhs->next) {
					// Two arguments.
					return resolveBinaryCall(scope, name, *getRV(*resolveExpression(scope, lhs->item, true)), *getRV(*resolveExpression(scope, rhs->item, true)));
				}
			} else {
				// Single argument.
				return resolveUnaryCall(scope, name, *getRV(*resolveExpression(scope, lhs->item, true)));
			}
		}
	}

	// Create a list of function arguments.
	bool resolved = true;
	auto args = map(expr.args, [&](auto e) {
		auto a = this->getRV(*this->resolveExpression(scope, e, true));
		if(!a->type->resolved) resolved = false;
		return a;
	});

	// If the arguments contain an incomplete type, create a generic call.
	if(!resolved) {
		if(expr.callee->isVar()) {
			auto name = ((ast::VarExpr*)expr.callee)->name;
			auto a = args;
			U32 i = 0;
			while(a) {
				constrain(a->item->type, FunConstraint(name, i));
				a = a->next;
				i++;
			}
			return build<GenAppExpr>(name, args, build<GenType>(0));
		} else {
			assert("Not implemented" == 0);
			return nullptr;
		}
	}

	// Otherwise, find the function to call.
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

Expr* Resolver::resolveLambda(Scope& scope, ast::LamExpr& expr) {
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
			error("could not find a function or variable named '%@'", context.find(name).name);
			return nullptr;
		}
	}
}

Expr* Resolver::resolveIf(Scope& scope, ast::IfExpr& expr, bool used) {
	return createIf(
			*resolveCondition(scope, expr.cond),
			*resolveExpression(scope, expr.then, used),
			expr.otherwise ? resolveExpression(scope, expr.otherwise, used) : nullptr, used);
}

Expr* Resolver::resolveMultiIf(Scope& scope, ast::IfCaseList* cases, bool used) {
	// Create a chain of ifs.
	if(cases) {
		auto cond = resolveCondition(scope, cases->item->cond);
		if(alwaysTrue(*cond)) {
			return resolveExpression(scope, cases->item->then, used);
		} else {
			return createIf(*cond, *resolveExpression(scope, cases->item->then, used), resolveMultiIf(scope, cases->next, used), used);
		}
	} else {
		return nullptr;
	}
}

Expr* Resolver::resolveDecl(Scope& scope, ast::DeclExpr& expr) {
	// If the caller knows the type of this variable, it must be the same as the assigned content.
	// If no content is provided, the variable will be unusable until it is explicitly assigned.
	Expr* content = nullptr;
	Type* type;
	if(expr.content) {
		content = getRV(*resolveExpression(scope, expr.content, true));
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
			scope.shadows << var;
		} else {
			error("redefinition of '%@'", var->name);
		}
	} else {
		// Create the variable allocation.
		var = build<Variable>(expr.name, type, scope, expr.constant);
		scope.variables << var;
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
	auto target = resolveExpression(scope, expr.target, true);
	if(target->type->isLvalue()) {
		auto value = getRV(*resolveExpression(scope, expr.value, true));

		// A variable declared without a type gets its type from the first location it is assigned at.
		if(target->isVar()) {
			auto var = ((VarExpr*)target)->var;
			if(var->type->isUnknown()) {
				// Set the variable type and update the target expression.
				var->type = value->type;
				target = resolveExpression(scope, expr.target, true);
			}
		}

		// Perform an implicit conversion if needed.
		value = implicitCoerce(*value, target->type->canonical);
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
	// The loop body is not used as an expression; it exists solely for side effects.
	auto loop = resolveExpression(scope, expr.loop, false);
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
	auto target = resolveExpression(scope, expr.target, true);
	auto type = resolveType(scope, expr.kind);
	if(auto l = findLiteral(*target)) {
		// Perform a literal coercion. This discards the coerce-expression.
		literalCoerce(l, type);
		updateLiteral(*target, type);
		return target;
	} else if(target->kind == Expr::EmptyDecl) {
		// Perform a declaration coercion.
		// This type of coercion is only valid if the declaration is still empty (has no type).
		// Otherwise, it is treated as a normal coercion of an existing variable.
		// Note that this works with code like "var x = 1: U8", since the coercion is applied to the literal.
		// This particular branch only executes for code like "var x: U8".
		auto decl = (EmptyDeclExpr*)target;
		assert(!decl->type->isKnown());

		// Set the type of the referenced variable.
		decl->type = type;
		decl->var.type = decl->type;
		return decl;
	} else if(target->kind == Expr::App) {
		// TODO: Generic function stuff.
		// For now, just do the default.
	}

	// Perform an implicit coercion as the default. Also handles lvalues.
	return implicitCoerce(*target, type);
}

Expr* Resolver::resolveField(Scope& scope, ast::FieldExpr& expr, ast::ExprList* args) {
	auto target = resolveExpression(scope, expr.target, true);

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
			assert(target->type->isLvalue());
			tupType = (TupleType*)target->type->canonical;
		}

		if(auto f = tupType->findField(((ast::VarExpr*)expr.field)->name)) {
			auto fexpr = createField(*target, f);
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
			auto name = context.find(expr.type->con).name;

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

			return implicitCoerce(*resolveExpression(scope, expr.args->item, true), type);
		}
	}

	if(type->isVariant()) {
		auto con = scope.findConstructor(expr.type->con);
		auto cone = build<ConstructExpr>(type, con);
		auto f = expr.args;
		U32 counter = 0;
		while(f) {
			auto e = getRV(*resolveExpression(scope, f->item, true));
			auto t = con->contents[counter];
			if(!typeCheck.compatible(*e, t)) {
				error("incompatible constructor argument type");
			}

			cone->args << ConstructArg{counter, *implicitCoerce(*e, t)};
			f = f->next;
			counter++;
		}

		if(counter != con->contents.size()) error("constructor argument count does not match type");
		return cone;
	}

	if(type->isTuple()) {
		auto ttype = (TupleType*)type;
		auto con = build<ConstructExpr>(type);
		auto f = expr.args;
		U32 counter = 0;
		while(f) {
			auto e = getRV(*resolveExpression(scope, f->item, true));
			auto t = ttype->fields[counter].type;
			if(!typeCheck.compatible(*e, t)) {
				error("incompatible constructor argument type");
			}

			con->args << ConstructArg{counter, *implicitCoerce(*e, t)};
			f = f->next;
			counter++;
		}

		if(counter != ttype->fields.size()) error("constructor argument count does not match type");
		return con;
	}

	return nullptr;
}

Expr* Resolver::resolveAnonConstruct(Scope& scope, ast::TupleConstructExpr& expr) {
	// Generate a hash for the fields.
	Hasher h;
	auto f = expr.args;
	auto con = build<ConstructExpr>(types.getUnknown());
	U32 index = 0;
	while(f) {
		assert(f->item->defaultValue);
		auto e = getRV(*resolveExpression(scope, f->item->defaultValue, true));
		h.add(e->type);
		if(f->item->name) h.add(f->item->name.force());

		con->args << ConstructArg{index, *e};

		index++;
		f = f->next;
	}

	// Check if this kind of tuple has been used already.
	TupleType* result = nullptr;
	if(!types.getTuple((uint32_t)h, result)) {
		// Otherwise, create the type.
		new (result) TupleType;
		U32 i = 0;
		f = expr.args;
		while(f) {
			auto t = con->args[i].expr.type;
			result->fields << Field{f->item->name ? f->item->name.force() : 0, i, t, result, nullptr, true};
			f = f->next;
			i++;
		}
	}
	con->type = result;
	return con;
}

Expr* Resolver::resolveAlt(Scope& scope, ExprRef pivot, ast::AltList* alt, bool used) {
	if(alt) {
		auto s = build<ScopedExpr>(scope);
		IfConds conds;
		resolvePattern(s->scope, pivot, *alt->item->pattern, conds);
		auto result = resolveExpression(s->scope, alt->item->expr, used);
		s->contents = createIf(std::move(conds), *result, resolveAlt(scope, pivot, alt->next, used), used, CondMode::And);
		s->type = result->type;
		return s;
	} else {
		return nullptr;
	}
}

Expr* Resolver::resolveCase(Scope& scope, ast::CaseExpr& expr, bool used) {
	auto alt = expr.alts;
	if(alt) {
		auto pivot = resolveExpression(scope, expr.pivot, true);
		return resolveAlt(scope, *pivot, expr.alts, used);
	} else {
		error("a case expression must have at least one alt");
		return build<EmptyExpr>(types.getUnit());
	}
}

void Resolver::resolvePattern(Scope& scope, ExprRef pivot, ast::Pattern& pat, IfConds& conds) {
	switch(pat.kind) {
		case ast::Pattern::Var: {
			auto var = build<Variable>(((ast::VarPattern&)pat).var, pivot.type, scope, true);
			scope.variables << var;
			conds << IfCond(build<AssignExpr>(*var, *getRV(pivot)), nullptr);
			break;
		}
		case ast::Pattern::Lit:
			conds << IfCond(nullptr, createCompare(scope, *getRV(pivot), *resolveLiteral(scope, ((ast::LitPattern&)pat).lit)));
			break;
		case ast::Pattern::Any:
			conds << IfCond(nullptr, nullptr);
			break;
		case ast::Pattern::Tup: {
			if(pivot.type->isTuple()) {
				auto type = (TupleType*)pivot.type;
				auto tpat = (ast::TupPattern&)pat;
				auto p = tpat.fields;
				U32 i = 0;
				while(p) {
					if(i >= type->fields.size()) {
						error("the number of patterns cannot be greater than the number of fields");
						conds << IfCond(nullptr, createFalse());
						break;
					}

					if(p->item->field) {
						if(auto field = type->findField(p->item->field.force())) {
							resolvePattern(scope, *createField(pivot, field), *p->item->pat, conds);
						} else {
							error("this field does not exist");
							conds << IfCond(nullptr, createFalse());
						}
					} else {
						resolvePattern(scope, *createField(pivot, i), *p->item->pat, conds);
					}

					p = p->next;
					i++;
				}
			} else {
				error("this pattern can only match a tuple");
				conds << IfCond(nullptr, createFalse());
			}
			break;
		}
		case ast::Pattern::Con: {
			/*
			 * Constructor patterns on variants consist of multiple steps:
			 *  - Check if the pivot and pattern constructor indices are the same.
			 *  - Retrieve the data for that constructor.
			 *  - Resolve each element pattern using the corresponding constructor data element as pivot.
			 * This is built up as a chain of ifs.
			 */
			auto type = pivot.type->canonical;
			if(type->isVariant()) {
				auto& cpat = (ast::ConPattern&)pat;
				auto con = scope.findConstructor(cpat.constructor);
				if(type == con->parentType) {
					if(con->contents.size() == ast::count(cpat.patterns)) {
						// Check if this is the correct constructor.
						conds << IfCond(nullptr, createCompare(scope, *createGetCon(pivot), *createInt(con->index)));
						if(!con->contents.size()) return;

						auto fieldData = createField(pivot, con->index);
						if(con->contents.size() == 1) {
							resolvePattern(scope, *fieldData, *cpat.patterns->item, conds);
						} else {
							// Retrieve the constructor data.
							// We save this in an unnamed variable, because otherwise
							// the code generator will copy these instructions for each pattern.
							auto fieldVar = build<Variable>(0, fieldData->type, scope, true);
							scope.variables << fieldVar;
							auto init = build<AssignExpr>(*fieldVar, *fieldData);
							auto data = build<VarExpr>(fieldVar, fieldVar->type);
							conds << IfCond(init, nullptr);
							auto conpat = cpat.patterns;
							U32 i = 0;
							while(conpat) {
								auto d = createField(*data, i);
								resolvePattern(scope, *d, *conpat->item, conds);
								conpat = conpat->next;
								i++;
							}
						}
					} else {
						error("invalid number of patterns for this constructor");
						conds << IfCond(nullptr, createFalse());
					}
				} else {
					error("incompatible type in pattern");
					conds << IfCond(nullptr, createFalse());
				}
			} else {
				assert("Not implemented" == 0);
			}
			break;
		}
		default:
			assert(false);
	}
}

Expr* Resolver::resolveCondition(ScopeRef scope, ast::ExprRef expr) {
	return implicitCoerce(*resolveExpression(scope, expr, true), types.getBool());
}

ast::InfixExpr* Resolver::reorder(ast::InfixExpr& expr, U32 min_prec) {
	auto lhs = &expr;
	while(lhs->rhs->isInfix() && !lhs->ordered) {
		auto first = context.findOp(lhs->op);
		if(first.precedence < min_prec) break;

		auto rhs = (ast::InfixExpr*)lhs->rhs;
		auto second = context.findOp(rhs->op);
		if(second.precedence > first.precedence ||
			  (second.precedence == first.precedence && second.associativity == ast::Assoc::Right)) {
			lhs->rhs = reorder(*rhs, second.precedence);
			if(lhs->rhs == rhs) {
				lhs->ordered = true;
				break;
			}
		} else {
			lhs->ordered = true;
			lhs->rhs = rhs->lhs;
			rhs->lhs = lhs;
			lhs = rhs;
		}
	}
	return lhs;
}

bool Resolver::alwaysTrue(ExprRef expr) {
	// TODO: Perform constant folding.
	auto e = &expr;
	if(expr.kind == Expr::Multi) e = *((MultiExpr*)e)->es.back();

	return e->kind == Expr::Lit
		   && ((LitExpr*)e)->literal.type == Literal::Bool
		   && ((LitExpr*)e)->literal.i == 1;
}

}} // namespace athena::resolve
