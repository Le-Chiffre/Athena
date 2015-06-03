
#include "mangle.h"

namespace athena {
namespace resolve {

Core::String Mangler::mangle(Function* function) {
	string.Clear();
	string += "_Z";

	auto name = context.Find(function->name);
	mangleQualifier(&name);
	if(function->scope.parent && function->scope.parent->function) {
		string += '$';
		name = context.Find(function->scope.parent->function->name);
		mangleQualifier(&name);
	}

	for(auto a : function->arguments) {
		mangleType(a->type);
	}
	return string;
}

Id Mangler::mangleId(Function* function) {
	return context.AddUnqualifiedName(mangle(function));
}

void Mangler::mangleQualifier(ast::Qualified* qualified) {
	string += "N";
	auto name = qualified->name;
	auto q = qualified->qualifier;
	while(q) {
		char buffer[32];
		Core::NumberToString((uint)q->name.length, buffer, 32);
		string += &buffer[0];
		string.Append(q->name.ptr, (uint)q->name.length);
		q = q->qualifier;
	}

	char buffer[32];
	Core::NumberToString((uint)name.length, buffer, 32);
	string += &buffer[0];
	string.Append(name.ptr, (uint)name.length);
	string += 'E';
}

void Mangler::mangleType(TypeRef type) {
	if(type->isUnit())
		string += 'v';
	else if(type->isPrimitive())
		mangleType(((PrimType*)type)->type);
	else if(type->isPointer())
		mangleType((PtrType*)type);
	else if(type->isVariant())
		mangleType((VarType*)type);
}

void Mangler::mangleType(PrimitiveType type) {
	static const char types[] = {
			'x', 'i', 's', 'c',
			'y', 'j', 't', 'h',
			'd', 'f', 't', 'b'
	};
	string += types[(uint)type];
}

void Mangler::mangleType(const PtrType* type) {
	string += 'P';
	mangleType(type->type);
}

void Mangler::mangleType(const VarType* type) {
	auto name = context.Find(type->name);
	mangleQualifier(&name);
}

}} // namespace athena::resolve
