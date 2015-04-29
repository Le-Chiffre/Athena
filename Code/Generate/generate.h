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
	void genFunction(llvm::Function* function, resolve::Function& decl);
	llvm::Function* genFunctionDecl(resolve::FunctionDecl& function);
	llvm::BasicBlock* genScope(resolve::Scope& scope);
	llvm::Value* genExpr(resolve::ExprRef expr);
	llvm::Value* genLiteral(resolve::Literal& literal);
	llvm::Value* genVar(resolve::Variable& var);
	llvm::Value* genAssign(resolve::AssignExpr& assign);
	llvm::Value* genLoad(resolve::Expr& target);
	llvm::Value* genStore(resolve::StoreExpr& expr);
	llvm::Value* genMulti(resolve::MultiExpr& expr);
	llvm::Value* genRet(resolve::RetExpr& expr);

	llvm::Value* genCall(resolve::FunctionDecl& function, resolve::ExprList* args);
	llvm::Value* genPrimitiveCall(resolve::PrimitiveOp op, resolve::ExprList* args);
	llvm::Value* genUnaryOp(resolve::PrimitiveOp op, resolve::PrimType type, llvm::Value* in);
	llvm::Value* genBinaryOp(resolve::PrimitiveOp op, llvm::Value* lhs, llvm::Value* rhs, resolve::PrimType lt, resolve::PrimType rt);
	llvm::Value* genPtrOp(resolve::PrimitiveOp op, llvm::Value* lhs, llvm::Value* rhs, resolve::PtrType type);
	llvm::Value* genPtrOp(resolve::PrimitiveOp op, llvm::Value* lhs, llvm::Value* rhs, resolve::PtrType lt, resolve::PrimType rt);
    llvm::Value* genCase(resolve::CaseExpr& casee);
	llvm::Value* genIf(resolve::IfExpr& ife);
	llvm::Value* genCoerce(const resolve::Expr& src, resolve::Type* dst);
	llvm::Value* genCoerceLV(const resolve::Expr& src, resolve::Type* dst);
	llvm::Value* genWhile(resolve::WhileExpr& expr);
	llvm::Value* genField(resolve::FieldExpr& expr);
	llvm::Value* genConstruct(resolve::ConstructExpr& expr);
	
	llvm::Function* getFunction() {
		return builder.GetInsertBlock()->getParent();
	}

	llvm::Type* getType(resolve::TypeRef type) {
		if(!type->codegen) {
			type->codegen = genLlvmType(type);
		}

		return (llvm::Type*)type->codegen;
	}

	uint getCconv(resolve::ForeignConvention conv) {
		switch(conv) {
			case resolve::ForeignConvention::CCall:
				return llvm::CallingConv::C;
			case resolve::ForeignConvention::Stdcall:
				return llvm::CallingConv::X86_StdCall;
			case resolve::ForeignConvention::Cpp:
				// Cpp only affects name mangling.
				return llvm::CallingConv::C;
		}

		ASSERT(false);
		return llvm::CallingConv::C;
	}

private:
	llvm::Type* genLlvmType(resolve::TypeRef type);

	llvm::LLVMContext& context;
	llvm::Module& module;
	llvm::IRBuilder<> builder;
	ast::CompileContext& ccontext;
};

}} // namespace athena::gen

#endif // Athena_Generate_generate_h