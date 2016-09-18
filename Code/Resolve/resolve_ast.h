#ifndef Athena_Resolve_resolve__ast_h
#define Athena_Resolve_resolve__ast_h

#include "../Parse/ast.h"
#include "../General/array.h"

namespace athena {
namespace resolve {

using ast::Fixity;
using ast::Literal;

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
typedef Expr& ExprRef;

typedef ast::ASTList<Expr*> ExprList;
typedef Array<Variable*> VarList;
typedef Array<Function*> FunList;
typedef Array<Type*> TypeList;
typedef ast::ASTList<Scope*> ScopeList;
typedef ast::ASTList<Alt*> AltList;
typedef Tritium::Map<Id, Type*> TypeMap;
typedef Tritium::Map<Id, VarConstructor> ConMap;
typedef Tritium::Map<Id, FunctionDecl*> FunMap;
typedef ast::ForeignConvention ForeignConvention;

struct VarConstructor {
	VarConstructor(Id name, U32 index, Type* parentType, ast::TypeList* astDecl) : name(name), index(index), parentType(parentType), astDecl(astDecl) {}
	VarConstructor(const VarConstructor&) = default;

	Id name;
	U32 index;
	Type* parentType;
	ast::TypeList* astDecl;
	TypeList contents;
	Type* dataType = nullptr; // A type that contains everything inside the contents list.
	void* codegen = nullptr;
};

struct Scope {
	/// Finds any variable with the provided name that is visible in this scope.
    Variable* findVar(Id name);

	/// Finds any local variable with the provided name that is declared within this scope.
	Variable* findLocalVar(Id name);

    Type* findType(Id name);
	VarConstructor* findConstructor(Id name);

	bool hasVariables();

	// The base name of this scope (determines type visibility).
	Id name = 0;

	// The parent scope, or null if it is a global scope.
	Scope* parent = nullptr;

	// The function that contains this scope, if any.
	Function* function = nullptr;

	// Children of this scope.
	ScopeList* children = nullptr;

	// The variables that were declared in this scope.
	VarList variables;

	// The function parameters that were shadowed in this scope.
	VarList shadows;

	// The variables from the parent function that are captured here.
	// If this list contains entries, the function is a closure.
	VarList captures;

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
	Type* type = nullptr;

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

	// Any expressions this function consists of.
	Expr* expression = nullptr;

	// The mangled name of this function.
	Id mangledName = 0;

	// This is true as long as the function contains any generic parameters or return type.
	// Generic functions must be instantiated before they can be called normally.
	bool generic = false;
};

struct ForeignFunction : FunctionDecl {
	ForeignFunction(ast::ForeignDecl* decl) :
		FunctionDecl(decl->importedName, true, false), astType((ast::FunType*)decl->type), importName(decl->importName), cconv(decl->cconv) {}

	ast::FunType* astType;
	Id importName;
	ForeignConvention cconv;
};

struct Variable {
	Variable(Id name, Type* type, ScopeRef scope, bool constant, bool funParam = false) : type(type), scope(scope), name(name), constant(constant), funParam(funParam) {}
	void* codegen = nullptr; // Opaque pointer that can be used by the code generator.
	Type* type;
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
	I64 = (::U32)FirstSigned,
	I32,
	I16,
	I8,

	FirstUnsigned,
	U64 = (::U32)FirstUnsigned,
	U32,
	U16,
	U8,

	FirstFloat,
	F64 = (::U32)FirstFloat,
	F32,
	F16,

    FirstOther,
    Bool = (::U32)FirstOther,

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
	return b > a ? a : b;
}

struct Type {
	void* codegen = nullptr; // Opaque pointer that can be used by the code generator.
	Type* canonical;

	enum Kind {
		Unknown,
        Alias,
		Unit,
		Tuple,
		Var,
		Array,
		Map,
		Lvalue,
		Gen,
		App,
		Lam,

		// These are often checked for together, so we let them share a single bit.
		Prim = 0x10,
		Ptr = 0x11,

		// When adding new types, make sure no other types have bit 4 set.
	} kind;

	// If set, this type is completely resolved and contains only concrete data.
	// If not, it still contains generic data.
	bool resolved = true;

