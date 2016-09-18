#ifndef Athena_Resolve_resolve_h
#define Athena_Resolve_resolve_h

#include "../General/compiler.h"
#include "../Parse/parser.h"
#include "resolve_ast.h"
#include "typecheck.h"
#include "mangle.h"
#include "../General/array.h"
#include "../General/map.h"

namespace athena {
namespace resolve {

typedef Tritium::Map<Id, PrimitiveOp> PrimOpMap;

struct TypeManager {
    TypeManager() {
		unknownType.resolved = false;
        for(int i=0; i < (int)PrimitiveType::TypeCount; i++) {
            prims.push(PrimType{(PrimitiveType)i});
        }

        stringType = getPtr(getU8());
    }

	Type* getUnit() {return &unitType;}
	Type* getUnknown() {return &unknownType;}

    Type* getPrim(PrimitiveType t) {return &prims[(Size)t];}
    Type* getBool() {return &prims[(Size)PrimitiveType::Bool];}
    Type* getFloat() {return &prims[(Size)PrimitiveType::F32];}
    Type* getDouble() {return &prims[(Size)PrimitiveType::F64];}
    Type* getInt() {return &prims[(Size)PrimitiveType::I32];}
    Type* getU8() {return &prims[(Size)PrimitiveType::U8];}
    Type* getString() {return stringType;}

    Type* getArray(Type* content) {
        ArrayType* type;
        if(!arrays.addGet(content, type)) {
			new(type) ArrayType{content};
			type->resolved = content->resolved;
		}

        return type;
    }

	Type* getPtr(Type* content) {
		PtrType* type;
		if(!ptrs.addGet(content, type)) {
			new(type) PtrType{content};
			type->resolved = content->resolved;
		}

		return type;
	}

	bool getTuple(U32 hash, TupleType*& type) {
        return tuples.addGet(hash, type);
	}

	Type* getTuple(const FieldList& fields) {
		Hasher h;
		for(auto& f : fields) {
			h.add(f.type);
			if(f.name) h.add(f.name);
		}

		// Check if this kind of tuple has been used already.
		TupleType* result = nullptr;
		if(!getTuple((Id)h, result)) {
			// Otherwise, create the type.
			new (result) TupleType;
			bool resolved = true;
            result->fields.reserve(fields.size());
            for(auto& f : fields) {
                if(!f.type->resolved) resolved = false;
                result->fields << f;
            }
            result->resolved = resolved;
		}

		return result;
	}

	Type* getTuple(const TypeList& types) {
		Hasher h;
		for(auto t : types) h.add(t);

		// Check if this kind of tuple has been used already.
		TupleType* result = nullptr;
		if(!getTuple((Id)h, result)) {
			// Otherwise, create the type.
			new (result) TupleType;
			bool resolved = true;
            result->fields.reserve(types.size());
            U32 i = 0;
            for(auto& t : types) {
                if(!t->resolved) resolved = false;
                result->fields << Field{0, i, t, result, nullptr, true};
                i++;
            }
			result->resolved = resolved;
		}

		return result;
	}

	Type* getLV(Type* t) {
        LVType* type;
        if(!lvalues.addGet(t, type)) {
            new(type) LVType(t);
            type->resolved = t->resolved;
        }

        return type;
	}

	// t must be an lvalue.
	Type* getRV(Type* t) {
		assert(t->isLvalue());
		return t->canonical;
	}

    ArrayF<PrimType, (Size)PrimitiveType::TypeCount> prims;
    Tritium::Map<Id, Type*> primMap; // Maps from ast type name to type.
    Tritium::Map<Type*, ArrayType> arrays;
    Tritium::Map<Type*, PtrType> ptrs;
    Tritium::Map<Id, TupleType> tuples;
    Tritium::Map<Type*, LVType> lvalues;

	Type* stringType;
	Type unitType{Type::Unit};
	Type unknownType{Type::Unknown};
};

struct Resolver {
	Resolver(ast::CompileContext& context, ast::Module& source);

