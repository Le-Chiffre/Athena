#include <CoreMath.h>
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
			FunctionDecl** f;
			if(!module->functions.AddGet(name, f)) *f = nullptr;
			auto fun = *f;
			*f = build<Function>(name, (ast::FunDecl*)decl);
			(*f)->sibling = fun;
		} else if(decl->kind == ast::Decl::Foreign) {
			auto fdecl = (ast::ForeignDecl*)decl;
			if(fdecl->type->kind == ast::Type::Fun) {
				auto name = fdecl->importedName;
				FunctionDecl **f;
				if (!module->functions.AddGet(name, f)) *f = nullptr;
				auto fun = *f;
				*f = build<ForeignFunction>(fdecl);
				(*f)->sibling = fun;
			} else {
				error("cannot handle foreign variable imports yet.");
			}
		} else {
			// Type names have to be unique - give an error and ignore any repeated definitions.
			Id name;
			if(decl->kind == ast::Decl::Type) {
				name = ((ast::TypeDecl*)decl)->name;
			} else {
				ASSERT(decl->kind == ast::Decl::Data);
				name = ((ast::DataDecl*)decl)->type->name;
			}
			
			TypeRef* type;
			if(module->types.AddGet(name, type)) {
				// This type was already declared in this scope.
				// Ignore the type that was defined last.
				error("redefinition of '%@'", context.Find(name).name);
			} else {
				// Insert the unresolved type.
				if(decl->kind == ast::Decl::Type) {
					*type = build<AliasType>(name, (ast::TypeDecl*)decl);
				} else if(decl->kind == ast::Decl::Data) {
					auto t = build<VarType>(name, (ast::DataDecl*)decl);

					// The constructors can be declared here, but are resolved later.
					auto con = ((ast::DataDecl*)decl)->constrs;
					bool isEnum = true;
					uint index = 0;
					while(con) {
						if(con->item->types) isEnum = false;
						VarConstructor* constr;
						if(module->constructors.AddGet(con->item->name, constr)) {
							// This constructor was already declared in this scope.
							// Ignore the constructor that was defined last.
							error("redefinition of type constructor '%@'", context.Find(name).name);
						} else {
							new (constr) VarConstructor{con->item->name, index, t, con->item->types};
							t->list += constr;
						}
						con = con->next;
						index++;
					}
					t->isEnum = isEnum;
					t->selectorBits = t->list.Count() ? Core::Math::FindLastBit(t->list.Count() - 1) + 1 : 0;

					ASSERT(t->list.Count() >= 1);
					*type = t;
				} else {
					DebugError("Not implemented");
				}
			}
		}
	}

    // Perform the resolve pass. All defined names in this scope are now available.
	// Symbols may be resolved lazily when used by other symbols,
	// so we just skip those that are already defined.
	module->types.Iterate([=](Id name, TypeRef& t) {
		if(t->kind == Type::Alias) {
			// Alias types are completely replaced by their contents, since they are equivalent.
            auto a = (AliasType*)t;
            if(a->astDecl) {
                t = resolveAlias(*module, a);
            }
        } else if(t->kind == Type::Var) {
            auto a = (VarType*)t;

			// This is valid, since variants have at least one constructor.
            if(a->astDecl) {
                resolveVariant(*module, a);
            }
        }
	});

    module->functions.Iterate([=](Id name, FunctionDecl* f) {
        resolveFunctionDecl(*module, *f);
    });
	
	return module;
}

Field Resolver::resolveField(ScopeRef scope, TypeRef container, uint index, ast::Field& field) {
	ASSERT(field.type || field.content);
	TypeRef type = nullptr;
	Expr* content = nullptr;
	
	if(field.type)
		type = resolveType(scope, field.type);
	if(field.content)
		content = resolveExpression(scope, field.content);
	
	// TODO: Typecheck here if both are set.
	return {field.name, index, type, container, content, field.constant};
}

PrimitiveOp* Resolver::tryPrimitiveBinaryOp(Id callee) {
	return primitiveBinaryMap.Get(callee);
}

PrimitiveOp* Resolver::tryPrimitiveUnaryOp(Id callee) {
	return primitiveUnaryMap.Get(callee);
}

Expr* Resolver::implicitLoad(ExprRef target) {
	if(target.type->isPointer()) {
		return build<LoadExpr>(target, ((PtrType*)target.type)->type);
	} else {
		return (Expr*)&target;
	}
}

Expr* Resolver::implicitCoerce(ExprRef src, TypeRef dst) {
	if(src.type == dst) return (Expr*)&src;

	if(src.isLiteral()) {
		return literalCoerce(((LitExpr&)src).literal, dst);
	}
	
	if(typeCheck.implicitCoerce(src.type, dst, Nothing)) {
		// Lvalue to Rvalue conversion is so common that we implement it as a special instruction.
		// This also allows for a simpler code generator.
		if(src.type->isLvalue())
			return implicitCoerce(*getRV(src), dst);
		else
			return build<CoerceExpr>(src, dst);
	}

	return nullptr;
}

LitExpr* Resolver::literalCoerce(const ast::Literal& lit, TypeRef dst) {
	Literal literal;
	typeCheck.literalCoerce(lit, dst, literal, Nothing);

	// We always return a valid value to simplify the resolver.
	// If an error occurred the code generator will not be invoked.
	return build<LitExpr>(literal, dst);
}

Expr* Resolver::createRet(ExprRef e) {
	return build<RetExpr>(*getRV(e));
}

Expr* Resolver::createTrue() {
	Literal lit;
	lit.i = 1;
	lit.type = Literal::Bool;
	return build<LitExpr>(lit, types.getBool());
}

Expr* Resolver::createCompare(Scope& scope, ExprRef left, ExprRef right) {
	return resolveBinaryCall(scope, context.AddUnqualifiedName("=="), left, right);
}

Expr* Resolver::getRV(ExprRef e) {
	if(e.type->isLvalue()) {
		return build<CoerceLVExpr>(e, types.getRV(e.type));
	} else return (Expr*)&e;
}

nullptr_t Resolver::error(const char* text) {
	Core::LogError(text);
	return nullptr;
}

}} // namespace athena::resolve
