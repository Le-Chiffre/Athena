#ifndef Athena_Resolve_resolve__ast_h
#define Athena_Resolve_resolve__ast_h

#include "../Parse/ast.h"

namespace athena {
namespace resolve {

using ast::Fixity;
using ast::Literal;
using ast::Id;

struct Variable;
struct Scope;
struct Function;
struct FunctionDecl;
struct Expr;
struct Alt;
struct Type;
struct Resolver;
struct VarConstructor;

typedef Scope& ScopeRef;
typedef FunctionDecl& FuncRef;
typedef Type* TypeRef;
typedef const Expr& ExprRef;

typedef ast::ASTList<const Expr*> ExprList;
typedef Core::Array<Variable*> VarList;
typedef Core::Array<Function*> FunList;
typedef Core::Array<Type*> TypeList;
typedef ast::ASTList<Scope*> ScopeList;
typedef ast::ASTList<Alt*> AltList;
typedef Core::NumberMap<TypeRef, Id> TypeMap;
typedef Core::NumberMap<VarConstructor, Id> ConMap;
typedef Core::NumberMap<FunctionDecl*, Id> FunMap;
typedef ast::ForeignConvention ForeignConvention;

struct VarConstructor {
	VarConstructor(Id name, uint index, TypeRef type, ast::TypeList* astDecl) : name(name), index(index), type(type), astDecl(astDecl) {}

	Id name;
	uint index;
	TypeRef type;
	ast::TypeList* astDecl;
	TypeList contents;
	void* codegen = nullptr;
};

struct Scope {
	/// Finds any variable with the provided name that is visible in this scope.
    Variable* findVar(Id name);

	/// Finds any local variable with the provided name that is declared within this scope.
	Variable* findLocalVar(Id name);

    TypeRef findType(Id name);
	VarConstructor* findConstructor(Id name);

	// The base name of this scope (determines type visibility).
	Id name;

	// The parent scope, or null if it is a global scope.
	Scope* parent;

	// Children of this scope.
	ScopeList* children;

	// The variables that were declared in this scope.
	VarList variables;

	// The function parameters that were shadowed in this scope.
	VarList shadows;

    // The functions that were declared in this scope.
	FunMap functions;

    // The types that were declared in this scope.
    TypeMap types;

	// The constructor names that were declared in this scope.
	ConMap constructors;
};

typedef Scope Module;

struct FunctionDecl {
	FunctionDecl(Id name, bool foreign, bool impl) : name(name), isForeign(foreign), hasImpl(impl) {}

	// This pointer can be used in any way by the code generator.
	void* codegen = nullptr;

	// The base name of this function.
	Id name;

	// If set, this is a ForeignFunction.
	bool isForeign = false;

	// If set, this is a Function.
	bool hasImpl = false;

	/*
	 * The following fields are invalid as long as the function has not been resolved.
	 */

	// The arguments this function takes.
	// Each argument also exists in the function scope as a variable.
	VarList arguments;

	// The return type of this function.
	// Note that this is resolved lazily in most cases.
	TypeRef type = nullptr;

	// The next function overload with this name.
	FunctionDecl* sibling = nullptr;
};

struct Function : FunctionDecl {
	Function(Id name, ast::FunDecl* decl) : FunctionDecl(name, false, true), astDecl(decl) {}

	// The scope this function contains.
	Scope scope;

	// The source declaration of this function in the AST.
	// This will be set as long as the function has not been resolved.
	// Any function where this is set after resolving is either unused or an error.
	ast::FunDecl* astDecl = nullptr;

	/*
	 * The following fields are invalid as long as the function has not been resolved.
	 */
	
	// The mangled name of this function.
	Id mangledName = 0;

	// Any expressions this function consists of.
	Expr* expression = nullptr;
};

struct ForeignFunction : FunctionDecl {
	ForeignFunction(ast::ForeignDecl* decl) :
		FunctionDecl(decl->importedName, true, false), astType((ast::FunType*)decl->type), importName(decl->importName), cconv(decl->cconv) {}

