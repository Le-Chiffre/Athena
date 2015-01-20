#ifndef Athena_Parser_ast_h
#define Athena_Parser_ast_h

#include <core.h>
#include <Array.h>
#include <Map.h>

namespace athena {
namespace ast {

typedef Core::StringRef String;
typedef uint Id;

template<class T>
struct ASTList
{
	ASTList<T>* next = nullptr;
	const T item;

	ASTList(const T& i) : item(i) {}
	ASTList(const T& i, ASTList<T>* n) : item(i), next(n) {}

	struct It {
		const ASTList<T>* curr;
		It operator ++ () {curr = curr->next; return *this;}
		bool hasNext() const {return curr->next != nullptr;}
		const T& operator * () const {return curr->item;}
		bool operator != (const It& i) const {return curr != i.curr;}
	};

	It begin() const {return {this};}
	It end() const {return {nullptr};}
};

template<class T>
struct ASTList<T*>
{
	ASTList<T*>* next = nullptr;
	const T* item;

	ASTList(const T* i) : item(i) {}
	ASTList(const T* i, ASTList<T*>* n) : next(n), item(i) {}

	struct It {
		const ASTList<T*>* curr;
		It operator ++ () {curr = curr->next; return *this;}
		bool hasNext() const {return curr->next != nullptr;}
		const T& operator * () const {return *curr->item;}
		bool operator != (const It& i) const {return curr != i.curr;}
	};

	It begin() const {return {this};}
	It end() const {return {nullptr};}

	uint_ptr count() {
		if(next) return 1 + next->count();
		else return 1;
	}
};

struct Fixity {
	enum Kind : uint8 {
		Left, Right, Prefix
	};

	Kind kind;
	uint8 prec;
};

struct Literal {
	double d;
};

struct Expr {
	enum Type {
		Lit,
		Var,
		App,
		Infix,
		Prefix,
        If
	} type;

	Expr(Type t) : type(t) {}
};

typedef const Expr& ExprRef;
typedef ASTList<Expr*> ExprList;

struct LitExpr : Expr {
	LitExpr(Literal lit) : Expr(Lit), literal(lit) {}
	Literal literal;
};

struct VarExpr : Expr {
	VarExpr(Id n) : Expr(Var), name(n) {}
	Id name;
};

struct AppExpr : Expr {
	AppExpr(ExprRef n, ExprList* args = nullptr) : Expr(App), callee(n), args(args) {}
	ExprRef callee;
	ExprList* args;
};

struct InfixExpr : Expr {
	InfixExpr(Id op, ExprRef lhs, ExprRef rhs) : Expr(Infix), lhs(lhs), rhs(rhs), op(op) {}
	ExprRef lhs, rhs;
	Id op;
};

struct PrefixExpr : Expr {
	PrefixExpr(Id op, ExprRef dst) : Expr(Prefix), dst(dst), op(op) {}
	ExprRef dst;
	Id op;
};

struct IfExpr : Expr {
    IfExpr(ExprRef cond, ExprRef then, const Expr* otherwise) : Expr(If), cond(cond), then(then), otherwise(otherwise) {}
    ExprRef cond;
    ExprRef then;
    const Expr* otherwise;
};



struct Decl {
	enum Type {
		Function
	} type;

	Decl(Type t) : type(t) {}
};

typedef ASTList<Id> ArgList;

struct FunDecl : Decl {
	FunDecl(Id name, ExprRef body, ArgList* args = nullptr) : Decl(Function), name(name), args(args), body(body) {}
	Id name;
	ArgList* args;
	ExprRef body;
};



struct Module {
	Id name;
	Core::Array<Decl*> declarations{32};
	Core::NumberMap<Fixity, Id> operators{16};
};

}} // namespace athena::ast

#endif // Athena_Parser_ast_h