	bool isPointer() const {return kind == Ptr;}
    bool isPrimitive() const {return kind == Prim;}
	bool isPtrOrPrim() const {return ((Size)kind & 0x10) != 0;}
	bool isTuple() const {return kind == Tuple;}
	bool isTupleOrIndirect() const;
	bool isKnown() const {return kind != Unknown;}
	bool isUnit() const {return kind == Unit;}
    bool isAlias() const {return kind == Alias;}
	bool isBool() const;
	bool isLvalue() const {return kind == Lvalue;}
	bool isVariant() const {return kind == Var;}
	bool isGeneric() const {return kind == Gen;}
	bool isApplication() const {return kind == App;}
	bool isLambda() const {return kind == Lam;}
	bool isUnknown() const {return kind == Unknown;}

	Type(Kind kind) : kind(kind) {
		canonical = this;
	}
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
	PtrType(Type* type) : Type(Ptr), type(type) {}
	Type* type;
};

/// Represents an lvalue (as opposed to rvalue).
struct LVType : Type {
	LVType(Type* type) : Type(Lvalue) {canonical = type;}
};

inline bool Type::isTupleOrIndirect() const {
	return isTuple()
		|| (kind == Ptr && ((PtrType*)this)->type->isTuple())
		|| (kind == Lvalue && ((LVType*)this)->canonical->isTuple());
}

struct Field {
	Field(Id name, U32 index, Type* type, Type* container, Expr* content, bool constant) :
		name(name), index(index), type(type), container(container), content(content), constant(constant) {}

	Id name;
	U32 index;
	Type* type;
	Type* container;
	Expr* content;
	bool constant;
};

typedef Array<Field> FieldList;

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

typedef Array<VarConstructor*> VarConstructorList;

struct VarType : Type {
	VarType(Id name, ast::DataDecl* astDecl, Scope& scope) :
			Type(Var), astDecl(astDecl), scope(scope), name(name) {}
	VarType(const VarType&) = default;

	ast::DataDecl* astDecl;
	Scope& scope;
	Id name;
	U32 generics = ast::count(astDecl->type->kind);
	VarConstructorList list;
	Byte selectorBits = 0;
	bool isEnum = false;
};

struct LamType : Type {
	LamType(Function& fun, ExprRef contents) : Type(Lam), fun(fun), contents(contents) {}
	Function& fun;
	ExprRef contents;
};

struct ArrayType : Type {
	ArrayType(Type* type) : Type(Array), type(type) {}
	Type* type;
};

struct MapType : Type {
	MapType(Type* from, Type* to) : Type(Map), from(from), to(to) {}
	Type* from;
	Type* to;
};

struct Constraint {
	enum Kind {
		// Defines that a certain function involving the type must exist.
		Fun,

		// Defines that the type must be a tuple and have a field with a certain index/name and type.
		Field
	};

	struct FunC {
		Id name; // The name of the function that has a parameter of this type.
		U32 index; // The index the parameter should be at.
	};
	struct FieldC {
		U32 index; // Set if index equals 0.
		Id name;
		Type* type; // The type of this field.
	};

	union {
		FunC fun;
		FieldC field;
	};
	Kind kind;
};

inline Constraint FunConstraint(Id name, U32 index) {
	Constraint c;
	c.kind = Constraint::Fun;
	c.fun.name = name;
	c.fun.index = index;
	return c;
}

inline Constraint FieldConstraint(Size index, Id name, Type* type) {
	Constraint c;
	c.kind = Constraint::Field;
	c.field.index = index;
	c.field.name = name;
	c.field.type = type;
	return c;
}

inline Constraint::FunC* FunConstraint(Constraint& c) {
	if(c.kind == Constraint::Fun) return &c.fun;
	else return nullptr;
}

inline Constraint::FieldC* FieldConstraint(Constraint& c) {
	if(c.kind == Constraint::Field) return &c.field;
	else return nullptr;
}

struct GenType : Type {
	GenType(int index) : Type(Gen), index(index) {resolved = false;}

	/**
	 * A set of constraints on what this type can be.
	 * Each constraint defines a subset of all types.
	 * The set of concrete types this generic can become is defined by the intersection between all subsets.
	 * Whenever the intersection size is reduced to one (partial) type, the `typeConstraint` field is set.
	 * This type can still define generic parameters of its own, but must be more specific than this one.
	 * A code example:
	 *    someFun a = case a of
	 *    				Just x  -> True
	 *    				Nothing -> False
	 *  - When compiling this function we walk through any explicit parameters first (in this case, 'a').
	 *  - Since no explicit type was given, 'a' is assigned a new generic type.
	 *  - We now compile the case statement, starting with the pivot. This just references 'a', and defines no new constraints.
	 *  - We compile the first alt. This contains a constructor pattern referencing the Just-constructor.
	 *  - Since Just is a constructor of Maybe, we can assign an explicit type. This is the new generic type 'Maybe a'.
	 *  - The nested pattern 'x' defines no new constraints; neither does the second alt 'Nothing'.
	 *  - The return type is trivially inferred to Bool.
	 *  - The signature of the whole function now becomes 'Maybe a -> Bool'.
	 */
	::Array<Constraint> constraints;
	Type* typeConstraint = nullptr;

