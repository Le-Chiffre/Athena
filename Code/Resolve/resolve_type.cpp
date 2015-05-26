
#include "resolve.h"

namespace athena {
namespace resolve {

TypeRef Resolver::resolveAlias(Scope& scope, AliasType* type) {
	ASSERT(type->astDecl);
	auto target = resolveTypeDef(scope, *type->astDecl->type, type->astDecl->target);
	type->astDecl = nullptr;
	return target;
}

TypeRef Resolver::resolveVariant(Scope& scope, VarType* type) {
	// Resolve each declared constructor.
	for(auto& c : type->list) {
		ast::walk(c->astDecl, [&](auto i) {
			c->contents += this->resolveTypeDef(scope, *type->astDecl->type, i);
		});
		c->astDecl = nullptr;
	}
	type->astDecl = nullptr;
	return type;
}

TypeRef Resolver::resolveTuple(Scope& scope, ast::SimpleType& tscope, ast::TupleType& type) {
	// Generate a hash for the fields.
	Core::Hasher h;
	ast::walk(type.fields, [&](auto i) {
		auto t = this->resolveTypeDef(scope, tscope, i.type);
		h.Add(t);
		// Include the name to ensure that different tuples with the same memory layout are not exactly the same.
		if(i.name) h.Add(i.name());
	});

	// Check if this kind of tuple has been used already.
	TupleType* result = nullptr;
	if(!types.getTuple(h, result)) {
		// Otherwise, create the type.
		new (result) TupleType;
		uint i = 0;
		bool resolved = true;
		ast::walk(type.fields, [&](auto it) {
			auto t = this->resolveTypeDef(scope, tscope, it.type);
			if(!t->resolved) resolved = false;
			result->fields += Field{it.name ? it.name() : 0, i, t, result, nullptr, true};
			i++;
		});

		result->resolved = resolved;
	}

	return result;
}

inline Maybe<uint> getGenIndex(ast::SimpleType& type, Id name) {
	auto t = type.kind;
	uint i = 0;
	while(t) {
		if(t->item == name) return i;
		i++;
		t = t->next;
	}

	return Nothing;
}

TypeRef Resolver::resolveType(ScopeRef scope, ast::SimpleType& tscope, ast::TypeRef type, bool constructor) {
	// Check if this is a primitive type.
	if(type->kind == ast::Type::Unit) {
		return types.getUnit();
	} else if(type->kind == ast::Type::Ptr) {
		ast::Type t{ast::Type::Con, type->con};
		return types.getPtr(resolveType(scope, tscope, &t));
	} else if(type->kind == ast::Type::Tup) {
		return resolveTuple(scope, tscope, *(ast::TupleType*)type);
	} else if(type->kind == ast::Type::Gen) {
		auto i = getGenIndex(tscope, type->con);
		if(i) {
			auto g = build<GenType>(i());
			g->resolved = false;
			return g;
		} else {
			error("undefined generic type");
			return types.getUnknown();
		}
	} else if(type->kind == ast::Type::App) {
		// Find the base type and instantiate it for these arguments.
		auto base = resolveType(scope, tscope, ((ast::AppType*)type)->base, constructor);
		if(base->isGeneric()) {
			auto t = build<AppType>(((GenType*)base)->index);
			auto a = ((ast::AppType*)type)->apps;
			while(a) {
				t->apps += resolveType(scope, tscope, type, constructor);
				a = a->next;
			}
		} else {
			return instantiateType(scope, base, ((ast::AppType*)type)->apps);
		}
	} else {
		// Check if this type has been defined in this scope.
		if(constructor) {
			if (auto t = scope.findConstructor(type->con))
				return t->type;

			// TODO: This is kind of a hack.
			// The Bool primitive type has separate constructors.
			auto name = context.Find(type->con).name;
			if(name == "True" || name == "False") {
				return types.getBool();
			} else if(name == "Bool") {
				// The Bool name is not a constructor.
				error("'Bool' cannot be used as a constructor; use True or False instead");
			} else {
				// Check if this is a primitive type.
				if (auto t = types.primMap.Get(type->con)) return *t;
			}
		} else {
			if (auto t = scope.findType(type->con)) return t;
			// Check if this is a primitive type.
			if (auto t = types.primMap.Get(type->con)) return *t;
		}

		return types.getUnknown();
	}
}

TypeRef Resolver::resolveTypeDef(ScopeRef scope, ast::SimpleType& tscope, ast::TypeRef type) {
	// Check if this is a primitive type.
	if(type->kind == ast::Type::Unit) {
		return types.getUnit();
	} else if(type->kind == ast::Type::Ptr) {
		ast::Type t{ast::Type::Con, type->con};
		return types.getPtr(resolveTypeDef(scope, tscope, &t));
	} else if(type->kind == ast::Type::Tup) {
		return resolveTuple(scope, tscope, *(ast::TupleType*)type);
	} else if(type->kind == ast::Type::Gen) {
		auto i = getGenIndex(tscope, type->con);
		if(i) {
			auto g = build<GenType>(i());
			g->resolved = false;
			return g;
		} else {
			error("undefined generic type");
			return types.getUnknown();
		}
	} else if(type->kind == ast::Type::App) {
		// Find the base type and instantiate it for these arguments.
		auto base = resolveTypeDef(scope, tscope, ((ast::AppType*)type)->base);
		if(base->isGeneric()) {
			auto t = build<AppType>(((GenType*)base)->index);
			auto a = ((ast::AppType*)type)->apps;
			while(a) {
				t->apps += resolveTypeDef(scope, tscope, type);
				a = a->next;
			}
		} else {
			return instantiateType(scope, base, ((ast::AppType*)type)->apps);
		}
	} else {
		if (auto t = scope.findType(type->con)) return t;
		// Check if this is a primitive type.
		if (auto t = types.primMap.Get(type->con)) return *t;

		return types.getUnknown();
	}
}

TypeRef Resolver::instantiateType(ScopeRef scope, TypeRef base, ast::TypeList* apps) {
	bool resolved = true;
	while(apps) {

	}
}

}} // namespace athena::resolve
