#ifndef Athena_Resolve_resolve_h
#define Athena_Resolve_resolve_h

#include "../Parse/parser.h"
#include "resolve_ast.h"
#include "../General/diagnostic.h"
#include "typecheck.h"
#include "mangle.h"

namespace athena {
namespace resolve {

typedef Core::NumberMap<PrimitiveOp, Id> PrimOpMap;

struct TypeManager {
    TypeManager() {
        for(uint i=0; i<(uint)PrimitiveType::TypeCount; i++) {
            prims += PrimType{(PrimitiveType)i};
        }

        stringType = getArray(getU8());
    }

	TypeRef getUnit() const {return &unitType;}
	TypeRef getUnknown() const {return &unknownType;}

    TypeRef getPrim(PrimitiveType t) const {return &prims[(uint)t];}
    TypeRef getBool() const {return &prims[(uint)PrimitiveType::Bool];}
    TypeRef getFloat() const {return &prims[(uint)PrimitiveType::F32];}
    TypeRef getDouble() const {return &prims[(uint)PrimitiveType::F64];}
    TypeRef getInt() const {return &prims[(uint)PrimitiveType::I32];}
    TypeRef getU8() const {return &prims[(uint)PrimitiveType::U8];}
    TypeRef getString() const {return stringType;}

    TypeRef getArray(TypeRef content) {
        ArrayType* type;
        if(!arrays.AddGet(content, type))
            new (type) ArrayType{content};

        return type;
    }

	TypeRef getPtr(TypeRef content) {
		PtrType* type;
		if(!ptrs.AddGet(content, type))
			new (type) PtrType{content};

		return type;
	}

	bool getTuple(uint hash, TupleType*& type) {
		return tuples.AddGet(hash, type);
	}

    Core::FixedArray<PrimType, (uint)PrimitiveType::TypeCount> prims;
	Core::NumberMap<TypeRef, Id> primMap; // Maps from ast type name to type.
    Core::NumberMap<ArrayType, TypeRef> arrays;
	Core::NumberMap<PtrType, TypeRef> ptrs;
	Core::NumberMap<TupleType, uint> tuples;
    const Type* stringType;
	const Type unitType{Type::Unit};
	const Type unknownType{Type::Unknown};
};

struct Resolver {
	Resolver(ast::CompileContext& context, ast::Module& source);

	Module* resolve();
	bool resolveFunction(Scope& scope, Function& fun);
	Expr* resolveExpression(Scope& scope, ast::ExprRef expr);
	Expr* resolveMulti(Scope& scope, ast::MultiExpr& expr);
	Expr* resolveMultiWithRet(Scope& scope, ast::MultiExpr& expr);
    Expr* resolveLiteral(Scope& scope, ast::LitExpr& expr);
	Expr* resolveInfix(Scope& scope, ast::InfixExpr& expr);
	Expr* resolvePrefix(Scope& scope, ast::PrefixExpr& expr);
	Expr* resolveBinaryCall(Scope& scope, Id function, ExprRef lhs, ExprRef rhs);
	Expr* resolveUnaryCall(Scope& scope, Id function, ExprRef dst);
	Expr* resolveCall(Scope& scope, ast::AppExpr& expr);
    Expr* resolveVar(Scope& scope, Id var);
    Expr* resolveIf(Scope& scope, ast::IfExpr& expr);
	Expr* resolveDecl(Scope& scope, ast::DeclExpr& expr);
	Expr* resolveAssign(Scope& scope, ast::AssignExpr& expr);
	Expr* resolveWhile(Scope& scope, ast::WhileExpr& expr);
	Expr* resolveCoerce(Scope& scope, ast::CoerceExpr& expr);
	Expr* resolveField(Scope& scope, ast::FieldExpr& expr, ast::ExprList* args = nullptr);
	Expr* resolveConstruct(Scope& scope, ast::ConstructExpr& expr);
	