	/**
	 * The index of a generic type is related to a possible type containing it.
	 * It refers to the index of the generic parameter this represents.
	 * For example, in the type 'Either a b', 'b' is represented with the index 1.
	 */
	U32 index;
};

struct AppType : Type {
	AppType(U32 baseIndex, ast::TypeList* apps) :
			Type(App), baseIndex(baseIndex), apps(apps) {resolved = false;}
	U32 baseIndex;
	ast::TypeList* apps;
};

struct AliasType : Type {
    AliasType(Id name, ast::TypeDecl* astDecl, Scope& scope) :
			Type(Alias), astDecl(astDecl), name(name), scope(scope) {resolved = generics == 0;}
	ast::TypeDecl* astDecl;
	Id name;
	U32 generics = ast::count(astDecl->type->kind);
	Scope& scope;
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
	Shl = (Size)FirstBit,
	Shr,
	And,
	Or,
	Xor,

    FirstCompare,
	CmpEq = (Size)FirstCompare,
	CmpNeq,
	CmpGt,
	CmpGe,
	CmpLt,
	CmpLe,

	// Unary
	FirstUnary,
	Neg = (Size)FirstUnary,
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
		Scoped,

		// The following expressions are only used while resolving or for error handling.
		// They are removed before the resolve stage finishes.
		FirstTemporary,
		EmptyDecl = FirstTemporary,
		Empty,
		GenApp,
	} kind;

	bool isTemp() const {return kind < FirstTemporary;}
	bool isLiteral() const {return kind == Lit;}
	bool isVar() const {return kind == Var;}

	Type* type;
	Expr(Kind k, Type* type) : kind(k), type(type) {}
};

typedef Array<Expr*> Exprs;

struct MultiExpr : Expr {
	MultiExpr(Exprs&& es) : Expr(Multi, (*es.back())->type), es(std::move(es)) {}
	Exprs es;
};

struct LitExpr : Expr {
	LitExpr(Literal lit, Type* type) : Expr(Lit, type), literal(lit) {}
	Literal literal;
};

struct VarExpr : Expr {
	VarExpr(Variable* var, Type* type) : Expr(Var, type), var(var) {}
	Variable* var;
};

struct AppExpr : Expr {
	AppExpr(FuncRef n, ExprList* args) : Expr(App, n.type), callee(n), args(args) {}
	FuncRef callee;
	ExprList* args;
};

struct AppIExpr : Expr {
	AppIExpr(const char* i, ExprList* args, Type* type) : Expr(AppI, type), callee(i), args(args) {}
	const char* callee;
	ExprList* args;
};

struct AppPExpr : Expr {
	AppPExpr(PrimitiveOp op, ExprList* args, Type* type) : Expr(AppP, type), args(args), op(op) {}
	ExprList* args;
	PrimitiveOp op;
};

struct GenAppExpr : Expr {
	GenAppExpr(Id name, ExprList* args, Type* type) : Expr(GenApp, type), args(args), name(name) {}
	ExprList* args;
	Id name;
};

struct CaseExpr : Expr {
    CaseExpr(AltList* alts, Expr* otherwise, Type* type) : Expr(Case, type), alts(alts), otherwise(otherwise) {}
    AltList* alts;
    Expr* otherwise;
};

struct IfCond {
	IfCond(Expr* scope, Expr* cond) : scope(scope), cond(cond) {}
	Expr* scope;
 	Expr* cond;
};

typedef Array<IfCond> IfConds;

enum class CondMode : Byte {
	/// The then-branch is executed if all conditions are true.
	And,

