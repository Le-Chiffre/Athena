
#define __STDC_CONSTANT_MACROS
#define __STDC_FORMAT_MACROS
#define __STDC_LIMIT_MACROS

#include <llvm/IR/Module.h>
#include "generate.h"

using namespace llvm;

namespace athena {
namespace gen {

Generator::Generator(ast::CompileContext& ccontext, llvm::LLVMContext& context, llvm::Module& target) :
	context(context), module(target), builder(context), ccontext(ccontext) {}

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
		case resolve::Expr::If:
			return genIf((resolve::IfExpr&)expr);
        default:
            FatalError("Unsupported expression type.");
            return nullptr;
	}
}

Value* Generator::genLiteral(resolve::Literal& literal) {
	switch(literal.type) {
		case resolve::Literal::Float: return ConstantFP::get(builder.getFloatTy(), literal.f);
		case resolve::Literal::Int: return ConstantInt::get(builder.getInt32Ty(), literal.i);
		case resolve::Literal::Char: return ConstantInt::get(builder.getInt8Ty(), literal.c);
		case resolve::Literal::String: {
			auto str = ccontext.Find(literal.s).name;
			return builder.CreateGlobalStringPtr(StringRef(str.ptr, str.length));
		}
	}

	FatalError("Unsupported literal type.");
	return nullptr;
}

llvm::Value* Generator::genCall(resolve::Function& function, resolve::ExprList* args) {
    return nullptr;
}

llvm::Value* Generator::genPrimitiveCall(resolve::PrimitiveOp op, resolve::ExprList* args) {
	if(resolve::isBinary(op)) {
		ASSERT(args && args->next && !args->next->next);
		auto lhs = args->item;
		auto rhs = args->next->item;
		auto le = genExpr(*lhs);
		auto re = genExpr(*rhs);
		if(lhs->type->isPointer()) {
			if(rhs->type->isPointer()) {
				return genPtrOp(op, le, re, *(const resolve::PtrType*)lhs->type);
			} else {
				return genPtrOp(op, le, re, *(const resolve::PtrType*)lhs->type, *(const resolve::PrimType*)rhs->type);
			}
		} else {
			return genBinaryOp(op, le, re, *(const resolve::PrimType*)lhs->type, *(const resolve::PrimType*)rhs->type);
		}
	} else if(resolve::isUnary(op)) {
		ASSERT(args && !args->next);
		return genUnaryOp(op, *(const resolve::PrimType*)args->item->type, genExpr(*args->item));
	} else {
		FatalError("Unsupported primitive operator provided");
        return nullptr;
	}
}

llvm::Value* Generator::genUnaryOp(resolve::PrimitiveOp op, resolve::PrimType type, llvm::Value* in) {
	switch(op) {
		case resolve::PrimitiveOp::Neg:
			// Negate works for integers and floats, but not bool.
			// This is a bit of an arbitrary distinction, since negating an i1 is the same as inverting it.
			ASSERT(resolve::category(type.type) <= resolve::PrimitiveTypeCategory::Float);
			if(resolve::category(type.type) == resolve::PrimitiveTypeCategory::Float) {
				return builder.CreateFNeg(in);
			} else {
				return builder.CreateNeg(in);
			}
		case resolve::PrimitiveOp::Not:
			// Performs a bitwise inversion. Defined for integers and Bool.
			ASSERT(type.type == resolve::PrimitiveType::Bool || resolve::category(type.type) <= resolve::PrimitiveTypeCategory::Unsigned);
			return builder.CreateNot(in);
		default:
			FatalError("Unsupported primitive operator provided.");
            return nullptr;
	}
}

