
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
			if(!module->functions.addGet(name, f)) *f = nullptr;
			auto fun = *f;
			*f = build<Function>(name, (ast::FunDecl*)decl);
			(*f)->sibling = fun;
		} else if(decl->kind == ast::Decl::Foreign) {
			auto fdecl = (ast::ForeignDecl*)decl;
			if(fdecl->type->kind == ast::Type::Fun) {
				auto name = fdecl->importedName;
				FunctionDecl **f;
				if (!module->functions.addGet(name, f)) *f = nullptr;
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
				name = ((ast::TypeDecl*)decl)->type->name;
			} else {
				assert(decl->kind == ast::Decl::Data);
				name = ((ast::DataDecl*)decl)->type->name;
			}

			Type** type;
			if(module->types.addGet(name, type)) {
				// This type was already declared in this scope.
				// Ignore the type that was defined last.
				error("redefinition of '%@'", context.find(name).name);
			} else {
				// Insert the unresolved type.
				if(decl->kind == ast::Decl::Type) {
					*type = build<AliasType>(name, (ast::TypeDecl*)decl, *module);
				} else if(decl->kind == ast::Decl::Data) {
					auto t = build<VarType>(name, (ast::DataDecl*)decl, *module);

					// The constructors can be declared here, but are resolved later.
					auto con = ((ast::DataDecl*)decl)->constrs;
					bool isEnum = true;
					U32 index = 0;
					while(con) {
						if(con->item->types) isEnum = false;
						VarConstructor* constr;
						if(module->constructors.addGet(con->item->name, constr)) {
							// This constructor was already declared in this scope.
							// Ignore the constructor that was defined last.
							error("redefinition of type constructor '%@'", context.find(name).name);
						} else {
							new (constr) VarConstructor{con->item->name, index, t, con->item->types};
							t->list << constr;
						}
						con = con->next;
						index++;
					}
					t->isEnum = isEnum;
					t->selectorBits = t->list.size() ? findLastBit(t->list.size() - 1) + 1 : 0;

					assert(t->list.size() >= 1);
					*type = t;
				} else {
					assert("Not implemented" == 0);
				}
			}
		}
	}

    // Perform the resolve pass. All defined names in this scope are now available.
	// Symbols may be resolved lazily when used by other symbols,
	// so we just skip those that are already defined.
    modify([=](Id name, Type*& t) {
		if(t->kind == Type::Alias) {
			// Alias types are completely replaced by their contents, since they are equivalent.
            auto a = (AliasType*)t;
            if(a->astDecl) t = resolveAlias(a);
        } else if(t->kind == Type::Var) {
			// This is valid, since variants have at least one constructor.
			auto a = (VarType*)t;
            if(a->astDecl) resolveVariant(a);
        }
	}, module->types);

    walk([=](Id name, FunctionDecl* f) {
        resolveFunctionDecl(*module, *f);
    }, module->functions);

	return module;
}

Field Resolver::resolveField(ScopeRef scope, Type* container, U32 index, ast::Field& field) {
	assert(field.type || field.content);
	Type* type = nullptr;
	Expr* content = nullptr;

	if(field.type)
		type = resolveType(scope, field.type);
	if(field.content)
		content = resolveExpression(scope, field.content, true);

	// TODO: Typecheck here if both are set.
	return {field.name, index, type, container, content, field.constant};
}

PrimitiveOp* Resolver::tryPrimitiveBinaryOp(Id callee) {
	return primitiveBinaryMap.get(callee).get();
}

PrimitiveOp* Resolver::tryPrimitiveUnaryOp(Id callee) {
	return primitiveUnaryMap.get(callee).get();
}

Expr* Resolver::implicitLoad(ExprRef target) {
	if(target.type->isPointer()) {
		return build<LoadExpr>(target, ((PtrType*)target.type)->type);
	} else {
		return &target;
	}
}

Expr* Resolver::implicitCoerce(ExprRef src, Type* dst) {
	if(src.type == dst) return &src;

	if(auto e = findLiteral(src)) {
		literalCoerce(e, dst);
		updateLiteral(src, dst);
		return &src;
	}

	if(typeCheck.implicitCoerce(src.type, dst, Nothing())) {
		// Lvalue to Rvalue conversion is so common that we implement it as a special instruction.
		// This also allows for a simpler code generator.
		if(src.type->isLvalue())
			return implicitCoerce(*getRV(src), dst);
		else
			return build<CoerceExpr>(src, dst);
	}

	return nullptr;
}

LitExpr* Resolver::literalCoerce(const ast::Literal& lit, Type* dst) {
	Literal literal;
	typeCheck.literalCoerce(lit, dst, literal, Nothing());

	// We always return a valid value to simplify the resolver.
	// If an error occurred the code generator will not be invoked.
	return build<LitExpr>(literal, dst);
}

LitExpr* Resolver::literalCoerce(LitExpr* lit, Type* dst) {
	if(typeCheck.literalCoerce(lit->literal, dst, lit->literal, Nothing()))
		lit->type = dst;
	return lit;
}

Expr* Resolver::getRV(ExprRef e) {
	if(e.type->isLvalue()) {
		return build<CoerceLVExpr>(e, types.getRV(e.type));
	} else return &e;
}

void* Resolver::error(const char* text) {
	return nullptr;
}

}} // namespace athena::resolve