	/// The then-branch is executed if at least one condition is true.
	Or
};

bool succeedsAlways(const IfConds& conds, CondMode mode);

struct IfExpr : Expr {
	IfExpr(IfConds&& conds, ExprRef then, Expr* otherwise, Type* type, bool ret, CondMode mode);
	IfExpr(IfConds&& conds, ExprRef then, Expr* otherwise, Type* type, bool ret, CondMode mode, bool alwaysTrue);
	IfConds conds;
	ExprRef then;
	Expr* otherwise;
	CondMode mode;
	bool returnResult;
	bool alwaysTrue;
};

struct WhileExpr : Expr {
	WhileExpr(ExprRef cond, ExprRef loop, Type* type) : Expr(While, type), cond(cond), loop(loop) {}
	ExprRef cond;
	ExprRef loop;
};

struct LoadExpr : Expr {
	LoadExpr(ExprRef target, Type* type) : Expr(Load, type), target(target) {}
	ExprRef target;
};

struct StoreExpr : Expr {
	StoreExpr(ExprRef target, ExprRef value, Type* type) : Expr(Store, type), target(target), value(value) {}
	ExprRef target;
	ExprRef value;
};

struct AssignExpr : Expr {
	AssignExpr(Variable& target, ExprRef value) : Expr(Assign, target.type), target(target), value(value) {}
	Variable& target;
	ExprRef value;
};

struct CoerceExpr : Expr {
	CoerceExpr(ExprRef src, Type* dst) : Expr(Coerce, dst), src(src) {}
	ExprRef src;
};

struct CoerceLVExpr : Expr {
	CoerceLVExpr(ExprRef src, Type* dst) : Expr(CoerceLV, dst), src(src) {}
	ExprRef src;
};

struct FieldExpr : Expr {
	FieldExpr(ExprRef container, ::athena::resolve::Field* field, Type* type) : Expr(Field, type), container(container), field(field) {}
	FieldExpr(ExprRef container, int constructor, Type* type) : Expr(Field, type), container(container), constructor(constructor) {}
	ExprRef container;
	union {
		// When the type is a structure.
		::athena::resolve::Field *field;

		// When the type is a variant. -1 indicates the index field.
		int constructor;
	};
};

struct RetExpr : Expr {
	RetExpr(ExprRef e) : Expr(Ret, e.type), expr(e) {}
	ExprRef expr;
};

struct ConstructArg {
	U32 index;
	ExprRef expr;
};

struct ConstructExpr : Expr {
	ConstructExpr(Type* type, VarConstructor* con = nullptr) : Expr(Construct, type), con(con) {}
	Array<ConstructArg> args;

	// Only set if the type is a variant.
	VarConstructor* con;
};

struct ScopedExpr : Expr {
	ScopedExpr(Scope& parent) : Expr(Scoped, nullptr) {
		scope.parent = &parent;
		scope.function = parent.function;
	}
	Expr* contents = nullptr;
	Scope scope;
};

/// A temporary expression that represents an unfinished declaration.
struct EmptyDeclExpr : Expr {
	EmptyDeclExpr(Variable& var) : Expr(EmptyDecl, var.type), var(var) {}
	Variable& var;
};

/// A temporary expression that represents a non-existent value, such as a compilation error.
struct EmptyExpr : Expr {
	EmptyExpr(Type* type) : Expr(Empty, type) {}
};

inline LitExpr* findLiteral(ExprRef e) {
	if(e.isLiteral()) {
		return (LitExpr*)&e;
	} else if(e.kind == Expr::Multi) {
		auto m = (MultiExpr*)&e;
		if(m->es.size() && (*m->es.back())->isLiteral())
			return findLiteral(**m->es.back());
	} else if(e.kind == Expr::Scoped) {
		return findLiteral(*((ScopedExpr*)&e)->contents);
	} else if(e.kind == Expr::If) {
		auto i = (IfExpr*)&e;
		if(i->alwaysTrue)
			return findLiteral(i->then);
	}
	return nullptr;
}

// Only call this after a successful call to findLiteral.
inline void updateLiteral(ExprRef e, Type* type) {
	if(e.kind == Expr::Multi) {
		auto m = (MultiExpr*)&e;
		if(m->es.size() && (*m->es.back())->isLiteral()) {
			findLiteral(**m->es.back());
			e.type = type;
		}
	} else if(e.kind == Expr::Scoped) {
		findLiteral(*((ScopedExpr*)&e)->contents);
		e.type = type;
	} else if(e.kind == Expr::If) {
		auto i = (IfExpr*)&e;
		if(i->alwaysTrue) {
			findLiteral(i->then);
			i->type = type;
		}
	}
}

}} // namespace athena::resolve

#endif // Athena_Resolve_resolve__ast_h