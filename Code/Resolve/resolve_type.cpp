
#include "resolve.h"

namespace athena {
namespace resolve {

void Resolver::resolveAlias(Scope& scope, AliasType* type) {
	ASSERT(type->astDecl);
	type->type = resolveType(scope, type->astDecl->target);
	type->astDecl = nullptr;
}

void Resolver::resolveAggregate(Scope& scope, AggType* type) {
	ASSERT(type->astDecl);

	// A type may have no fields (this is used a lot in variant types).
	if(type->astDecl->fields) {
		for(auto i : *type->astDecl->fields) {
			type->fields += resolveField(scope, type, i);
		}
	}

	type->astDecl = nullptr;
}

TypeRef Resolver::resolveTuple(Scope& scope, ast::TupleType& type) {
	// Generate a hash for the fields.
	Core::Hasher h;
	auto f = type.fields;
	while(f) {
		auto t = resolveType(scope, f->item.type);
		h.Add(t);
		f = f->next;
	}

	// Check if this kind of tuple has been used already.
	TupleType* result = nullptr;
	if(!types.getTuple(h, result)) {
		// Otherwise, create the type.
		new (result) TupleType;
		f = type.fields;
		while(f) {
			auto t = resolveType(scope, f->item.type);
			result->fields += Field{f->item.name ? f->item.name() : 0, t, result, nullptr, true};
			f = f->next;
		}
	}

	return result;
}

TypeRef Resolver::resolveType(ScopeRef scope, ast::TypeRef type) {
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
		if(auto t = scope.findType(type->con)) {
			return t;
		}

		// Check if this is a primitive type.
		if(auto t = types.primMap.Get(type->con)) {
			return *t;
		} else {
			return types.getUnknown();
		}
	}
}

}} // namespace athena::resolve