	ast::FunType* astType;
	Id importName;
	ForeignConvention cconv;
};

struct Variable {
	Variable(Id name, TypeRef type, ScopeRef scope, bool constant, bool funParam = false) : type(type), scope(scope), name(name), constant(constant), funParam(funParam) {}
	void* codegen = nullptr; // Opaque pointer that can be used by the code generator.
	TypeRef type;
	ScopeRef scope;
	Id name;
	bool constant;
	bool funParam;
	
	bool isVar() const {return !constant && !funParam;}
};

struct Alt {
    Alt(Expr* c, Expr* r) : cond(c), result(r) {}
    Expr* cond;
    Expr* result;
};

/// Supported primitive types. Operations on these types map directly to instructions.
/// To support efficient code generation, they are divided in categories.
/// Types within the same category (except Other) are often compatible with each other.
/// Within a category they are ordered large to small.
enum class PrimitiveType {
	/*
	 * Make sure to update PrimitiveTypeCategory and category() when changing this!
	 */
    FirstSigned,
	I64 = (uint)FirstSigned,
	I32,
	I16,
	I8,
	
	FirstUnsigned,
	U64 = (uint)FirstUnsigned,
	U32,
	U16,
	U8,

	FirstFloat,
	F64 = (uint)FirstFloat,
	F32,
	F16,

    FirstOther,
    Bool = (uint)FirstOther,

	TypeCount
};

/// Categories for the primitive types.
/// Used to quickly determine supported operations.
enum class PrimitiveTypeCategory {
    Signed = 0,
    Unsigned = 0b100,
    Float = 0b1000,
    Other = 0b1100
};

/// Returns the category a primitive type belongs to.
inline PrimitiveTypeCategory category(PrimitiveType t) {
	if(t < PrimitiveType::FirstUnsigned) return PrimitiveTypeCategory::Signed;
	if(t < PrimitiveType::FirstFloat) return PrimitiveTypeCategory::Unsigned;
	if(t < PrimitiveType::FirstOther) return PrimitiveTypeCategory::Float;
	return PrimitiveTypeCategory::Other;
}

/// Returns the largest of the two provided types.
inline PrimitiveType largest(PrimitiveType a, PrimitiveType b) {
	return Core::Min(a, b);
}

struct Type {
	void* codegen = nullptr; // Opaque pointer that can be used by the code generator.
	
	enum Kind {
		Unknown,
        Alias,
		Unit,
		Tuple,
		Var,
		Array,
		Map,
		Lvalue,

		// These are often checked for together, so we let them share a single bit.
		Prim = 0x10,
		Ptr = 0x11,

		// When adding new types, make sure no other types have bit 4 set.
	} kind;

	bool isPointer() const {return kind == Ptr;}
    bool isPrimitive() const {return kind == Prim;}
	bool isPtrOrPrim() const {return ((uint)kind & 0x10) != 0;}
	bool isTuple() const {return kind == Tuple;}
	bool isTupleOrIndirect() const;
	bool isKnown() const {return kind != Unknown;}
	bool isUnit() const {return kind == Unit;}
    bool isAlias() const {return kind == Alias;}
	bool isBool() const;
	bool isLvalue() const {return kind == Lvalue;}
	bool isVariant() const {return kind == Var;}

	Type(Kind kind) : kind(kind) {}
};

/// A primitive type, where a primitive is a "native" type that represents a raw number in some form.
/// Note that pointers are separate types, because they contain additional information about what they point to.
struct PrimType : Type {
	PrimType(PrimitiveType type) : Type(Prim), type(type) {}
	PrimitiveType type;
};

inline bool Type::isBool() const {return isPrimitive() && ((const PrimType*)this)->type == PrimitiveType::Bool;}

/// A pointer type representing a memory address to some data type.
/// Pointers must have a concrete type; pointers to the unit type are not supported.
struct PtrType : Type {
	PtrType(TypeRef type) : Type(Ptr), type(type) {}
	TypeRef type;
};

/// Represents an lvalue (as opposed to rvalue).
struct LVType : Type {
	LVType(TypeRef type) : Type(Lvalue), type(type) {}
	TypeRef type;
};
	
inline bool Type::isTupleOrIndirect() const {
	return isTuple()
		|| (kind == Ptr && ((PtrType*)this)->type->isTuple())
		|| (kind == Lvalue && ((LVType*)this)->type->isTuple());
}

struct Field {
	Field(Id name, uint index, TypeRef type, TypeRef container, Expr* content, bool constant) :
		name(name), index(index), type(type), container(container), content(content), constant(constant) {}

