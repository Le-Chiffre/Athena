#ifndef Athena_Resolve_mangle_h
#define Athena_Resolve_mangle_h

#include <sstream>
#include "../Parse/parser.h"
#include "resolve_ast.h"

namespace athena {
namespace resolve {

struct Mangler {
	Mangler(ast::CompileContext& context) : context(context) {}

	/// Mangles the name of the provided function.
	std::string mangle(Function* function);

	/// Mangles a function name to a name id.
	Id mangleId(Function* function);

	/// Mangles a qualified name.
	void mangleQualifier(ast::Qualified* qualified);

	/// Mangles a type name.
	void mangleType(Type* type);
	void mangleType(PrimitiveType type);
	void mangleType(const PtrType* type);
	void mangleType(const VarType* type);

private:
	ast::CompileContext& context;
	std::ostringstream string;
};

}} // namespace athena::resolve

#endif // Athena_Resolve_mangle_h
