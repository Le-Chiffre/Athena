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

typedef Scope& ScopeRef;
typedef Function& FuncRef;

typedef Core::Array<Variable*> VarList;
typedef Core::Array<Function*> FunList;
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
	Variable(Id name, ScopeRef scope) : name(name), scope(scope) {}

	Id name;
	ScopeRef scope;
};

struct Alt {
    Alt(Expr* c, Expr* r) : cond(c), result(r) {}
    Expr* cond;
    Expr* result;
};

struct Expr {
	enum Type {
		Lit,
		Var,
		App,
		AppI,
		AppP,
        Case
	} type;

	Expr(Type t) : type(t) {}
};

typedef const Expr& ExprRef;
typedef ast::ASTList<Expr*> ExprList;

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

struct LitExpr : Expr {
	LitExpr(Literal lit) : Expr(Lit), literal(lit) {}
	Literal literal;
};

struct VarExpr : Expr {
	VarExpr(Variable* var) : Expr(Var), var(var) {}
	Variable* var;
};

struct AppExpr : Expr {
	AppExpr(FuncRef n, ExprList* args = nullptr) : Expr(App), callee(n), args(args) {}
	FuncRef callee;
	ExprList* args;
};

struct AppIExpr : Expr {
	AppIExpr(const char* i, ExprList* args = nullptr) : Expr(AppI), callee(i), args(args) {}
	const char* callee;
	ExprList* args;
};

struct AppPExpr : Expr {
	AppPExpr(PrimitiveOp op, ExprList* args) : Expr(AppP), args(args), op(op) {}
	ExprList* args;
	PrimitiveOp op;
};

struct CaseExpr : Expr {
    CaseExpr(AltList* alts, Expr* otherwise) : Expr(Case), alts(alts), otherwise(otherwise) {}
    AltList* alts;
    Expr* otherwise;
};

struct Module {
	Id name;
	Core::Array<Function*> functions{32};
};

}} // namespace athena::resolve

#endif // Athena_Parser_resolve__ast_h