	Id name;
	uint index;
	TypeRef type;
	TypeRef container;
	Expr* content;
	bool constant;
};

typedef Core::Array<Field> FieldList;

struct TupleType : Type {
	TupleType() : Type(Tuple) {}
	FieldList fields;

	Field* findField(Id name) {
		for(auto& i : fields) {
			if(i.name == name) return &i;
		}
		return nullptr;
	}
};

typedef Core::Array<VarConstructor*> VarConstructorList;

struct VarType : Type {
	VarType(Id name, ast::DataDecl* astDecl) : Type(Var), name(name), astDecl(astDecl) {}
	Id name;
	ast::DataDecl* astDecl;
	VarConstructorList list;
	uint8 selectorBits = 0;
	bool isEnum = false;
};

struct ArrayType : Type {
	ArrayType(TypeRef type) : Type(Array), type(type) {}
	TypeRef type;
};

struct MapType : Type {
	MapType(TypeRef from, TypeRef to) : Type(Map), from(from), to(to) {}
	TypeRef from;
	TypeRef to;
};

// This type is just a placeholder. It is put in place of alias declarations while type names are still being resolved.
// When all names exist, the aliases are replaced with their target types.
struct AliasType : Type {
    AliasType(Id name, ast::TypeDecl* astDecl) : Type(Alias), name(name), astDecl(astDecl) {}
	Id name;
	ast::TypeDecl* astDecl;
};

/// Operations that can be applied to primitive types.
/// These should map directly to instructions on most hardware.
/// They are divided into categories for easier code generation.
/// Not each primitive type supports each operation.
enum class PrimitiveOp {
	// Binary
	Add,
	Sub,
	Mul,
	Div,
	Rem,

    FirstBit,
	Shl = (uint)FirstBit,
	Shr,
	And,
	Or,
	Xor,

    FirstCompare,
	CmpEq = (uint)FirstCompare,
	CmpNeq,
	CmpGt,
	CmpGe,
	CmpLt,
	CmpLe,

	// Unary
	FirstUnary,
	Neg = (uint)FirstUnary,
	Not,
	Ref,
	Deref,
	
	// Must be last
	OpCount
};

/// Returns true if this operation takes exactly one parameter.
inline bool isUnary(PrimitiveOp op) {
	return op >= PrimitiveOp::FirstUnary;
}

/// Returns true if this operation takes exactly two parameters.
inline bool isBinary(PrimitiveOp op) {
	return op < PrimitiveOp::FirstUnary;
}

/// Returns true if this operation is logical and/or.
inline bool isAndOr(PrimitiveOp op) {
	return op == PrimitiveOp::And || op == PrimitiveOp::Or;
}

struct Expr {
	void* codegen = nullptr; // Opaque pointer that can be used by the code generator.
	
	enum Kind {
		Multi,
		Lit,
		Var,
		Load,
		Store,
		App,
		AppI,
		AppP,
		Case,
		If,
		While,
		Assign, // constant variables.
		Coerce,
		CoerceLV, // common special case of Coerce.
		Field,
		Ret,
		Construct,

		// The following expressions are only used while resolving or for error handling.
		// They are removed before the resolve stage finishes.
		FirstTemporary,
		EmptyDecl = FirstTemporary,
		Empty
	} kind;

	bool isTemp() const {return kind < FirstTemporary;}

