
#ifndef Athena_Generate_genHLSL_h
#define Athena_Generate_genHLSL_h

#include "../Resolve/resolve_ast.h"
#include "../Parse/lexer.h"

namespace athena {
namespace gen {

struct GenHLSL {
    GenHLSL(ast::CompileContext& ccontext);

    String generate(resolve::Module& module);
    void genFunction(resolve::Function& decl);
    void genFunctionDecl(resolve::FunctionDecl& function);
    void genScope(resolve::Scope& scope);
    String genExpr(resolve::ExprRef expr);
    String genLiteral(resolve::Literal& literal, resolve::TypeRef type);
    String genVar(resolve::Variable& var);
    String genAssign(resolve::AssignExpr& assign);
    String genLoad(resolve::Expr& target);
    void genStore(resolve::StoreExpr& expr);
    String genMulti(resolve::MultiExpr& expr);
    void genRet(resolve::RetExpr& expr);

    String genCall(resolve::FunctionDecl& function, resolve::ExprList* args);
    String genPrimitiveCall(resolve::PrimitiveOp op, resolve::ExprList* args);
    String genUnaryOp(resolve::PrimitiveOp op, resolve::PrimType type, llvm::Value* in);
    String genBinaryOp(resolve::PrimitiveOp op, llvm::Value* lhs, llvm::Value* rhs, resolve::PrimType lt, resolve::PrimType rt);
    String genPtrOp(resolve::PrimitiveOp op, llvm::Value* lhs, llvm::Value* rhs, resolve::PtrType type);
    String genPtrOp(resolve::PrimitiveOp op, llvm::Value* lhs, llvm::Value* rhs, resolve::PtrType lt, resolve::PrimType rt);
    String genCase(resolve::CaseExpr& casee);
    String genIf(resolve::IfExpr& ife);
    String genCoerce(resolve::Expr& src, resolve::Type* dst);
    String genCoerceLV(resolve::Expr& src, resolve::Type* dst);
    String genWhile(resolve::WhileExpr& expr);
    String genField(resolve::FieldExpr& expr);
    String genConstruct(resolve::ConstructExpr& expr);
    String genScoped(resolve::ScopedExpr& expr);
    String genLazyCond(resolve::PrimitiveOp op, resolve::ExprRef lhs, resolve::ExprRef rhs);
    void genVarDecl(resolve::Variable& var);

    void genType(StringBuilder& target, resolve::TypeRef type);
    void genTypeDecl(StringBuilder& target, resolve::TypeRef type);

private:
    StringBuilder declarations;
    StringBuilder functions;
    ast::CompileContext& ccontext;
    Size tupleCounter = 0;
};

}} // namespace athena::gen

#endif // Athena_Generate_genHLSL_h
