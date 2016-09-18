#ifndef Athena_Parser_ast_h
#define Athena_Parser_ast_h

#include "../General/types.h"
#include "../General/maybe.h"
#include "../General/array.h"
#include "../General/map.h"
#include <string>

namespace athena {
namespace ast {

template<class T>
struct ASTList {
	ASTList<T>* next = nullptr;
	T item;

	ASTList() {}
	ASTList(const T& i) : item(i) {}
	ASTList(const T& i, ASTList<T>* n) : item(i), next(n) {}
};

template<class T>
struct ASTList<T*> {
	ASTList<T*>* next = nullptr;
	T* item;

	ASTList(T* i) : item(i) {}
	ASTList(T* i, ASTList<T*>* n) : next(n), item(i) {}
};

template<class T>
inline auto getListElem(T* a) { return a; }

template<class T, class F>
void walk(ASTList<T>* l, F&& f) {
	while(l) {
		f(l->item);
		l = l->next;
	}
}

template<class T, class U, class F>
U fold(ASTList<T>* l, U start, F&& f) {
	while(l) {
		start = f(l->item, start);
		l = l->next;
	}
	return start;
}

template<class T>
U32 count(const ASTList<T>* l) {
	if(!l) return 0;
	else return 1 + count(l->next);
}

enum class ForeignConvention {
	CCall,
	Stdcall,
	Cpp,
	JS
};

struct Fixity {
	enum Kind : Byte {
		Left, Right, Prefix
	};

	Kind kind;
	Byte prec;
};

struct Literal {
    enum Type {
        Float,
        Int,
        Char,
        String,
		Bool
    };

    union {
        double f;
        U64 i;
        WChar32 c;
        Id s;
    };

    Type type;
};

inline Literal trueLit() {
	Literal l;
	l.type = Literal::Bool;
	l.i = 1;
	return l;
}

struct SimpleType {
	SimpleType(Id name, ASTList<Id*>* kind) : name(name), kind(kind) {}
	Id name;
	ASTList<Id*>* kind;
};

struct Type {
	enum Kind {
		Unit, // The empty unit type.
		Con,  // A type name for a named type.
		Ptr,  // A pointer to a type.
        Gen,  // A generic or polymorphic named type.
        Tup,  // A tuple type with optionally named fields.
		Fun,  // A function type.
		App   // Application of higher-kinded type.
	} kind;
	Id con = 0;

	Type(Kind k) : kind(k) {}
	Type(Kind k, Id con) : kind(k), con(con) {}
};

typedef const Type* TypeRef;

struct TupleField {
    TupleField(TypeRef type, Maybe<Id> name, struct Expr* def) : type(type), defaultValue(def), name(name) {}

    TypeRef type;
    struct Expr* defaultValue;
    Maybe<Id> name;
};

typedef ASTList<TupleField*> TupleFieldList;
typedef ASTList<Type*> TypeList;

struct TupleType : Type {
    TupleType(TupleFieldList* fields) : Type(Tup), fields(fields) {}
    TupleFieldList* fields;
};

struct FunType : Type {
	FunType(TypeList* types) : Type(Fun), types(types) {}
	TypeList* types;
};

struct AppType : Type {
	AppType(TypeRef base, TypeList* apps) : Type(App), base(base), apps(apps) {}
	TypeRef base;
	TypeList* apps;
};

struct Expr {
	enum Type {
		Unit,
		Multi,
		Lit,
		Var,
		App,
		Lam,
		Infix,
		Prefix,
        If,
		MultiIf,
		Decl,
		While,
		Assign,
		Nested,
		Coerce,
		Field,
		Construct,
		TupleConstruct,
		Format,
		Case
	} type;

	Expr(Type t) : type(t) {}
	bool isVar() const {return type == Var;}
	bool isInfix() const {return type == Infix;}
	bool isLiteral() const {return type == Lit;}
	bool isCall() const {return type == App;}
	bool isDecl() const {return type == Decl;}
	bool isField() const {return type == Field;}
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

struct LamExpr : Expr {
	LamExpr(TupleType* args, bool isCase, ExprRef body) : Expr(Lam), args(args), body(body), isCase(isCase) {}
	TupleType* args;
	ExprRef body;
	bool isCase;
};

struct InfixExpr : Expr {
	InfixExpr(Id op, ExprRef lhs, ExprRef rhs) : Expr(Infix), lhs(lhs), rhs(rhs), op(op) {}
	ExprRef lhs, rhs;
	Id op;
	bool ordered = false;
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

struct IfCase {
	IfCase(ExprRef cond, ExprRef then) : cond(cond), then(then) {}
	ExprRef cond;
	ExprRef then;
};

typedef ASTList<IfCase*> IfCaseList;

struct MultiIfExpr : Expr {
	MultiIfExpr(IfCaseList* cases) : Expr(MultiIf), cases(cases) {}
	IfCaseList* cases;
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

struct TupleConstructExpr : Expr {
	TupleConstructExpr(TupleFieldList* args) : Expr(TupleConstruct), args(args) {}
	TupleFieldList* args;
};

struct ConstructExpr : Expr {
	ConstructExpr(TypeRef type, ExprList* args) : Expr(Construct), type(type), args(args) {}
	TypeRef type;
	ExprList* args;
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



struct Pattern {
	enum Kind {
		Var,
		Lit,
		Any,
		Tup,
		Con
	};

