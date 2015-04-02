#ifndef Athena_Resolve_mangle_h
#define Athena_Resolve_mangle_h

#include "../Parse/parser.h"
#include "resolve_ast.h"

namespace athena {
namespace resolve {

struct Mangler {
	Mangler(ast::CompileContext& context) : context(context) {}

	/// Mangles the name of the provided function.
	Core::String mangle(Function* function);

	/// Mangles a function name to a name id.
	Id mangleId(Function* function);

	/// Mangles a qualified name.
	void mangleQualifier(ast::Qualified* qualified);

	/// Mangles a type name.
	void mangleType(TypeRef type);
	void mangleType(PrimitiveType type);
	void mangleType(const PtrType* type);
	void mangleType(const AggType* type);

private:
	ast::CompileContext& context;
	Core::String string;
};

}} // namespace athena::resolve

#endif // Athena_Resolve_mangle_h
