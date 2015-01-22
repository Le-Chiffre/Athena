#include <msxml.h>
#include "resolve.h"

namespace athena {
namespace resolve {

Module* Resolver::resolve() {
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
        if(decl->type == ast::Decl::Function) {
            module->functions += build<Function>(((ast::FunDecl*)decl)->name);
        }
    }

    // Perform the resolve pass.
    uint i = 0;
	for(auto decl : source.declarations) {
		if(decl->type == ast::Decl::Function) {
			resolveFunction(*module->functions[i], *(ast::FunDecl*)decl);
            i++;
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
	switch(expr.type) {
		case ast::Expr::Infix:
			return resolveInfix(scope, (const ast::InfixExpr&)expr);
		case ast::Expr::Prefix:
			return resolvePrefix(scope, (const ast::PrefixExpr&)expr);
		case ast::Expr::App:
			return resolveCall(scope, (const ast::AppExpr&)expr);
        case ast::Expr::Var:
            // This can be either a variable or a function call without parameters.
            return resolveVar(scope, ((const ast::VarExpr&)expr).name);
        case ast::Expr::If:
            return resolveIf(scope, (const ast::IfExpr&)expr);
		case ast::Expr::Decl:
			return resolveDecl(scope, (const ast::DeclExpr&)expr);
		case ast::Expr::Assign:
			return resolveAssign(scope, (const ast::AssignExpr&)expr);
		case ast::Expr::While:
			return resolveWhile(scope, (const ast::WhileExpr&)expr);
		default:
			FatalError("Unsupported expression type.");
	}
	
	return nullptr;
}
	
Expr* Resolver::resolveInfix(Scope& scope, const ast::InfixExpr& expr) {
	// Check if this can be a primitive operator.
	for(uint i = 0; i < (uint)PrimitiveOp::FirstUnary; i++) {
		if(expr.op == primitiveOps[i]) {
			return resolvePrimitiveOp(scope, (PrimitiveOp)i, expr);
		}
	}
	
	// Otherwise, create a normal function call.
	ast::ExprList l2{&expr.rhs};
	ast::ExprList l1{&expr.lhs, &l2};
	ast::VarExpr v{expr.op};
	return resolveCall(scope, {v, &l1});
}

Expr* Resolver::resolvePrefix(Scope& scope, const ast::PrefixExpr& expr) {
	// Check if this can be a primitive operator.
	for(auto i = (uint)PrimitiveOp::FirstUnary; i < (uint)PrimitiveOp::OpCount; i++) {
		if(expr.op == primitiveOps[i]) {
			return resolvePrimitiveOp(scope, (PrimitiveOp)i, expr);
		}
	}
	
	// Otherwise, create a normal function call.
	ast::ExprList l1{&expr.dst};
	ast::VarExpr v{expr.op};
	return resolveCall(scope, {v, &l1});
}
	
Expr* Resolver::resolveCall(Scope& scope, const ast::AppExpr& expr) {
    // Resolve the name of the function being called.
    if(expr.callee.type == ast::Expr::Var) {
        // Find the function being called, or give an error.
        if(auto fun = scope.findFun(((const ast::VarExpr&)expr.callee).name)) {
            // Resolve each argument.
            ExprList* args = nullptr;
            if(expr.args) {
                auto arg = expr.args;
                args = build<ExprList>(resolveExpression(scope, *arg->item));
                auto a = args;
                arg = arg->next;
                while(arg) {
                    a->next = build<ExprList>(resolveExpression(scope, *arg->item));
                    a = a->next;
                    arg = arg->next;
                }
            }
            return build<AppExpr>(*fun);
        } else {
            // No applicable function was found.
            return nullptr;
        }
    } else {
        // This isn't a callable type.
        return nullptr;
    }
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
            return build<AppExpr>(*fun);
        } else {
            // No variable or function was found; we are out of luck.
            return nullptr;
        }
    }
}

Expr* Resolver::resolveIf(Scope& scope, const ast::IfExpr& expr) {
    auto alt = build<Alt>(resolveExpression(scope, expr.cond), resolveExpression(scope, expr.then));
    auto otherwise = expr.otherwise ? resolveExpression(scope, *expr.otherwise) : nullptr;
	// If-expressions without an else part can fail and never return a value.
    return build<CaseExpr>(build<AltList>(alt), otherwise, otherwise ? Type::Unknown : Type::Unit);
}

Expr* Resolver::resolveDecl(Scope& scope, const ast::DeclExpr& expr) {
	auto content = resolveExpression(scope, expr.content);

	// Make sure this scope doesn't already have a variable with the same name.
	Variable* var;
	if((var = scope.findVar(expr.name))) {
		error("redefinition of '%@'", var->name);
	} else {
		// Create the variable allocation.
		var = build<Variable>(expr.name, content->type, scope, expr.constant);
		scope.variables += var;
	}

	// Create the assignment.
	return build<AssignExpr>(*var, *content);
}

Expr* Resolver::resolveAssign(Scope& scope, const ast::AssignExpr& expr) {
	// Make sure the type can be assigned to.
	auto target = resolveExpression(scope, expr.target);
	if(target->kind == Expr::Var) {
		auto value = resolveExpression(scope, expr.value);
		if(typeCheck.compatible(*target, *value)) {
			return build<AssignExpr>(*((VarExpr*)target)->var, *value);
		} else {
			error("assigning to '' from incompatible type ''");
			//TODO: Implement Type printing.
			//error("assigning to '%@' from incompatible type '%@'", target->type, value->type);
		}
	} else {
		error("expression is not assignable");
	}
}

Expr* Resolver::resolveWhile(Scope& scope, const ast::WhileExpr& expr) {
	return nullptr;
}

Expr* Resolver::resolvePrimitiveOp(Scope& scope, PrimitiveOp op, const ast::InfixExpr& expr) {
	auto list = build<ExprList>(resolveExpression(scope, expr.lhs), build<ExprList>(resolveExpression(scope, expr.rhs)));
	return build<AppPExpr>(op, list);
}
	
Expr* Resolver::resolvePrimitiveOp(Scope& scope, PrimitiveOp op, const ast::PrefixExpr& expr) {
	auto list = build<ExprList>(resolveExpression(scope, expr.dst));
	return build<AppPExpr>(op, list);
}
	
Variable* Resolver::resolveArgument(ScopeRef scope, Id arg) {
	return build<Variable>(arg, Type::Unknown, scope, true);
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
