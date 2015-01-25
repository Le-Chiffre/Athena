#ifndef Athena_Resolve_resolve_h
#define Athena_Resolve_resolve_h

#include "../Parse/parser.h"
#include "resolve_ast.h"

namespace athena {
namespace resolve {

struct TypeCheck {
	bool compatible(resolve::ExprRef a, resolve::ExprRef b) {
		return &a.type == &b.type;
	}
};

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

    Core::FixedArray<PrimType, (uint)PrimitiveType::TypeCount> prims;
    Core::NumberMap<ArrayType, TypeRef> arrays;
	Core::NumberMap<PtrType, TypeRef> ptrs;
    const Type* stringType;
	const Type unitType{Type::Unit};
	const Type unknownType{Type::Unknown};
};

struct Resolver {
	Resolver(ast::CompileContext& context, ast::Module& source);

	Module* resolve();
	bool resolveFunction(Function& fun, ast::FunDecl& decl);
	Expr* resolveExpression(Scope& scope, ast::ExprRef expr);
    Expr* resolveLiteral(Scope& scope, ast::LitExpr& expr);
	Expr* resolveInfix(Scope& scope, ast::InfixExpr& expr);
	Expr* resolvePrefix(Scope& scope, ast::PrefixExpr& expr);
	Expr* resolveBinaryCall(Scope& scope, ast::ExprRef function, ast::ExprRef lhs, ast::ExprRef rhs);
	Expr* resolveUnaryCall(Scope& scope, ast::ExprRef function, ast::ExprRef dst);
	Expr* resolveCall(Scope& scope, ast::AppExpr& expr);
    Expr* resolveVar(Scope& scope, Id var);
    Expr* resolveIf(Scope& scope, ast::IfExpr& expr);
	Expr* resolveDecl(Scope& scope, ast::DeclExpr& expr);
	Expr* resolveAssign(Scope& scope, ast::AssignExpr& expr);
	Expr* resolveWhile(Scope& scope, ast::WhileExpr& expr);
	Expr* resolveCoerce(Scope& scope, ast::CoerceExpr& expr);
	Expr* resolveField(Scope& scope, ast::FieldExpr& expr);
	Expr* resolveConstruct(Scope& scope, ast::ConstructExpr& expr);

    /// Resolves a binary operation on two primitive types.
    /// *lhs* and *rhs* must be primitives.
	Expr* resolvePrimitiveOp(Scope& scope, PrimitiveOp op, resolve::ExprRef lhs, resolve::ExprRef rhs);

    /// Resolves a unary operation on a primitive type.
    /// *dst* must be a primitive.
	Expr* resolvePrimitiveOp(Scope& scope, PrimitiveOp op, resolve::ExprRef dst);

	Variable* resolveArgument(ScopeRef scope, Id arg);

	/// Retrieves or creates a concrete type.
	TypeRef resolveType(ScopeRef scope, ast::TypeRef type);

    const Type* getBinaryOpType(PrimitiveOp, PrimitiveType, PrimitiveType);
	const Type* getPtrOpType(PrimitiveOp, const PtrType*, PrimitiveType);
	const Type* getPtrOpType(PrimitiveOp, const PtrType*, const PtrType*);
    const Type* getUnaryOpType(PrimitiveOp, PrimitiveType);

	/// Checks if the provided callee expression can contain a primitive operator.
	PrimitiveOp* tryPrimitiveOp(ast::ExprRef callee);

	/// Checks if the provided type can be implicitly converted to the target type.
	CoerceExpr* implicitCoerce(ExprRef src, TypeRef dst);

	/// Tries to find a function from the provided expression that takes the provided parameters.
	Function* findFunction(ScopeRef scope, ast::ExprRef, ExprList* args);

	/// Reorders the provided chain of infix operators according to the operator precedence table.
	ast::InfixExpr& reorder(ast::InfixExpr& expr);

	nullptr_t error(const char*);

	template<class P, class...Ps>
	nullptr_t error(const char*, P, Ps...);

	template<class T, class... P>
	T* build(P&&... p) {
		return buffer.New<T>(Core::Forward<P>(p)...);
	}

	/// Initializes the data used to recognize and resolve primitives.
	/// This also makes sure that no primitive types and operators are redefined in the program.
	void initPrimitives();

	Id primitiveOps[(uint)PrimitiveOp::OpCount];
	Core::NumberMap<PrimitiveOp, Id> primitiveMap;
	ast::CompileContext& context;
	ast::Module& source;
	Core::StaticBuffer buffer;
	TypeCheck typeCheck;
    TypeManager types;
};

}} // namespace athena::resolve

#endif // Athena_Resolve_resolve_h