	Module* resolve();
	bool resolveFunctionDecl(Scope& scope, FunctionDecl& fun);
	bool resolveFunction(Scope& scope, Function& fun);
	bool resolveForeignFunction(Scope& scope, ForeignFunction& fun);
	Expr* resolveFunctionCases(Scope& scope, Function& fun, ast::FunCaseList* cases);
	Expr* resolveExpression(Scope& scope, ast::ExprRef expr, bool used);
	Expr* resolveMulti(Scope& scope, ast::MultiExpr& expr, bool used);
    Expr* resolveLiteral(Scope& scope, ast::Literal& expr);
	Expr* resolveInfix(Scope& scope, ast::InfixExpr& expr);
	Expr* resolvePrefix(Scope& scope, ast::PrefixExpr& expr);
	Expr* resolveBinaryCall(Scope& scope, Id function, ExprRef lhs, ExprRef rhs);
	Expr* resolveUnaryCall(Scope& scope, Id function, ExprRef dst);
	Expr* resolveCall(Scope& scope, ast::AppExpr& expr);
	Expr* resolveLambda(Scope& scope, ast::LamExpr& expr);
    Expr* resolveVar(Scope& scope, Id var);
    Expr* resolveIf(Scope& scope, ast::IfExpr& expr, bool used);
	Expr* resolveMultiIf(Scope& scope, ast::IfCaseList* cases, bool used);
	Expr* resolveDecl(Scope& scope, ast::DeclExpr& expr);
	Expr* resolveAssign(Scope& scope, ast::AssignExpr& expr);
	Expr* resolveWhile(Scope& scope, ast::WhileExpr& expr);
	Expr* resolveCoerce(Scope& scope, ast::CoerceExpr& expr);
	Expr* resolveField(Scope& scope, ast::FieldExpr& expr, ast::ExprList* args = nullptr);
	Expr* resolveConstruct(Scope& scope, ast::ConstructExpr& expr);
	Expr* resolveAnonConstruct(Scope& scope, ast::TupleConstructExpr& expr);
	Expr* resolveCase(Scope& scope, ast::CaseExpr& expr, bool used);
	Expr* resolveAlt(Scope& scope, ExprRef pivot, ast::AltList* alt, bool used);

	Type* resolveAlias(AliasType* type);
	Type* resolveTuple(Scope& scope, ast::TupleType& type, ast::SimpleType* tscope = nullptr);
	Type* resolveVariant(VarType* type);

	/// Adds to a list of conditions that must be true.
	void resolvePattern(Scope& scope, ExprRef pivot, ast::Pattern& pat, IfConds& conds);

    /// Resolves a binary operation on two primitive types.
    /// *lhs* and *rhs* must be primitives.
	Expr* resolvePrimitiveOp(Scope& scope, PrimitiveOp op, resolve::ExprRef lhs, resolve::ExprRef rhs);

    /// Resolves a unary operation on a primitive type.
    /// *dst* must be a primitive.
	Expr* resolvePrimitiveOp(Scope& scope, PrimitiveOp op, resolve::ExprRef dst);

	Variable* resolveArgument(ScopeRef scope, ast::TupleField& arg);
	Variable* resolveArgument(ScopeRef scope, ast::Type* arg);
    Field resolveField(ScopeRef scope, Type* container, U32 index, ast::Field& field);

	/// Creates a boolean condition from the provided expression.
	Expr* resolveCondition(ScopeRef scope, ast::ExprRef expr);

	/// Retrieves or creates a concrete type.
	/// @param constructor If set, the provided type is interpreted as a constructor instead of a type.
	Type* resolveType(ScopeRef scope, ast::TypeRef type, bool constructor = false, ast::SimpleType* tscope = nullptr);

	/// Ensures that the provided type has been converted from AST before use.
	/// This is used to solve dependencies between types.
	Type* lazyResolve(Type*);

	template<class F>
	Type* mapType(F&& f, Type* type);

	/// Instantiates a generic type.
	Type* instantiateType(ScopeRef scope, Type* base, ast::TypeList* apps, ast::SimpleType* tscope = nullptr);

	void constrain(Type* type, const Constraint&& c);
	void constrain(Type* type, Type* c);

	Type* getBinaryOpType(PrimitiveOp, PrimitiveType, PrimitiveType, Expr*&, Expr*&);
	Type* getPtrOpType(PrimitiveOp, PtrType*, PrimitiveType);
	Type* getPtrOpType(PrimitiveOp, PtrType*, PtrType*);
	Type* getUnaryOpType(PrimitiveOp, PrimitiveType);

	/// Creates a return of the provided expression.
	Expr* createRet(ExprRef);

	/// Creates a constant true value.
	Expr* createTrue() {return createBool(true);}

	/// Creates a constant false value.
	Expr* createFalse() {return createBool(false);}

	/// Creates a constant boolean value.
	Expr* createBool(bool b);

	/// Creates a constant integer value.
	Expr* createInt(int i);

