
#include "resolve.h"

namespace athena {
namespace resolve {

TypeRef Resolver::resolveAlias(Scope& scope, AliasType* type) {
	ASSERT(type->astDecl);
	auto target = resolveType(scope, type->astDecl->target);
	type->astDecl = nullptr;
	return target;
}

TypeRef Resolver::resolveVariant(Scope& scope, VarType* type) {
	// Resolve each declared constructor.
	for(auto& c : type->list) {
		auto t = c.astDecl;
		while(t) {
			c.contents += resolveType(scope, t->item);
			t = t->next;
		}
		c.astDecl = nullptr;
	}
	type->astDecl = nullptr;
	return type;
}

TypeRef Resolver::resolveTuple(Scope& scope, ast::TupleType& type) {
	// Generate a hash for the fields.
	Core::Hasher h;
	auto f = type.fields;
	while(f) {
		auto t = resolveType(scope, f->item.type);
		h.Add(t);
		// Include the name to ensure that tuples with the same memory layout are not exactly the same.
		if(f->item.name) h.Add(f->item.name());
		f = f->next;
	}

	// Check if this kind of tuple has been used already.
	TupleType* result = nullptr;
	if(!types.getTuple(h, result)) {
		// Otherwise, create the type.
		new (result) TupleType;
		uint i = 0;
		f = type.fields;
		while(f) {
			auto t = resolveType(scope, f->item.type);
			result->fields += Field{f->item.name ? f->item.name() : 0, i, t, result, nullptr, true};
			f = f->next;
			i++;
		}
	}

	return result;
}
	
TypeRef Resolver::resolveType(ScopeRef scope, ast::TypeRef type, bool constructor) {
	// Check if this is a primitive type.
	if(type->kind == ast::Type::Unit) {
		return types.getUnit();
	} else if(type->kind == ast::Type::Ptr) {
		ast::Type t{ast::Type::Con, type->con};
		return types.getPtr(resolveType(scope, &t));
	} else if(type->kind == ast::Type::Tup) {
		return resolveTuple(scope, *(ast::TupleType*)type);
	} else {
		// Check if this type has been defined in this scope.
		if(constructor) {
			if (auto t = scope.findConstructor(type->con)) return t->type;

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

}} // namespace athena::resolve
