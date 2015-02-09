#include "resolve.h"

namespace athena {
namespace resolve {

Resolver::Resolver(ast::CompileContext& context, ast::Module& source) :
	context(context), source(source), buffer(4*1024*1024) {}

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
		if(decl->kind == ast::Decl::Function) {
			// Create a linked list of functions with the same name.
			auto name = ((ast::FunDecl*)decl)->name;
			Function** f;
			if(!module->functions.AddGet(name, f)) *f = nullptr;
			auto fun = *f;
			*f = build<Function>(name, (ast::FunDecl*)decl);
			(*f)->sibling = fun;
		} else {
			// Type names have to be unique - give an error and ignore any repeated definitions.
			Id name;
			if(decl->kind == ast::Decl::Type) {
				name = ((ast::TypeDecl*)decl)->name;
			} else {
				ASSERT(decl->kind == ast::Decl::Data);
				name = ((ast::DataDecl*)decl)->name;
			}
			
			Type** type;
			if(module->types.AddGet(name, type)) {
				// This type was already declared in this scope.
				// Ignore the type that was defined last.
				error("redefinition of '%@'", context.Find(name).name);
			} else {
				// Insert the unresolved type.
				if(decl->kind == ast::Decl::Type) {
					*type = build<AliasType>(name, (ast::TypeDecl*)decl);
				} else {
					*type = build<AggType>(name, (ast::DataDecl*)decl);
				}
			}
		}
	}

    // Perform the resolve pass. All defined names in this scope are now available.
	// Symbols may be resolved lazily when used by other symbols,
	// so we just skip those that are already defined.
	module->types.Iterate([=](Id name, Type* t) {
		if(t->kind == Type::Alias) {
            auto a = (AliasType*)t;
            if(a->astDecl) {
                resolveAlias(*module, a);
            }
        } else if(t->kind == Type::Agg) {
            auto a = (AggType*)t;
            if(a->astDecl) {
                resolveAggregate(*module, a);
            }
        }
	});

    module->functions.Iterate([=](Id name, Function* f) {
        if(f->astDecl) resolveFunction(*f, *f->astDecl);
    });
	
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
	
Variable* Resolver::resolveArgument(ScopeRef scope, ast::Arg& arg) {
	auto type = arg.type ? resolveType(scope, arg.type) : types.getUnknown();
	return build<Variable>(arg.name, type, scope, arg.constant);
}

Field Resolver::resolveField(ScopeRef scope, ast::Field& field) {
	ASSERT(field.type || field.content);
	TypeRef type = nullptr;
	Expr* content = nullptr;
	
	if(field.type)
		type = resolveType(scope, field.type);
	if(field.content)
		content = resolveExpression(scope, field.content);
	
	// TODO: Typecheck here if both are set.
	return {field.name, type, *content, field.constant};
}

PrimitiveOp* Resolver::tryPrimitiveOp(ast::ExprRef callee) {
	if(callee->isVar()) {
		return primitiveMap.Get(((const ast::VarExpr*)callee)->name);
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