	/// Creates a comparison between two values.
	Expr* createCompare(Scope& scope, ExprRef left, ExprRef right);

	/// Creates an if-expression.
	Expr* createIf(ExprRef cond, ExprRef then, Expr* otherwise, bool used);
	Expr* createIf(IfConds&& cond, ExprRef then, Expr* otherwise, bool used, CondMode mode);

	/// Creates a field-expression.
	Expr* createField(ExprRef pivot, Field* field);
	Expr* createField(ExprRef pivot, int field);

	/// Gets the provided variant's constructor index.
	Expr* createGetCon(ExprRef variant);

	/// Makes sure the provided expression is an rvalue.
	Expr* getRV(ExprRef);

	/// Checks if the provided callee expression can contain a primitive operator.
	PrimitiveOp* tryPrimitiveBinaryOp(Id callee);
	PrimitiveOp* tryPrimitiveUnaryOp(Id callee);

	/**
	 * In some cases, pointer expressions can be implicitly dereferenced.
	 * One example is "x {a: *Float2} = a.x", where a is deferenced implicitly.
	 * This function helps implementing this by doing the following:
	 *  - For pointer or reference types, it generates a load on the target.
	 *  - For other types, it just returns the target.
	 */
	Expr* implicitLoad(ExprRef target);

	/// Checks if the provided type can be implicitly converted to the target type.
	Expr* implicitCoerce(ExprRef src, Type* dst);

	/// Checks if the provided literal type can be converted to the target type.
	/// This is more flexible than an implicit coercion and done on compile time.
	LitExpr* literalCoerce(const ast::Literal& lit, Type* dst);
	LitExpr* literalCoerce(LitExpr* lit, Type* dst);

	/// Tries to find a function from the provided expression that takes the provided parameters.
	FunctionDecl* findFunction(ScopeRef scope, ast::ExprRef, ExprList* args);

	/// Tries to find a function with the provided name that takes the provided parameters.
	FunctionDecl* findFunction(ScopeRef scope, Id name, ExprList* args);

	/// Checks if the provided function can potentially be called with the provided arguments.
	bool potentiallyCallable(FunctionDecl* fun, ExprList* args);

	/// Finds the best matching function from the current potential callees list.
	FunctionDecl* findBestMatch(ExprList* args);

	/// Returns the number of implicit conversions needed to call this function with the provided arguments.
	/// The function must be callable with these arguments.
	U32 findImplicitConversionCount(FunctionDecl* f, ExprList* args);

	/// Reorders the provided chain of infix operators according to the operator precedence table.
	ast::InfixExpr* reorder(ast::InfixExpr& expr, U32 min_prec = 0);

	/// Checks if the provided expression always evaluates to a true constant.
	bool alwaysTrue(ExprRef expr);

	template<class T> auto list(const T& t) {return build<ast::ASTList<T>>(t);}
	template<class T> auto list(const T& t, ast::ASTList<T>* next) {return build<ast::ASTList<T>>(t, next);}

	template<class T, class F>
	auto map(ast::ASTList<T>* l, F&& f) {
		decltype(list(f(l->item))) ll = nullptr;
		if(l) {
			ll = list(f(l->item));
			auto a = ll;
			l = l->next;
			while(l) {
				a->next = list(f(l->item));
				a = a->next;
				l = l->next;
			}
		}
		return ll;
	}

	void* error(const char*);

	template<class P, class... Ps>
	void* error(const char* text, P first, Ps... more) {
		return nullptr;
	}

	template<class T, class... P>
	T* build(P&&... p) {
		return buffer.create<T>(forward<P>(p)...);
	}

	/// Initializes the data used to recognize and resolve primitives.
	/// This also makes sure that no primitive types and operators are redefined in the program.
	void initPrimitives();

	Id primitiveOps[(int)PrimitiveOp::OpCount];
	PrimOpMap primitiveBinaryMap;
	PrimOpMap primitiveUnaryMap;
	ast::CompileContext& context;
	ast::Module& source;
	Tritium::StaticBuffer buffer;
    TypeManager types;
	TypeCheck typeCheck;
	EmptyExpr emptyExpr{types.getUnit()};
	Mangler mangler{context};

	Function* currentFunction = nullptr;
	Scope* currentScope = nullptr;

	// This is used to accumulate potentially callable functions.
	Array<FunctionDecl*> potentialCallees{32};
};

}} // namespace athena::resolve

#endif // Athena_Resolve_resolve_h