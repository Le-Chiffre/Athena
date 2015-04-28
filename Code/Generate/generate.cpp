
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

Module* Generator::generate(resolve::Module& module) {
	module.functions.Iterate([=](resolve::Id name, resolve::FunctionDecl* f) {
		if(!f->codegen) genFunctionDecl(*f);
	});

	return &this->module;
}
	
Function* Generator::genFunctionDecl(resolve::FunctionDecl& function) {
	auto argCount = function.arguments.Count();
	auto argTypes = (Type**)StackAlloc(sizeof(Type*) * argCount);
	for(uint i=0; i<argCount; i++) {
		argTypes[i] = getType(function.arguments[i]->type);
	}
	
	auto type = FunctionType::get(getType(function.type), ArrayRef<Type*>(argTypes, argCount), false);
	if(function.isForeign) {
		auto &ff = (resolve::ForeignFunction&)function;
		auto func = Function::Create(type, Function::ExternalLinkage, toRef(ccontext.Find(ff.importName).name), &module);
		func->setCallingConv(getCconv(ff.cconv));
		function.codegen = func;
		return func;
	} else {
		auto func = Function::Create(type, Function::InternalLinkage, toRef(ccontext.Find(function.name).name), &module);
		func->setCallingConv(CallingConv::Fast);
		function.codegen = func;
		if(function.hasImpl) {
			genFunction(func, (resolve::Function&)function);
		}
		return func;
	}
}

void Generator::genFunction(Function* func, resolve::Function& function) {
	// Generate the function arguments.
	uint i=0;
	for(auto it=func->arg_begin(); i<function.arguments.Count(); i++, it++) {
		auto a = function.arguments[i];
		it->setName(toRef(ccontext.Find(a->name).name));
		a->codegen = it;
	}
	
	// Generate the function body.
	auto scope = genScope(function.scope);
	scope->insertInto(func);
	if(function.expression) {
		SaveInsert save{builder};
		builder.SetInsertPoint(scope);
		genExpr(*function.expression);
	}
}

BasicBlock* Generator::genScope(resolve::Scope& scope) {
	auto block = BasicBlock::Create(context, "");
	SaveInsert save{builder};
	builder.SetInsertPoint(block);
	// Generate the variables used in this scope (excluding function parameters).
	// Constant variables are generated lazily as registers.
	for(auto v : scope.variables) {
		if(v->isVar()) {
			auto var = builder.CreateAlloca(getType(v->type));
			v->codegen = var;
		}
	}
	return block;
}
	
