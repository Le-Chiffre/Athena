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
    enum Type {
        Float,
        Int,
        Char,
        String
    };

    union {
        double f;
        uint64 i;
        wchar32 c;
        Id s;
    };

    Type type;
};

struct Expr {
	enum Type {
		Multi,
		Lit,
		Var,
		App,
		Infix,
		Prefix,
        If,
		Decl,
		While,
		Assign
	} type;

	Expr(Type t) : type(t) {}
};

typedef const Expr& ExprRef;
typedef ASTList<Expr*> ExprList;

struct MultiExpr : Expr {
	MultiExpr(ExprList* exprs) : Expr(Multi), exprs(exprs) {}
	ExprList* exprs;
};

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

struct DeclExpr : Expr {
	DeclExpr(Id name, ExprRef content, bool constant) : Expr(Decl), name(name), content(content), constant(constant) {}
	Id name;
	ExprRef content;
	bool constant;
};

struct WhileExpr : Expr {
	WhileExpr(ExprRef cond, ExprRef loop) : Expr(While), cond(cond), loop(loop) {}
	ExprRef cond;
	ExprRef loop;
};

struct AssignExpr : Expr {
	AssignExpr(ExprRef target, ExprRef value) : Expr(Assign), target(target), value(value) {}
	ExprRef target;
	ExprRef value;
};

struct Decl {
	enum Type {
		Function
	} type;

	Decl(Type t) : type(t) {}
};

typedef const Decl& DeclRef;
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

typedef const Module& ModuleRef;

struct CompileContext;

String toString(ExprRef e, CompileContext& c);
String toString(DeclRef e, CompileContext& c);
String toString(ModuleRef m, CompileContext& c);

}} // namespace athena::ast

#endif // Athena_Parser_ast_h