	Id asVar;
	Kind kind;

	Pattern(Kind k, Id asVar = 0) : asVar(asVar), kind(k) {}
};

typedef ASTList<Pattern*> PatList;

struct VarPattern : Pattern {
	VarPattern(Id var, Id asVar = 0) : Pattern(Var, asVar), var(var) {}
	Id var;
};

struct LitPattern : Pattern {
	LitPattern(Literal lit, Id asVar = 0) : Pattern(Lit, asVar), lit(lit) {}
	Literal lit;
};

struct FieldPat {
	FieldPat(Maybe<Id> field, Pattern* pat) : field(field), pat(pat) {}
	Maybe<Id> field;
	Pattern* pat;
};

typedef ASTList<FieldPat*> FieldPatList;

struct TupPattern : Pattern {
	TupPattern(FieldPatList* fields, Id asVar = 0) : Pattern(Tup, asVar), fields(fields) {}
	FieldPatList* fields;
};

struct ConPattern : Pattern {
	ConPattern(Id constructor, PatList* patterns) : Pattern(Con), constructor(constructor), patterns(patterns) {}
	Id constructor;
	PatList* patterns;
};


struct Alt {
	Pattern* pattern;
	ExprRef expr;
};

typedef ASTList<Alt*> AltList;

struct CaseExpr : Expr {
	CaseExpr(ExprRef pivot, AltList* alts) : Expr(Case), pivot(pivot), alts(alts) {}
	ExprRef pivot;
	AltList* alts;
};


struct Decl {
	enum Kind {
		Function,
		Type,
		Data,
		Foreign
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

struct FunCase {
	FunCase(PatList* patterns, ExprRef body) : patterns(patterns), body(body) {}
	PatList* patterns;
	ExprRef body;
};

typedef ASTList<FunCase*> FunCaseList;

struct FunDecl;
typedef ASTList<FunDecl*> FunDeclList;

struct FunDecl : Decl {
	FunDecl(Id name, ExprRef body, TupleType* args, TypeRef ret) :
			Decl(Function), name(name), args(args), ret(ret), body(body), cases(nullptr) {}
	FunDecl(Id name, FunCaseList* cases, TupleType* args, TypeRef ret) :
			Decl(Function), name(name), args(args), ret(ret), body(nullptr), cases(cases) {}

	Id name;
	TupleType* args;
	TypeRef ret; // If the function explicitly defines one.
	FunDeclList* locals = nullptr;

	// One of these is set.
	ExprRef body;
	FunCaseList* cases;
};

struct TypeDecl : Decl {
	TypeDecl(SimpleType* type, TypeRef target) : Decl(Type), type(type), target(target) {}
	SimpleType* type;
	TypeRef target;
};

struct ForeignDecl : Decl {
	ForeignDecl(Id importName, Id importedName, ::athena::ast::Type* type, ForeignConvention cconv) :
		Decl(Foreign), importName(importName), importedName(importedName), type(type), cconv(cconv) {}
	Id importName;
	Id importedName;
	::athena::ast::Type* type;
	ForeignConvention cconv;
};

struct Field {
	Field(Id name, TypeRef type, ExprRef content, bool constant) : name(name), type(type), content(content), constant(constant) {}

	Id name;

	// One of these must be set.
	TypeRef type;
	ExprRef content;

	bool constant;
};

struct Constr {
	Constr(Id name, TypeList* types) : name(name), types(types) {}
	Id name;
	TypeList* types;
};

typedef ASTList<Constr*> ConstrList;

struct DataDecl : Decl {
	DataDecl(SimpleType* type, ConstrList* constrs) : Decl(Data), constrs(constrs), type(type) {}
	ConstrList* constrs;
	SimpleType* type;
};


struct Module {
	Id name;
    Array<Decl*> declarations{32};
    Tritium::Map<Id, Fixity> operators;
};

typedef const Module& ModuleRef;

struct CompileContext;

std::string toString(ExprRef e, CompileContext& c);
std::string toString(DeclRef e, CompileContext& c);
std::string toString(ModuleRef m, CompileContext& c);

}} // namespace athena::ast

#endif // Athena_Parser_ast_h