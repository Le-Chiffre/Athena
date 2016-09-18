
#include "mangle.h"

namespace athena {
namespace resolve {

std::string Mangler::mangle(Function* function) {
	string.clear();
	string << "_Z";

	auto name = context.find(function->name);
	mangleQualifier(&name);
	if(function->scope.parent && function->scope.parent->function) {
		string << '$';
		name = context.find(function->scope.parent->function->name);
		mangleQualifier(&name);
	}

	for(auto a : function->arguments) {
		mangleType(a->type);
	}
	return string.str();
}

Id Mangler::mangleId(Function* function) {
	return context.addUnqualifiedName(mangle(function));
}

void Mangler::mangleQualifier(ast::Qualified* qualified) {
	string << "N";
	auto name = qualified->name;
	auto q = qualified->qualifier;
	while(q) {
		string << q->name.length();
        string << q->name;
		q = q->qualifier;
	}

	string << name.length();
    string << name;
	string << 'E';
}

void Mangler::mangleType(Type* type) {
	if(type->isUnit())
		string << 'v';
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
	string << types[(Size)type];
}

void Mangler::mangleType(const PtrType* type) {
	string << 'P';
	mangleType(type->type);
}

void Mangler::mangleType(const VarType* type) {
	auto name = context.find(type->name);
	mangleQualifier(&name);
}

}} // namespace athena::resolve