Value* Generator::genExpr(resolve::ExprRef expr) {
	switch(expr.kind) {
		case resolve::Expr::Multi:
			return genMulti((resolve::MultiExpr&)expr);
		case resolve::Expr::Lit:
			return genLiteral(((resolve::LitExpr&)expr).literal);
		case resolve::Expr::Var:
			return genVar(*((resolve::VarExpr&)expr).var);
		case resolve::Expr::Load:
			return genLoad((resolve::Expr&)((resolve::LoadExpr&)expr).target);
		case resolve::Expr::Store:
			return genStore((resolve::StoreExpr&)expr);
		case resolve::Expr::App:
			return genCall(((resolve::AppExpr&)expr).callee, ((resolve::AppExpr&)expr).args);
		case resolve::Expr::AppI:
			return nullptr;
		case resolve::Expr::AppP:
			return genPrimitiveCall(((resolve::AppPExpr&)expr).op, ((resolve::AppPExpr&)expr).args);
        case resolve::Expr::Case:
            return genCase((resolve::CaseExpr&)expr);
		case resolve::Expr::If:
			return genIf((resolve::IfExpr&)expr);
		case resolve::Expr::While:
			return genWhile((resolve::WhileExpr&)expr);
		case resolve::Expr::Assign:
			return genAssign((resolve::AssignExpr&)expr);
		case resolve::Expr::Coerce:
			return genCoerce(((resolve::CoerceExpr&)expr).src, ((resolve::CoerceExpr&)expr).type);
		case resolve::Expr::Field:
			return genField((resolve::FieldExpr&)expr);
		case resolve::Expr::Ret:
			return genRet((resolve::RetExpr&)expr);
		case resolve::Expr::Construct:
			return genConstruct((resolve::ConstructExpr&)expr);
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
	
Value* Generator::genVar(resolve::Variable& var) {
	ASSERT(var.codegen != nullptr); // This could happen if a constant is used before its creation.
	return (Value*)var.codegen;
}
	
Value* Generator::genAssign(resolve::AssignExpr& assign) {
	ASSERT(!assign.target.isVar());
	auto e = genExpr(assign.value);
	assign.target.codegen = e;
	return e;
}

Value* Generator::genLoad(resolve::Expr& target) {
	return builder.CreateLoad(genExpr(target));
}

Value* Generator::genStore(resolve::StoreExpr& expr) {
	return builder.CreateStore(genExpr(expr.value), genExpr(expr.target));
}

Value* Generator::genMulti(resolve::MultiExpr& expr) {
	auto e = &expr;
	Value* v = nullptr;
	while(e) {
		v = genExpr(*e->expr);
		e = e->next;
	}
	return v;
}
	
Value* Generator::genRet(resolve::RetExpr& expr) {
	auto e = genExpr(expr.expr);
	return builder.CreateRet(e);
}

Value* Generator::genCall(resolve::FunctionDecl& function, resolve::ExprList* argList) {
	// Make sure this function has been generated.
	if(!function.codegen) {
		genFunctionDecl(function);
	}
	
	// Generate the function arguments.
	auto argCount = function.arguments.Count();
	auto args = (Value**)StackAlloc(sizeof(Value*) * argCount);
	for(uint i=0; i<argCount; i++) {
		args[i] = genExpr(*argList->item);
		argList = argList->next;
	}
	
	return builder.CreateCall((Value*)function.codegen, ArrayRef<Value*>{args, argCount});
}

Value* Generator::genPrimitiveCall(resolve::PrimitiveOp op, resolve::ExprList* args) {
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

Value* Generator::genUnaryOp(resolve::PrimitiveOp op, resolve::PrimType type, llvm::Value* in) {
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
		case resolve::PrimitiveOp::Deref:
			ASSERT(type.isPointer());
			return builder.CreateLoad(in);
		default:
			FatalError("Unsupported primitive operator provided.");
            return nullptr;
	}
}

Value* Generator::genBinaryOp(resolve::PrimitiveOp op, llvm::Value* lhs, llvm::Value* rhs, resolve::PrimType lt, resolve::PrimType rt) {
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

Value* Generator::genPtrOp(resolve::PrimitiveOp op, llvm::Value* lhs, llvm::Value* rhs, resolve::PtrType type) {
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

Value* Generator::genPtrOp(resolve::PrimitiveOp op, llvm::Value* lhs, llvm::Value* rhs, resolve::PtrType lt, resolve::PrimType rt) {
	ASSERT(op == resolve::PrimitiveOp::Sub || op == resolve::PrimitiveOp::Add);
	if(op == resolve::PrimitiveOp::Sub) {
		rhs = builder.CreateNeg(rhs);
	}
	return builder.CreateGEP(lhs, rhs);
}

Value* Generator::genCase(resolve::CaseExpr& casee) {
    return nullptr;
}

Value* Generator::genIf(resolve::IfExpr& ife) {
	ASSERT(ife.cond.type->isBool());

	// Create basic blocks for each branch.
	// Don't restore the previous one, as the next expression needs to be put in the continuation block.
	auto function = getFunction();
	auto thenBlock = BasicBlock::Create(context, "then", function);
	auto elseBlock = ife.otherwise ? BasicBlock::Create(context, "else", function) : nullptr;
	auto contBlock = BasicBlock::Create(context, "cont", function);

	// Create condition.
	auto cond = genExpr(ife.cond);
	builder.CreateCondBr(cond, thenBlock, elseBlock ? elseBlock : contBlock);

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
		// TODO: Return some kind of empty value.
		return nullptr;
	}
}

Value* Generator::genCoerce(const resolve::Expr& srce, resolve::Type* dst) {
	auto src = srce.type;
	auto llSrc = getType(src);
	auto llDst = getType(dst);
	auto expr = genExpr(srce);

	if(src == dst) return expr;

	if(src->isPointer() && dst->isPointer()) {
		// Pointer typecast.
		return builder.CreateBitCast(expr, llDst);
	} else if(dst->isPrimitive()) {
		auto d_typ = ((const resolve::PrimType*)dst)->type;

		if(src->isPointer()) {
			// Pointer to integer conversion.
			ASSERT(d_typ < resolve::PrimitiveType::FirstFloat);
			return builder.CreatePtrToInt(expr, llDst);
		}

		if(src->isPrimitive()) {
			// Primitive to primitive conversion.
			auto s_typ = ((const resolve::PrimType*)src)->type;
			if(d_typ < resolve::PrimitiveType::FirstFloat) {
				if(s_typ < resolve::PrimitiveType::FirstFloat)
					// Integer-to-integer cast.
					return builder.CreateIntCast(expr, llDst, s_typ < resolve::PrimitiveType::FirstUnsigned);

				if(s_typ < resolve::PrimitiveType::FirstOther) {
					// Float-to-integer cast.
					if(d_typ < resolve::PrimitiveType::FirstUnsigned) {
						return builder.CreateFPToSI(expr, llDst);
					} else {
						return builder.CreateFPToUI(expr, llDst);
					}
				}

				if(s_typ == resolve::PrimitiveType::Bool) {
					// Bool-to-integer cast.
					return builder.CreateZExt(expr, llDst);
				}
			} else if(d_typ < resolve::PrimitiveType::FirstOther) {
				if(s_typ < resolve::PrimitiveType::FirstUnsigned) {
					// Signed-to-float cast.
					return builder.CreateSIToFP(expr, llDst);
				} else if(s_typ < resolve::PrimitiveType::FirstFloat) {
					// Unsigned-to-float cast.
					return builder.CreateUIToFP(expr, llDst);
				} else if(s_typ < resolve::PrimitiveType::FirstOther) {
					// Float-to-float cast.
					return builder.CreateFPCast(expr, llDst);
				} else {
					// Bool-to-float cast.
					return builder.CreateSelect(expr, ConstantFP::get(llSrc, 1.f), ConstantFP::get(context, APFloat(0.f)));
				}
			} else if(d_typ == resolve::PrimitiveType::Bool) {
				if(s_typ < resolve::PrimitiveType::FirstFloat) {
					// Integer-to-Bool cast.
					return builder.CreateICmpNE(expr, ConstantInt::get(llSrc, 0));
				} else if(s_typ < resolve::PrimitiveType::FirstOther) {
					// Float-to-Bool cast.
					return builder.CreateFCmpONE(expr, ConstantFP::get(llSrc, 0.f));
				}
			}
		}
	} else if(dst->isPointer()) {
		// Integer to pointer conversion.
		auto s_typ = ((const resolve::PrimType*)src)->type;
		ASSERT(s_typ < resolve::PrimitiveType::FirstFloat);
	}

	FatalError("Invalid coercion between types.");
	return nullptr;
}

Value* Generator::genWhile(resolve::WhileExpr& expr) {
	ASSERT(expr.cond.type->isBool());

	// Create basic blocks for each branch.
	// Don't restore the previous one, as the next expression needs to be put in the continuation block.
	auto function = getFunction();
	auto testBlock = BasicBlock::Create(context, "test", function);
	auto loopBlock = BasicBlock::Create(context, "loop", function);
	auto contBlock = BasicBlock::Create(context, "cont", function);

	// Create condition.
	builder.SetInsertPoint(testBlock);
	auto cond = genExpr(expr.cond);
	builder.CreateCondBr(cond, loopBlock, contBlock);

	// Create loop branch.
	builder.SetInsertPoint(loopBlock);
	genExpr(expr.loop);
	builder.CreateBr(testBlock);

	// Continue in this block.
	builder.SetInsertPoint(contBlock);
	return nullptr;
}

Value* Generator::genField(resolve::FieldExpr& expr) {
	auto container = genExpr(expr.container);
	if(container->getType()->isPointerTy()) {
		return builder.CreateStructGEP(container, expr.field->index);
	} else {
		ASSERT(container->getType()->isAggregateType());
		return builder.CreateExtractValue(container, expr.field->index);
	}
}

Value* Generator::genConstruct(resolve::ConstructExpr& expr) {
	if(expr.type->isPtrOrPrim()) {
		return genCoerce(expr.args[0].expr, expr.type);
	} else if(expr.type->isTuple()) {
		auto type = getType(expr.type);
		Value* v = UndefValue::get(type);
		for(auto a : expr.args) {
			v = builder.CreateInsertValue(v, genExpr(a.expr), a.index);
		}
		return v;
	} else {
		DebugError("Not implemented");
	}
}

Type* Generator::genLlvmType(resolve::TypeRef type) {
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
	} else if(type->isPointer()) {
		auto llType = getType(((const resolve::PtrType*)type)->type);
		return PointerType::getUnqual(llType);
	} else if(type->isTuple()) {
		// Each tuple type is unique, so we can always generate and put it here.
		auto tuple = (resolve::TupleType*)type;

		// Generate the tuple contents.
		auto fCount = tuple->fields.Count();
		auto fields = (Type**)StackAlloc(sizeof(Type*) * fCount);
		for(uint i=0; i<fCount; i++) {
			fields[i] = getType(tuple->fields[i].type);
		}

		auto llType = StructType::create(context, {fields, fCount});
		return llType;
	}

	FatalError("Unsupported type.");
	return nullptr;
}

}} // namespace athena::gen