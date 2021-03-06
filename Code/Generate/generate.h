#ifndef Athena_Generate_generate_h
#define Athena_Generate_generate_h

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/IRBuilder.h>
#include "../Resolve/resolve_ast.h"
#include "../Parse/lexer.h"

namespace athena {
namespace gen {

inline llvm::StringRef toRef(const std::string& str) {
	return {str.c_str(), str.length()};
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

struct VariantData {
	int selectorIndex = -1;
};

struct TypeData {
	llvm::Type* llType = nullptr;
	void* data = nullptr;
	bool onStack = false;
};

struct Generator {
	Generator(ast::CompileContext& ccontext, llvm::LLVMContext& context, llvm::Module& target);

	llvm::Module* generate(resolve::Module& module);
	void genFunction(llvm::Function* function, resolve::Function& decl);
	llvm::Function* genFunctionDecl(resolve::FunctionDecl& function);
	llvm::BasicBlock* genScope(resolve::Scope& scope);
	llvm::Value* genExpr(resolve::ExprRef expr);
	llvm::Value* genLiteral(resolve::Literal& literal, resolve::Type* type);
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
	llvm::Value* genCoerce(resolve::Expr& src, resolve::Type* dst);
	llvm::Value* genCoerceLV(resolve::Expr& src, resolve::Type* dst);
	llvm::Value* genWhile(resolve::WhileExpr& expr);
	llvm::Value* genField(resolve::FieldExpr& expr);
	llvm::Value* genConstruct(resolve::ConstructExpr& expr);
	llvm::Value* genScoped(resolve::ScopedExpr& expr);
	llvm::Value* genLazyCond(resolve::PrimitiveOp op, resolve::ExprRef lhs, resolve::ExprRef rhs);
	void genVarDecl(resolve::Variable& var);

	llvm::Function* getFunction() {
		return builder.GetInsertBlock()->getParent();
	}

	TypeData* getType(resolve::Type* type) {
		if(!type->codegen) {
			type->codegen = genLlvmType(type);
		}

		return (TypeData*)type->codegen;
	}

	U32 getCconv(resolve::ForeignConvention conv) {
		switch(conv) {
			case resolve::ForeignConvention::CCall:
				return llvm::CallingConv::C;
			case resolve::ForeignConvention::Stdcall:
				return llvm::CallingConv::X86_StdCall;
			case resolve::ForeignConvention::Cpp:
				// Cpp only affects name mangling.
				return llvm::CallingConv::C;
			case resolve::ForeignConvention::JS:
				return llvm::CallingConv::WebKit_JS;
		}

		assert(false);
		return llvm::CallingConv::C;
	}

private:
	TypeData* genLlvmType(resolve::Type* type);

	llvm::LLVMContext& context;
	llvm::Module& module;
	llvm::IRBuilder<> builder;
	ast::CompileContext& ccontext;
};

}} // namespace athena::gen

#endif // Athena_Generate_generate_h