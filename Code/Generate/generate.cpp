
#define __STDC_CONSTANT_MACROS
#define __STDC_FORMAT_MACROS
#define __STDC_LIMIT_MACROS

#include <llvm/IR/Module.h>
#include "generate.h"

using namespace llvm;

namespace athena {
namespace gen {

Function* Generator::genFunction(resolve::Function& function) {
	auto argCount = function.arguments.Count();
	auto argTypes = (Type**)StackAlloc(sizeof(Type*) * argCount);
	for(uint i=0; i<argCount; i++) {
		argTypes[i] = builder.getFloatTy();
	}

	auto type = FunctionType::get(builder.getFloatTy(), ArrayRef<Type*>(argTypes, argCount), false);
	auto func = Function::Create(type, Function::ExternalLinkage, toRef(ccontext.Find(function.name).name), &module);
	genScope(function)->insertInto(func);
	return func;
}

BasicBlock* Generator::genScope(resolve::Scope& scope) {
	auto block = BasicBlock::Create(context, "");
	if(scope.expression) {
		builder.SetInsertPoint(block);
		genExpr(*scope.expression);
		builder.ClearInsertionPoint();
	}
	return block;
}

Value* Generator::genExpr(resolve::ExprRef expr) {
	switch(expr.kind) {
		case resolve::Expr::Lit:
			return genLiteral(((resolve::LitExpr&)expr).literal);
		case resolve::Expr::Var:
			return nullptr;//((resolve::VarExpr&)expr).var;
		case resolve::Expr::App:
			return genCall(((resolve::AppExpr&)expr).callee, ((resolve::AppExpr&)expr).args);
		case resolve::Expr::AppI:
			return genPrimitiveCall(((resolve::AppPExpr&)expr).op, ((resolve::AppPExpr&)expr).args);
        case resolve::Expr::Case:
            return genCase((resolve::CaseExpr&)expr);
        default:
            FatalError("Unsupported expression type.");
            return nullptr;
	}
}

Value* Generator::genLiteral(resolve::Literal& literal) {
	return ConstantFP::get(context, APFloat(literal.d));
}

llvm::Value* Generator::genCall(resolve::Function& function, resolve::ExprList* args) {
    return nullptr;
}

llvm::Value* Generator::genPrimitiveCall(resolve::PrimitiveOp op, resolve::ExprList* args) {
	if(resolve::isBinary(op)) {
		ASSERT(args && args->next && !args->next->next);
		return genBinaryOp(op, genExpr(*args->item), genExpr(*args->next->item));
	} else if(resolve::isUnary(op)) {
		ASSERT(args && !args->next);
		return genUnaryOp(op, genExpr(*args->item));
	} else {
		FatalError("Unsupported primitive operator provided");
        return nullptr;
	}
}

llvm::Value* Generator::genUnaryOp(resolve::PrimitiveOp op, llvm::Value* in) {
	switch(op) {
		case resolve::PrimitiveOp::Neg:
			return builder.CreateFNeg(in);
		default:
			FatalError("Unsupported primitive operator provided.");
            return nullptr;
	}
}

llvm::Value* Generator::genBinaryOp(resolve::PrimitiveOp op, llvm::Value* lhs, llvm::Value* rhs) {
	switch(op) {
		case resolve::PrimitiveOp::Add:
			return builder.CreateFAdd(lhs, rhs);
		case resolve::PrimitiveOp::Sub:
			return builder.CreateFSub(lhs, rhs);
		case resolve::PrimitiveOp::Mul:
			return builder.CreateFMul(lhs, rhs);
		case resolve::PrimitiveOp::Div:
			return builder.CreateFDiv(lhs, rhs);
		case resolve::PrimitiveOp::Rem:
			return builder.CreateFRem(lhs, rhs);
		default:
			FatalError("Unsupported primitive operator provided.");
            return nullptr;
	}
}

llvm::Value* Generator::genCase(resolve::CaseExpr& casee) {
    return nullptr;
}

}} // namespace athena::gen