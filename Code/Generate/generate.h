#ifndef Athena_Generate_generate_h
#define Athena_Generate_generate_h

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/IRBuilder.h>
#include "../Parse/resolve_ast.h"
#include "../Parse/lexer.h"

namespace athena {
namespace gen {

llvm::StringRef toRef(ast::String str) {
	return {str.ptr, str.length};
}

struct Generator {
	Generator(ast::CompileContext& ccontext, llvm::LLVMContext& context, llvm::Module& target) :
		context(context), module(target), builder(context), ccontext(ccontext) {}

	llvm::Function* genFunction(resolve::Function& function);
	llvm::BasicBlock* genScope(resolve::Scope& scope);
	llvm::Value* genExpr(resolve::Expr& expr);
	llvm::Value* genLiteral(resolve::Literal& literal);

	llvm::Value* genCall(resolve::Function& function, resolve::ExprList* args);
	llvm::Value* genPrimitiveCall(resolve::PrimitiveOp op, resolve::ExprList* args);
	llvm::Value* genUnaryOp(resolve::PrimitiveOp op, llvm::Value* in);
	llvm::Value* genBinaryOp(resolve::PrimitiveOp op, llvm::Value* lhs, llvm::Value* rhs);

private:
	llvm::LLVMContext& context;
	llvm::Module& module;
	llvm::IRBuilder<> builder;
	ast::CompileContext& ccontext;
};

}} // namespace athena::gen

#endif // Athena_Generate_generate_h