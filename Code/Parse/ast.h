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
	T item;

	ASTList() {}
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
	T* item;

	ASTList(T* i) : item(i) {}
	ASTList(T* i, ASTList<T*>* n) : next(n), item(i) {}

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

struct Type {
	enum Kind {
		Unit, // The empty unit type.
		Con,  // A type constructor for a named type.
		Ptr,  // A pointer to a type.
        Gen,  // A generic or polymorphic named type.
        Tup,  // A tuple type with optionally named fields.
	} kind;
	Id con = 0;

	Type(Kind k) : kind(k) {}
	Type(Kind k, Id con) : kind(k), con(con) {}
};

typedef const Type* TypeRef;

struct TupleField {
    TupleField(TypeRef type, Core::Maybe<Id> name, struct Expr* def) : type(type), defaultValue(def), name(name) {}

    TypeRef type;
    struct Expr* defaultValue;
    Core::Maybe<Id> name;
};

typedef ASTList<TupleField> TupleFieldList;

struct TupleType : Type {
    TupleType(TupleFieldList* fields) : Type(Tup), fields(fields) {}
    TupleFieldList* fields;
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
		Assign,
		Nested,
		Coerce,
		Field,
		Construct,
		Format
	} type;

	Expr(Type t) : type(t) {}
	bool isVar() const {return type == Var;}
	bool isInfix() const {return type == Infix;}
	bool isLiteral() const {return type == Lit;}
	bool isCall() const {return type == App;}
	bool isDecl() const {return type == Decl;}
};

typedef Expr* ExprRef;
typedef ASTList<Expr*> ExprList;

struct MultiExpr : Expr {
	MultiExpr(ExprList* exprs) : Expr(Multi), exprs(exprs) {}
	ExprList* exprs;
};

// This is used to represent parenthesized expressions.
// We need to keep all ordering information for the reordering pass later.
struct NestedExpr : Expr {
	NestedExpr(ExprRef expr) : Expr(Nested), expr(expr) {}
	ExprRef expr;
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
    IfExpr(ExprRef cond, ExprRef then, Expr* otherwise) : Expr(If), cond(cond), then(then), otherwise(otherwise) {}
    ExprRef cond;
    ExprRef then;
    Expr* otherwise;
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

struct CoerceExpr : Expr {
	CoerceExpr(ExprRef target, TypeRef kind) : Expr(Coerce), target(target), kind(kind) {}
	ExprRef target;
	TypeRef kind;
};

struct FieldExpr : Expr {
	FieldExpr(ExprRef target, ExprRef field) : Expr(Field), target(target), field(field) {}
	ExprRef target; // Either a var, literal or a complex expression.
	ExprRef field;  // Field to apply to.
};

struct ConstructExpr : Expr {
	ConstructExpr(Id name) : Expr(Construct), name(name) {}
	Id name;
};

/// Formatted strings are divided into chunks.
/// Each chunk consists of a string part and an expression to format and insert after it.
/// The expression may be null if this chunk is the first one in a literal.
struct FormatChunk {
	Id string;
	Expr* format;
};

typedef ASTList<FormatChunk> FormatList;

struct FormatExpr : Expr {
	FormatExpr(const FormatList& format) : Expr(Format), format(format) {}
	FormatList format;
};

struct Decl {
	enum Kind {
		Function,
		Type,
		Data
	} kind;

	Decl(Kind t) : kind(t) {}
};

typedef const Decl& DeclRef;

struct Arg {
	Id name;
	TypeRef type;
	bool constant;
};

typedef ASTList<Arg> ArgList;

struct FunDecl : Decl {
	FunDecl(Id name, ExprRef body, TupleType* args, TypeRef ret) : Decl(Function), name(name), args(args), ret(ret), body(body) {}
	Id name;
	TupleType* args;
	TypeRef ret; // If the function explicitly defines one.
	ExprRef body;
};

struct TypeDecl : Decl {
	TypeDecl(Id name, TypeRef target) : Decl(Type), name(name), target(target) {}
	Id name;
	TypeRef target;
};

struct Field {
	Field(Id name, TypeRef type, ExprRef content, bool constant) : name(name), type(type), content(content), constant(constant) {}

	Id name;

	// One of these must be set.
	TypeRef type;
	ExprRef content;

	bool constant;
};

typedef ASTList<const Field*> FieldList;

struct DataDecl : Decl {
	DataDecl(Id name, FieldList* fields) : Decl(Data), fields(fields), name(name) {}
	FieldList* fields;
	Id name;
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