llvm::Value* Generator::genBinaryOp(resolve::PrimitiveOp op, llvm::Value* lhs, llvm::Value* rhs, resolve::PrimType lt, resolve::PrimType rt) {
	ASSERT(op < resolve::PrimitiveOp::FirstUnary);
	if(resolve::category(lt.type) == resolve::PrimitiveTypeCategory::Float) {
		// Handle floating point types.
		ASSERT(op < resolve::PrimitiveOp::FirstBit || op >= resolve::PrimitiveOp::FirstCompare);
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
			case resolve::PrimitiveOp::CmpEq:
				return builder.CreateFCmpOEQ(lhs, rhs);
			case resolve::PrimitiveOp::CmpNeq:
				return builder.CreateFCmpONE(lhs, rhs);
			case resolve::PrimitiveOp::CmpGt:
				return builder.CreateFCmpOGT(lhs, rhs);
			case resolve::PrimitiveOp::CmpLt:
				return builder.CreateFCmpOLT(lhs, rhs);
			case resolve::PrimitiveOp::CmpGe:
				return builder.CreateFCmpOGE(lhs, rhs);
			case resolve::PrimitiveOp::CmpLe:
				return builder.CreateFCmpOLE(lhs, rhs);
			default: ;
		}
	} else {
		// Handle integral types (Bool is also handled here).
		bool hasSign = resolve::category(lt.type) == resolve::PrimitiveTypeCategory::Signed;
		switch(op) {
			case resolve::PrimitiveOp::Add:
				return builder.CreateAdd(lhs, rhs);
			case resolve::PrimitiveOp::Sub:
				return builder.CreateSub(lhs, rhs);
			case resolve::PrimitiveOp::Mul:
				return builder.CreateMul(lhs, rhs);
			case resolve::PrimitiveOp::Div:
				if(hasSign) return builder.CreateSDiv(lhs, rhs);
				else return builder.CreateUDiv(lhs, rhs);
			case resolve::PrimitiveOp::Rem:
				if(hasSign) return builder.CreateSRem(lhs, rhs);
				else return builder.CreateURem(lhs, rhs);
			case resolve::PrimitiveOp::Shl:
				return builder.CreateShl(lhs, rhs);
			case resolve::PrimitiveOp::Shr:
				if(hasSign) return builder.CreateAShr(lhs, rhs);
				else return builder.CreateLShr(lhs, rhs);
			case resolve::PrimitiveOp::And:
				return builder.CreateAnd(lhs, rhs);
			case resolve::PrimitiveOp::Or:
				return builder.CreateOr(lhs, rhs);
			case resolve::PrimitiveOp::Xor:
				return builder.CreateXor(lhs, rhs);
			case resolve::PrimitiveOp::CmpEq:
				return builder.CreateICmpEQ(lhs, rhs);
			case resolve::PrimitiveOp::CmpNeq:
				return builder.CreateICmpNE(lhs, rhs);
			case resolve::PrimitiveOp::CmpGt:
				if(hasSign) return builder.CreateICmpSGT(lhs, rhs);
				else return builder.CreateICmpUGT(lhs, rhs);
			case resolve::PrimitiveOp::CmpLt:
				if(hasSign) return builder.CreateICmpSLT(lhs, rhs);
				else return builder.CreateICmpULT(lhs, rhs);
			case resolve::PrimitiveOp::CmpGe:
				if(hasSign) return builder.CreateICmpSGE(lhs, rhs);
				else return builder.CreateICmpUGE(lhs, rhs);
			case resolve::PrimitiveOp::CmpLe:
				if(hasSign) return builder.CreateICmpSLE(lhs, rhs);
				else return builder.CreateICmpULE(lhs, rhs);
			default: ;
		}
	}

	FatalError("Unsupported primitive operator provided.");
	return nullptr;
}

llvm::Value* Generator::genPtrOp(resolve::PrimitiveOp op, llvm::Value* lhs, llvm::Value* rhs, resolve::PtrType type) {
	ASSERT(op == resolve::PrimitiveOp::Sub || (op >= resolve::PrimitiveOp::FirstCompare && op < resolve::PrimitiveOp::FirstUnary));
	switch(op) {
		case resolve::PrimitiveOp::Sub:
			return builder.CreatePtrDiff(lhs, rhs);
		case resolve::PrimitiveOp::CmpEq:
			return builder.CreateICmpEQ(lhs, rhs);
		case resolve::PrimitiveOp::CmpNeq:
			return builder.CreateICmpNE(lhs, rhs);
		case resolve::PrimitiveOp::CmpGt:
			return builder.CreateICmpUGT(lhs, rhs);
		case resolve::PrimitiveOp::CmpLt:
			return builder.CreateICmpULT(lhs, rhs);
		case resolve::PrimitiveOp::CmpGe:
			return builder.CreateICmpUGE(lhs, rhs);
		case resolve::PrimitiveOp::CmpLe:
			return builder.CreateICmpULE(lhs, rhs);
		default: ;
	}
	return nullptr;
}

