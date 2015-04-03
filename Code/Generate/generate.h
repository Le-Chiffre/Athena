#ifndef Athena_Generate_generate_h
#define Athena_Generate_generate_h

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/IRBuilder.h>
#include "../Resolve/resolve_ast.h"
#include "../Parse/lexer.h"

namespace athena {
namespace gen {

inline llvm::StringRef toRef(ast::String str) {
	return {str.ptr, str.length};
}

struct SaveInsert {
	SaveInsert(llvm::IRBuilder<>& builder) : builder(builder) {
		block = builder.GetInsertBlock();
		insert = builder.GetInsertPoint();
	}
	
	~SaveInsert() {
		builder.SetInsertPoint(block, insert);
	}
	
	llvm::IRBuilder<>& builder;
	llvm::BasicBlock::iterator insert;
	llvm::BasicBlock* block;
};
	
struct Generator {
	Generator(ast::CompileContext& ccontext, llvm::LLVMContext& context, llvm::Module& target);

	llvm::Module* generate(resolve::Module& module);
	llvm::Function* genFunction(resolve::Function& function);
	llvm::Function* genFunctionDecl(resolve::Function& function);
	llvm::BasicBlock* genScope(resolve::Scope& scope);
	llvm::Value* genExpr(resolve::ExprRef expr);
	llvm::Value* genLiteral(resolve::Literal& literal);
	llvm::Value* genVar(resolve::Variable& var);
	llvm::Value* genAssign(resolve::AssignExpr& assign);
	llvm::Value* genMulti(resolve::MultiExpr& expr);
	llvm::Value* genRet(resolve::RetExpr& expr);

	llvm::Value* genCall(resolve::Function& function, resolve::ExprList* args);
	llvm::Value* genPrimitiveCall(resolve::PrimitiveOp op, resolve::ExprList* args);
	llvm::Value* genUnaryOp(resolve::PrimitiveOp op, resolve::PrimType type, llvm::Value* in);
	llvm::Value* genBinaryOp(resolve::PrimitiveOp op, llvm::Value* lhs, llvm::Value* rhs, resolve::PrimType lt, resolve::PrimType rt);
	llvm::Value* genPtrOp(resolve::PrimitiveOp op, llvm::Value* lhs, llvm::Value* rhs, resolve::PtrType type);
	llvm::Value* genPtrOp(resolve::PrimitiveOp op, llvm::Value* lhs, llvm::Value* rhs, resolve::PtrType lt, resolve::PrimType rt);
    llvm::Value* genCase(resolve::CaseExpr& casee);
	llvm::Value* genIf(resolve::IfExpr& ife);
	llvm::Value* genCoerce(resolve::CoerceExpr& coerce);

	llvm::Value* useResult(resolve::ExprRef expr);
	
	llvm::Function* getFunction() {
		return builder.GetInsertBlock()->getParent();
	}

	llvm::Type* getType(resolve::TypeRef type) {
		llvm::Type** t;
		if(!typeMap.AddGet(type, t))
			*t = genLlvmType(type);
		return *t;
	}

private:
	llvm::Type* genLlvmType(resolve::TypeRef type);

	llvm::LLVMContext& context;
	llvm::Module& module;
	llvm::IRBuilder<> builder;
	ast::CompileContext& ccontext;
	Core::NumberMap<llvm::Type*, resolve::TypeRef> typeMap;
};

}} // namespace athena::gen

#endif // Athena_Generate_generate_h