	void resolveAlias(Scope& scope, AliasType* type);
    void resolveAggregate(Scope& scope, AggType* type);
	TypeRef resolveTuple(Scope& scope, ast::TupleType& type);

    /// Resolves a binary operation on two primitive types.
    /// *lhs* and *rhs* must be primitives.
	Expr* resolvePrimitiveOp(Scope& scope, PrimitiveOp op, resolve::ExprRef lhs, resolve::ExprRef rhs);

    /// Resolves a unary operation on a primitive type.
    /// *dst* must be a primitive.
	Expr* resolvePrimitiveOp(Scope& scope, PrimitiveOp op, resolve::ExprRef dst);

	Variable* resolveArgument(ScopeRef scope, ast::TupleField& arg);
    Field resolveField(ScopeRef scope, TypeRef container, ast::Field& field);
	
	/// Creates a boolean condition from the provided expression.
	Expr* resolveCondition(ScopeRef scope, ast::ExprRef expr);

	/// Retrieves or creates a concrete type.
	TypeRef resolveType(ScopeRef scope, ast::TypeRef type);

    const Type* getBinaryOpType(PrimitiveOp, PrimitiveType, PrimitiveType);
	const Type* getPtrOpType(PrimitiveOp, const PtrType*, PrimitiveType);
	const Type* getPtrOpType(PrimitiveOp, const PtrType*, const PtrType*);
    const Type* getUnaryOpType(PrimitiveOp, PrimitiveType);

	/// Checks if the provided callee expression can contain a primitive operator.
	PrimitiveOp* tryPrimitiveBinaryOp(Id callee);
	PrimitiveOp* tryPrimitiveUnaryOp(Id callee);

	/// Checks if the provided type can be implicitly converted to the target type.
	CoerceExpr* implicitCoerce(ExprRef src, TypeRef dst);

	/// Checks if the provided literal type can be converted to the target type.
	/// This is more flexible than an implicit coercion and done on compile time.
	LitExpr* literalCoerce(const ast::Literal& lit, TypeRef dst);

	/// Tries to find a function from the provided expression that takes the provided parameters.
	Function* findFunction(ScopeRef scope, ast::ExprRef, ExprList* args);

	/// Tries to find a function with the provided name that takes the provided parameters.
	Function* findFunction(ScopeRef scope, Id name, ExprList* args);

	/// Checks if the provided function can potentially be called with the provided arguments.
	bool potentiallyCallable(Function* fun, ExprList* args);

	/// Finds the best matching function from the current potential callees list.
	Function* findBestMatch(ExprList* args);

	/// Returns the number of implicit conversions needed to call this function with the provided arguments.
	/// The function must be callable with these arguments.
	uint findImplicitConversionCount(Function* f, ExprList* args);

	/// Reorders the provided chain of infix operators according to the operator precedence table.
	ast::InfixExpr& reorder(ast::InfixExpr& expr);

	nullptr_t error(const char*);

	template<class P, class... Ps>
	nullptr_t error(const char* text, P first, Ps... more) {
		Core::LogError(text, first, more...);
		return nullptr;
	}

	template<class T, class... P>
	T* build(P&&... p) {
		return buffer.New<T>(Core::Forward<P>(p)...);
	}

	/// Initializes the data used to recognize and resolve primitives.
	/// This also makes sure that no primitive types and operators are redefined in the program.
	void initPrimitives();

	Id primitiveOps[(uint)PrimitiveOp::OpCount];
	PrimOpMap primitiveBinaryMap;
	PrimOpMap primitiveUnaryMap;
	ast::CompileContext& context;
	ast::Module& source;
	Core::StaticBuffer buffer;
    TypeManager types;
	TypeCheck typeCheck;
	EmptyExpr emptyExpr{types.getUnit()};
	Mangler mangler{context};

	Function* currentFunction = nullptr;
	Scope* currentScope = nullptr;

	// This is used to accumulate potentially callable functions.
	Core::Array<Function*> potentialCallees{32};
};

}} // namespace athena::resolve

#endif // Athena_Resolve_resolve_h