llvm::Value* Generator::genPtrOp(resolve::PrimitiveOp op, llvm::Value* lhs, llvm::Value* rhs, resolve::PtrType lt, resolve::PrimType rt) {
	ASSERT(op == resolve::PrimitiveOp::Sub || op == resolve::PrimitiveOp::Add);
	if(op == resolve::PrimitiveOp::Sub) {
		rhs = builder.CreateNeg(rhs);
	}
	return builder.CreateGEP(lhs, rhs);
}

llvm::Value* Generator::genCase(resolve::CaseExpr& casee) {
    return nullptr;
}

llvm::Value* Generator::genIf(resolve::IfExpr& ife) {
	ASSERT(ife.cond.type->isBool());

	// Create basic blocks for each branch.
	auto function = getFunction();
	auto thenBlock = BasicBlock::Create(context, "", function);
	auto elseBlock = ife.otherwise ? BasicBlock::Create(context, "", function) : nullptr;
	auto contBlock = BasicBlock::Create(context, "", function);

	// Create condition.
	auto cond = genExpr(ife.cond);
	auto cmp = builder.CreateICmpEQ(cond, builder.getInt1(true));
	builder.CreateCondBr(cmp, thenBlock, elseBlock ? elseBlock : contBlock);

	// Create "then" branch.
	builder.SetInsertPoint(thenBlock);
	auto thenValue = genExpr(ife.then);
	builder.CreateBr(contBlock);

	// Create "else" branch if needed.
	Value* elseValue = nullptr;
	if(ife.otherwise) {
		builder.SetInsertPoint(elseBlock);
		elseValue = genExpr(*ife.otherwise);
		builder.CreateBr(contBlock);
	}

	// Continue in this block.
	// If the expression returned a result, create a Phi node to capture it.
	// Otherwise, return a void value.
	// TODO: Should we explicitly use the stack for some types?
	builder.SetInsertPoint(contBlock);
	if(ife.returnResult && !ife.then.type->isUnit()) {
		ASSERT(ife.otherwise && ife.then.type == ife.otherwise->type);
		auto phi = builder.CreatePHI(getType(ife.then.type), 2);
		phi->addIncoming(thenValue, thenBlock);
		phi->addIncoming(elseValue, elseBlock);
		return phi;
	} else {
		return nullptr;
	}
}

llvm::Type* Generator::genLlvmType(resolve::TypeRef type) {
	if(type->isUnit()) return builder.getVoidTy();
	if(type->isPrimitive()) {
		switch(((const resolve::PrimType*)type)->type) {
			case resolve::PrimitiveType::I64: return builder.getInt64Ty();
			case resolve::PrimitiveType::I32: return builder.getInt32Ty();
			case resolve::PrimitiveType::I16: return builder.getInt16Ty();
			case resolve::PrimitiveType::I8 : return builder.getInt8Ty();

			case resolve::PrimitiveType::U64: return builder.getInt64Ty();
			case resolve::PrimitiveType::U32: return builder.getInt32Ty();
			case resolve::PrimitiveType::U16: return builder.getInt16Ty();
			case resolve::PrimitiveType::U8 : return builder.getInt8Ty();

			case resolve::PrimitiveType::F64: return builder.getDoubleTy();
			case resolve::PrimitiveType::F32: return builder.getFloatTy();
			case resolve::PrimitiveType::F16: return builder.getHalfTy();

			case resolve::PrimitiveType::Bool: return builder.getInt1Ty();
			default: FatalError("Unsupported primitive type."); return nullptr;
		}
	}

	FatalError("Unsupported type.");
	return nullptr;
}

}} // namespace athena::gen