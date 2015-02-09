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
struct Expr;
struct Alt;
struct Type;

typedef Scope& ScopeRef;
typedef Function& FuncRef;
typedef const Type* TypeRef;
typedef const Expr& ExprRef;

typedef ast::ASTList<const Expr*> ExprList;
typedef Core::Array<Variable*> VarList;
typedef Core::Array<Function*> FunList;
typedef Core::Array<Type*> TypeList;
typedef ast::ASTList<Scope*> ScopeList;
typedef ast::ASTList<Alt*> AltList;
typedef Core::NumberMap<Type*, Id> TypeMap;
typedef Core::NumberMap<Function*, Id> FunMap;

struct Scope {
    Variable* findVar(Id name);
    Function* findFun(Id name, ExprList* types);
    Type* findType(Id name);

	// The base name of this scope (determines type visibility).
	Id name;

	// The parent scope, or null if it is a global scope.
	Scope* parent;

	// Children of this scope.
	ScopeList* children;

	// The variables that were declared in this scope.
	VarList variables;

    // The functions that were declared in this scope.
	FunMap functions{32};

    // The types that were declared in this scope.
    TypeMap types{32};
};

typedef Scope Module;

struct Function {
	Function(Id name, ast::FunDecl* decl) : name(name), astDecl(decl) {}

	// The base name of this function.
	Id name;
	
	// The source declaration of this function in the AST.
	// This will be set as long as the function has not been resolved.
	// Any function where this is set after resolving is either unused or an error.
	ast::FunDecl* astDecl = nullptr;
	
	/*
	 * The following fields are invalid as long as the function has not been resolved.
	 */
	
	// The mangled name of this function.
	Id mangledName = 0;

	// The arguments this function takes.
	// Each argument also exists in the list of variables.
	VarList arguments;
	
	// The variables that were declared in the function.
	// This also includes the arguments.
	VarList variables;
	
	// Any expressions this function consists of.
	Expr* expression = nullptr;
	
	// The next function overload with this name.
	Function* sibling = nullptr;
};

struct Variable {
	Variable(Id name, TypeRef type, ScopeRef scope, bool constant) : name(name), type(type), scope(scope), constant(constant) {}
	Id name;
	TypeRef type;
	ScopeRef scope;
	bool constant;
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
    FirstSigned = 0x10,
	I64 = (uint)FirstSigned,
	I32,
	I16,
	I8,
	
	FirstUnsigned = 0x20,
	U64 = (uint)FirstUnsigned,
	U32,
	U16,
	U8,

	FirstFloat = 0x40,
	F64 = (uint)FirstFloat,
	F32,
	F16,

    FirstOther = 0x80,
    Bool = (uint)FirstOther,

	TypeCount
};

/// Categories for the primitive types.
/// Used to quickly determine supported operations.
enum class PrimitiveTypeCategory {
    Signed = 0x10,
    Unsigned = 0x20,
    Float = 0x40,
    Other = 0x80
};

/// Returns the category a primitive type belongs to.
inline PrimitiveTypeCategory category(PrimitiveType t) {
    return (PrimitiveTypeCategory)((uint)t & 0xf0);
}

struct Type {
	enum Kind {
		Unknown,
        Alias,
		Unit,
		Agg,
		Var,
		Array,
		Map,

		// These are often checked for together, so we let them share a single bit.
		Prim = 0x10,
		Ptr = 0x11,

		// When adding new types, make sure no other types have bit 4 set.
	} kind;

	bool isPointer() const {return kind == Ptr;}
    bool isPrimitive() const {return kind == Prim;}
	bool isPtrOrPrim() const {return ((uint)kind & 0x10) != 0;}
	bool isKnown() const {return kind != Unknown;}
	bool isUnit() const {return kind == Unit;}
    bool isAlias() const {return kind == Alias;}
	bool isBool() const;

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

struct Field {
	Field(Id name, TypeRef type, ExprRef content, bool constant) : name(name), type(type), content(content), constant(constant) {}

	Id name;
	TypeRef type;
	ExprRef content;
	bool constant;
};

typedef Core::Array<Field> FieldList;

struct AggType : Type {
	AggType(Id name, ast::DataDecl* astDecl) : Type(Agg), name(name), astDecl(astDecl) {}
	Id name;
	FieldList fields;
	ast::DataDecl* astDecl;
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

struct AliasType : Type {
    AliasType(Id name, ast::TypeDecl* astDecl) : Type(Alias), name(name), astDecl(astDecl) {}
	Id name;
    TypeRef type = nullptr;
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

struct Expr {
	enum Kind {
		Lit,
		Var,
		App,
		AppI,
		AppP,
		Case,
		If,
		While,
		Assign,
		Coerce,
		Field,

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

struct LitExpr : Expr {
	LitExpr(Literal lit, TypeRef type) : Expr(Lit, type), literal(lit) {}
	Literal literal;
};

struct VarExpr : Expr {
	VarExpr(Variable* var) : Expr(Var, var->type), var(var) {}
	Variable* var;
};

struct AppExpr : Expr {
	AppExpr(FuncRef n, ExprList* args) : Expr(App, n.expression->type), callee(n), args(args) {}
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

struct AssignExpr : Expr {
	AssignExpr(Variable& var, ExprRef val) : Expr(Assign, var.type), var(var), val(val) {}
	Variable& var;
	ExprRef val;
};

struct CoerceExpr : Expr {
	CoerceExpr(ExprRef src, TypeRef dst) : Expr(Coerce, dst), src(src) {}
	ExprRef src;
};

struct FieldExpr : Expr {
	FieldExpr(AggType* type, ::athena::resolve::Field* field) : Expr(Field, field->type), type(type), field(field) {}
	AggType* type;
	::athena::resolve::Field* field;
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