	TypeRef type;
	Expr(Kind k, TypeRef type) : kind(k), type(type) {}
};
	
struct MultiExpr : Expr {
	MultiExpr(Expr* expr) : Expr(Multi, expr->type), expr(expr) {}
	Expr* expr;
	MultiExpr* next = nullptr;
};

struct LitExpr : Expr {
	LitExpr(Literal lit, TypeRef type) : Expr(Lit, type), literal(lit) {}
	Literal literal;
};

struct VarExpr : Expr {
	VarExpr(Variable* var, TypeRef type) : Expr(Var, type), var(var) {}
	Variable* var;
};

struct AppExpr : Expr {
	AppExpr(FuncRef n, ExprList* args) : Expr(App, n.type), callee(n), args(args) {}
	FuncRef callee;
	ExprList* args;
};

struct AppIExpr : Expr {
	AppIExpr(const char* i, ExprList* args, TypeRef type) : Expr(AppI, type), callee(i), args(args) {}
	const char* callee;
	ExprList* args;
};

struct AppPExpr : Expr {
	AppPExpr(PrimitiveOp op, ExprList* args, TypeRef type) : Expr(AppP, type), args(args), op(op) {}
	ExprList* args;
	PrimitiveOp op;
};

struct CaseExpr : Expr {
    CaseExpr(AltList* alts, Expr* otherwise, TypeRef type) : Expr(Case, type), alts(alts), otherwise(otherwise) {}
    AltList* alts;
    Expr* otherwise;
};

struct IfExpr : Expr {
	IfExpr(ExprRef cond, ExprRef then, const Expr* otherwise, TypeRef type, bool ret) : Expr(If, type), cond(cond), then(then), otherwise(otherwise), returnResult(ret) {}
	ExprRef cond;
	ExprRef then;
	const Expr* otherwise;
	bool returnResult;
};

struct WhileExpr : Expr {
	WhileExpr(ExprRef cond, ExprRef loop, TypeRef type) : Expr(While, type), cond(cond), loop(loop) {}
	ExprRef cond;
	ExprRef loop;
};

struct LoadExpr : Expr {
	LoadExpr(ExprRef target, TypeRef type) : Expr(Load, type), target(target) {}
	ExprRef target;
};

struct StoreExpr : Expr {
	StoreExpr(ExprRef target, ExprRef value, TypeRef type) : Expr(Store, type), target(target), value(value) {}
	ExprRef target;
	ExprRef value;
};

struct AssignExpr : Expr {
	AssignExpr(Variable& target, ExprRef value) : Expr(Assign, target.type), target(target), value(value) {}
	Variable& target;
	ExprRef value;
};

struct CoerceExpr : Expr {
	CoerceExpr(ExprRef src, TypeRef dst) : Expr(Coerce, dst), src(src) {}
	ExprRef src;
};

struct CoerceLVExpr : Expr {
	CoerceLVExpr(ExprRef src, TypeRef dst) : Expr(CoerceLV, dst), src(src) {}
	ExprRef src;
};

struct FieldExpr : Expr {
	FieldExpr(ExprRef container, ::athena::resolve::Field* field, TypeRef type) : Expr(Field, type), container(container), field(field) {}
	ExprRef container;
	::athena::resolve::Field* field;
};
	
struct RetExpr : Expr {
	RetExpr(ExprRef e) : Expr(Ret, e.type), expr(e) {}
	ExprRef expr;
};

struct ConstructArg {
	uint index;
	ExprRef expr;
};

struct ConstructExpr : Expr {
	ConstructExpr(TypeRef type, VarConstructor* con = nullptr) : Expr(Construct, type), con(con) {}
	Core::Array<ConstructArg> args;

	// Only set if the type is a variant.
	VarConstructor* con;
};

/// A temporary expression that represents an unfinished declaration.
struct EmptyDeclExpr : Expr {
	EmptyDeclExpr(Variable& var) : Expr(EmptyDecl, var.type), var(var) {}
	Variable& var;
};

/// A temporary expression that represents a non-existent value, such as a compilation error.
struct EmptyExpr : Expr {
	EmptyExpr(TypeRef type) : Expr(Empty, type) {}
};

}} // namespace athena::resolve

#endif // Athena_Resolve_resolve__ast_h