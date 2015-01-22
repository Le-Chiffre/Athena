#ifndef Athena_Parser_resolve__ast_h
#define Athena_Parser_resolve__ast_h

#include "ast.h"

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
typedef const Type& TypeRef;

typedef Core::Array<Variable*> VarList;
typedef Core::Array<Function*> FunList;
typedef Core::Array<Type*> TypeList;
typedef ast::ASTList<Scope*> ScopeList;
typedef ast::ASTList<Alt*> AltList;

struct Scope {
    Variable* findVar(Id name);
    Function* findFun(Id name);

	// The function containing this scope, or null if it is a function or global scope.
	Function* function;

	// Any expressions this scope contains.
	Expr* expression;

	// The parent scope, or null if it is a global scope.
	Scope* parent;

	// Children of this scope.
	ScopeList* children;

	// The variables that were declared in this scope.
	VarList variables;

    // The functions that were declared in this scope.
    FunList functions;
};

struct Function : Scope {
    Function(Id name) : name(name) {}

	// The mangled name of this function.
	Id name;

	// The arguments this function takes.
	// Each argument also exists in the list of variables.
	VarList arguments;
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

enum class PrimitiveType {
	I64,
	I32,
	I16,
	I8,
	U64,
	U32,
	U16,
	U8,

	FirstFloat,
	F64 = (uint)FirstFloat,
	F32,
	F16,

	TypeCount
};

struct Type {
	enum Kind {
		Unknown,
		Unit,
		Prim,
		Agg,
		Var,
		Array,
		Map
	} kind;

	Type(Kind kind) : kind(kind) {}
};

struct PrimType : Type {
	PrimType(PrimitiveType type) : Type(Prim), type(type) {}
	PrimitiveType type;
};

struct AggType : Type {
	struct Element {
		Element(Id name, TypeRef type) : name(name), type(type) {}
		Id name;
		TypeRef type;
	};

	typedef Core::Array<Element> ElementList;

	AggType() : Type(Agg) {}
	ElementList elements;
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

enum class PrimitiveOp {
	// Binary
	Add,
	Sub,
	Mul,
	Div,
	Rem,
	Shl,
	Shr,
	And,
	Or,
	Xor,
	CmpEq,
	CmpNeq,
	CmpGt,
	CmpGeq,
	CmpLt,
	CmpLe,

	// Unary
	FirstUnary,
	Neg = (uint)FirstUnary,
	Not,
	
	// Must be last
	OpCount
};

inline bool isUnary(PrimitiveOp op) {
	return op >= PrimitiveOp::FirstUnary;
}

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
		Decl,
		While,
		Assign
	} kind;

	TypeRef type;
	Expr(Kind k, TypeRef type = Type::Unknown) : kind(k), type(type) {}
};

typedef const Expr& ExprRef;
typedef ast::ASTList<Expr*> ExprList;

struct LitExpr : Expr {
	LitExpr(Literal lit, TypeRef type = Type::Unknown) : Expr(Lit, type), literal(lit) {}
	Literal literal;
};

struct VarExpr : Expr {
	VarExpr(Variable* var, TypeRef type = Type::Unknown) : Expr(Var, type), var(var) {}
	Variable* var;
};

struct AppExpr : Expr {
	AppExpr(FuncRef n, ExprList* args = nullptr, TypeRef type = Type::Unknown) : Expr(App, type), callee(n), args(args) {}
	FuncRef callee;
	ExprList* args;
};

struct AppIExpr : Expr {
	AppIExpr(const char* i, ExprList* args = nullptr, TypeRef type = Type::Unknown) : Expr(AppI, type), callee(i), args(args) {}
	const char* callee;
	ExprList* args;
};

struct AppPExpr : Expr {
	AppPExpr(PrimitiveOp op, ExprList* args, TypeRef type = Type::Unknown) : Expr(AppP, type), args(args), op(op) {}
	ExprList* args;
	PrimitiveOp op;
};

struct CaseExpr : Expr {
    CaseExpr(AltList* alts, Expr* otherwise, TypeRef type = Type::Unknown) : Expr(Case, type), alts(alts), otherwise(otherwise) {}
    AltList* alts;
    Expr* otherwise;
};

struct DeclExpr : Expr {
	DeclExpr(Variable& var, ExprRef val) : Expr(Decl, var.type), var(var), val(val) {}
	Variable& var;
	ExprRef val;
};

struct WhileExpr : Expr {
	WhileExpr(ExprRef cond, ExprRef loop) : Expr(While, Type::Unit), cond(cond), loop(loop) {}
	ExprRef cond;
	ExprRef loop;
};

struct AssignExpr : Expr {
	AssignExpr(Variable& var, ExprRef val) : Expr(Assign, var.type), var(var), val(val) {}
	Variable& var;
	ExprRef val;
};

struct Module {
	Id name;
	Core::Array<Function*> functions{32};
};

}} // namespace athena::resolve

#endif // Athena_Parser_resolve__ast_h