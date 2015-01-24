#ifndef Athena_Parser_resolve_h
#define Athena_Parser_resolve_h

#include "parser.h"
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

        stringType = &getArray(getU8());
    }

    TypeRef getPrim(PrimitiveType t) const {return prims[(uint)t];}
    TypeRef getBool() const {return prims[(uint)PrimitiveType::Bool];}
    TypeRef getFloat() const {return prims[(uint)PrimitiveType::F32];}
    TypeRef getDouble() const {return prims[(uint)PrimitiveType::F64];}
    TypeRef getInt() const {return prims[(uint)PrimitiveType::I32];}
    TypeRef getU8() const {return prims[(uint)PrimitiveType::U8];}
    TypeRef getString() const {return *stringType;}

    TypeRef getArray(TypeRef content) {
        ArrayType* type;
        if(!arrays.AddGet(&content, type))
            new (type) ArrayType{content};

        return *type;
    }

    Core::FixedArray<PrimType, (uint)PrimitiveType::TypeCount> prims;
    Core::NumberMap<ArrayType, const Type*> arrays;
    const Type* stringType;
};

struct Resolver {
	Resolver(ast::CompileContext& context, ast::Module& source) : context(context), source(source), buffer(4*1024*1024) {}

	Module* resolve();
	bool resolveFunction(Function& fun, ast::FunDecl& decl);
	Expr* resolveExpression(Scope& scope, ast::ExprRef expr);
    Expr* resolveLiteral(Scope& scope, const ast::LitExpr& expr);
	Expr* resolveInfix(Scope& scope, const ast::InfixExpr& expr);
	Expr* resolvePrefix(Scope& scope, const ast::PrefixExpr& expr);
	Expr* resolveCall(Scope& scope, const ast::AppExpr& expr);
    Expr* resolveVar(Scope& scope, Id var);
    Expr* resolveIf(Scope& scope, const ast::IfExpr& expr);
	Expr* resolveDecl(Scope& scope, const ast::DeclExpr& expr);
	Expr* resolveAssign(Scope& scope, const ast::AssignExpr& expr);
	Expr* resolveWhile(Scope& scope, const ast::WhileExpr& expr);

    /// Resolves a binary operation on two primitive types.
    /// *lhs* and *rhs* must be primitives.
	Expr* resolvePrimitiveOp(Scope& scope, PrimitiveOp op, resolve::ExprRef lhs, resolve::ExprRef rhs);

    /// Resolves a unary operation on a primitive type.
    /// *dst* must be a primitive.
	Expr* resolvePrimitiveOp(Scope& scope, PrimitiveOp op, resolve::ExprRef dst);

	Variable* resolveArgument(ScopeRef scope, Id arg);

    const Type* getBinaryOpType(PrimitiveOp, PrimitiveType, PrimitiveType);
    const Type* getUnaryOpType(PrimitiveOp, PrimitiveType);

	nullptr_t error(const char*);

	template<class P, class...Ps>
	nullptr_t error(const char*, P, Ps...);

	template<class T, class... P>
	T* build(P&&... p) {
		return buffer.New<T>(Core::Forward<P>(p)...);
	}

	Id primitiveOps[(uint)PrimitiveOp::OpCount];
	ast::CompileContext& context;
	ast::Module& source;
	Core::StaticBuffer buffer;
	TypeCheck typeCheck;
    TypeManager types;
};

}} // namespace athena::resolve

#endif // Athena_Parser_resolve_h