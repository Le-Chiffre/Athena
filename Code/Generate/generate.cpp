
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
	walk([=](resolve::Id name, resolve::FunctionDecl* f) {
		if(!f->codegen) genFunctionDecl(*f);
	}, module.functions);

	return &this->module;
}

Function* Generator::genFunctionDecl(resolve::FunctionDecl& function) {
	auto argCount = function.arguments.size();
	auto argTypes = (Type**)alloca(sizeof(Type*) * argCount);
	for(U32 i=0; i<argCount; i++) {
		argTypes[i] = getType(function.arguments[i]->type)->llType;
	}

	auto type = FunctionType::get(getType(function.type)->llType, ArrayRef<Type*>(argTypes, argCount), false);
	if(function.isForeign) {
		auto &ff = (resolve::ForeignFunction&)function;
		auto func = Function::Create(type, Function::ExternalLinkage, toRef(ccontext.Find(ff.importName).name), &module);
		func->setCallingConv(getCconv(ff.cconv));
		function.codegen = func;
		return func;
	} else {
		auto func = Function::Create(type, Function::ExternalLinkage, toRef(ccontext.Find(function.name).name), &module);
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
	U32 i=0;
	for(auto it=func->arg_begin(); i<function.arguments.size(); i++, it++) {
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
	auto block = BasicBlock::Create(context, "scope");
	SaveInsert save{builder};
	builder.SetInsertPoint(block);
	// Generate the variables used in this scope (excluding function parameters).
	// Constant variables are generated lazily as registers.
	for(auto v : scope.shadows) genVarDecl(*v);
	for(auto v : scope.variables) genVarDecl(*v);
	return block;
}

void Generator::genVarDecl(resolve::Variable& v) {
	auto type = getType(v.type);
	if(v.isVar() && !v.funParam) {
		auto var = builder.CreateAlloca(type->llType, nullptr, toRef(ccontext.Find(v.name).name));
		v.codegen = var;
	} else if(v.funParam && type->onStack) {
		auto var = builder.CreateAlloca(type->llType, nullptr, toRef(ccontext.Find(v.name).name));
		builder.CreateStore((Value*)v.codegen, var);
		v.codegen = var;
	}
}

Value* Generator::genExpr(resolve::ExprRef expr) {
	switch(expr.kind) {
		case resolve::Expr::Multi:
			return genMulti((resolve::MultiExpr&)expr);
		case resolve::Expr::Lit:
			return genLiteral(((resolve::LitExpr&)expr).literal, ((resolve::LitExpr&)expr).type);
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
		case resolve::Expr::CoerceLV:
			return genCoerceLV(((resolve::CoerceLVExpr&)expr).src, ((resolve::CoerceLVExpr&)expr).type);
		case resolve::Expr::Field:
			return genField((resolve::FieldExpr&)expr);
		case resolve::Expr::Ret:
			return genRet((resolve::RetExpr&)expr);
		case resolve::Expr::Construct:
			return genConstruct((resolve::ConstructExpr&)expr);
		case resolve::Expr::Scoped:
			return genScoped((resolve::ScopedExpr&)expr);
        default:
            fatalError("Unsupported expression type.");
            return nullptr;
	}
}

Value* Generator::genLiteral(resolve::Literal& literal, resolve::TypeRef type) {
	auto lltype = getType(type)->llType;
	switch(literal.type) {
		case resolve::Literal::Float: return ConstantFP::get(lltype, literal.f);
		case resolve::Literal::Int: return ConstantInt::get(lltype, literal.i);
		case resolve::Literal::Char: return ConstantInt::get(lltype, literal.c);
		case resolve::Literal::String: {
			auto str = ccontext.Find(literal.s).name;
			return builder.CreateGlobalStringPtr(StringRef(str.text(), str.size()));
		}
		case resolve::Literal::Bool: return ConstantInt::get(lltype, literal.i);
	}

	fatalError("Unsupported literal type.");
	return nullptr;
}

Value* Generator::genVar(resolve::Variable& var) {
	assert(var.codegen != nullptr); // This could happen if a constant is used before its creation.
	return (Value*)var.codegen;
}

Value* Generator::genAssign(resolve::AssignExpr& assign) {
	assert(!assign.target.isVar());
	auto e = genExpr(assign.value);

	// This should also work for stack types - in that case the pointer to the stack object is replaced.
	// This is not a problem, since assign-expressions are always executed before the variable is used.
	assign.target.codegen = e;
	return e;
}

Value* Generator::genLoad(resolve::Expr& target) {
	// Loads are just placeholders for generating lvalues.
	// The actual load is done when converting to rvalue.
	return genExpr(target);
}

Value* Generator::genStore(resolve::StoreExpr& expr) {
	assert(expr.target.type->isLvalue());
	auto target = genExpr(expr.target);
	builder.CreateStore(genExpr(expr.value), target);
	return target;
}

Value* Generator::genMulti(resolve::MultiExpr& expr) {
	Value* v = nullptr;
	for(auto i : expr.es)
		v = genExpr(*i);
	return v;
}

Value* Generator::genRet(resolve::RetExpr& expr) {
	auto e = genExpr(expr.expr);
	if(getType(expr.expr.type)->onStack) {
		e = builder.CreateLoad(e);
	}
	if(expr.type->isUnit())
		return builder.CreateRetVoid();
	else
		return builder.CreateRet(e);
}

Value* Generator::genCall(resolve::FunctionDecl& function, resolve::ExprList* argList) {
	// Make sure this function has been generated.
	if(!function.codegen) {
		genFunctionDecl(function);
	}

	// Generate the function arguments.
	auto argCount = function.arguments.size();
	auto args = (Value**)alloca(sizeof(Value*) * argCount);
	for(U32 i=0; i<argCount; i++) {
		args[i] = genExpr(*argList->item);
		if(getType(argList->item->type)->onStack) {
			args[i] = builder.CreateLoad(args[i]);
		}
		argList = argList->next;
	}

	auto f = (Function*)function.codegen;
	auto call = builder.CreateCall(f, ArrayRef<Value*>{args, argCount});
	call->setCallingConv(f->getCallingConv());
	return call;
}

Value* Generator::genPrimitiveCall(resolve::PrimitiveOp op, resolve::ExprList* args) {
	if(resolve::isBinary(op)) {
		assert(args && args->next && !args->next->next);
		auto lhs = args->item;
		auto rhs = args->next->item;

		// Special case for booleans with && and ||, where we use early-out.
		if(lhs->type->isBool() && rhs->type->isBool() && resolve::isAndOr(op)) {
			return genLazyCond(op, *lhs, *rhs);
		}

		// Create a general primitive operation.
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
		assert(args && !args->next);
		return genUnaryOp(op, *(const resolve::PrimType*)args->item->type, genExpr(*args->item));
	} else {
		fatalError("Unsupported primitive operator provided");
		return nullptr;
	}
}

Value* Generator::genUnaryOp(resolve::PrimitiveOp op, resolve::PrimType type, llvm::Value* in) {
	switch(op) {
		case resolve::PrimitiveOp::Neg:
			// Negate works for integers and floats, but not bool.
			// This is a bit of an arbitrary distinction, since negating an i1 is the same as inverting it.
            assert(resolve::category(type.type) <= resolve::PrimitiveTypeCategory::Float);
			if(resolve::category(type.type) == resolve::PrimitiveTypeCategory::Float) {
				return builder.CreateFNeg(in);
			} else {
				return builder.CreateNeg(in);
			}
		case resolve::PrimitiveOp::Not:
			// Performs a bitwise inversion. Defined for integers and Bool.
            assert(type.type == resolve::PrimitiveType::Bool || resolve::category(type.type) <= resolve::PrimitiveTypeCategory::Unsigned);
			return builder.CreateNot(in);
		case resolve::PrimitiveOp::Deref:
            assert(type.isPointer());
			return builder.CreateLoad(in);
		default:
			fatalError("Unsupported primitive operator provided.");
            return nullptr;
	}
}

Value* Generator::genBinaryOp(resolve::PrimitiveOp op, llvm::Value* lhs, llvm::Value* rhs, resolve::PrimType lt, resolve::PrimType rt) {
    assert(op < resolve::PrimitiveOp::FirstUnary);
	auto cat = resolve::category(lt.type);
	if(cat == resolve::PrimitiveTypeCategory::Float) {
		// Handle floating point types.
        assert(op < resolve::PrimitiveOp::FirstBit || op >= resolve::PrimitiveOp::FirstCompare);
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
		// Handle integral types and booleans.
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

	fatalError("Unsupported primitive operator provided.");
	return nullptr;
}

Value* Generator::genPtrOp(resolve::PrimitiveOp op, llvm::Value* lhs, llvm::Value* rhs, resolve::PtrType type) {
    assert(op == resolve::PrimitiveOp::Sub || (op >= resolve::PrimitiveOp::FirstCompare && op < resolve::PrimitiveOp::FirstUnary));
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
    assert(op == resolve::PrimitiveOp::Sub || op == resolve::PrimitiveOp::Add);
	if(op == resolve::PrimitiveOp::Sub) {
		rhs = builder.CreateNeg(rhs);
	}
	return builder.CreateGEP(lhs, rhs);
}

Value* Generator::genCase(resolve::CaseExpr& casee) {
    return nullptr;
}

Value* Generator::genIf(resolve::IfExpr& ife) {
	// If the chain always succeeds, we just execute the condition scopes and the then-branch.
	if(ife.alwaysTrue) {
		for(auto e : ife.conds) {
			if(e.scope) genExpr(*e.scope);
		}
		return genExpr(ife.then);
	}

	// Create basic blocks for each branch.
	// Don't restore the previous one, as the next expression needs to be put in the continuation block.
	auto function = getFunction();
	BasicBlock* thenBlock, *elseBlock, *contBlock, *nextCond;
	elseBlock = ife.otherwise ? BasicBlock::Create(context, "else", function) : nullptr;
	contBlock = BasicBlock::Create(context, "cont", function);
	if(ife.mode == resolve::CondMode::Or)
		thenBlock = BasicBlock::Create(context, "then", function);

	// Generate a chain of conditions.
	for(U32 i = 0;; i++) {
		auto c = &ife.conds[i];
		if(c->scope) genExpr(*c->scope);

		if(i == ife.conds.size() - 1) {
			if(c->cond) {
				auto gen = genExpr(*c->cond);
				thenBlock = BasicBlock::Create(context, "then", function);
				builder.CreateCondBr(gen, thenBlock, elseBlock ? elseBlock : contBlock);
			} else {
				thenBlock = builder.GetInsertBlock();
			}
			break;
		} else if(c->cond) {
			auto gen = genExpr(*c->cond);
			nextCond = BasicBlock::Create(context, "then", function);
			if(ife.mode == resolve::CondMode::Or) {
				builder.CreateCondBr(gen, thenBlock, nextCond);
			} else {
				builder.CreateCondBr(gen, nextCond, elseBlock ? elseBlock : contBlock);
			}
			builder.SetInsertPoint(nextCond);
		}
	}

	// Create "then" branch.
	// The code generated by the block may create blocks of its own,
	// which is why we retrieve the current block afterwards.
	builder.SetInsertPoint(thenBlock);
	auto thenValue = genExpr(ife.then);
	thenBlock = builder.GetInsertBlock();
	builder.CreateBr(contBlock);

	// Create "else" branch if needed.
	Value* elseValue = nullptr;
	if(ife.otherwise) {
		builder.SetInsertPoint(elseBlock);
		elseValue = genExpr(*ife.otherwise);
		elseBlock = builder.GetInsertBlock();
		builder.CreateBr(contBlock);
	}

	// Continue in this block.
	// If the expression returned a result, create a Phi node to capture it.
	// Otherwise, return a void value.
	// TODO: Should we explicitly use the stack for some types?
	builder.SetInsertPoint(contBlock);
	if(ife.returnResult && !ife.then.type->isUnit()) {
        assert(ife.otherwise && ife.then.type == ife.otherwise->type);
		auto phi = builder.CreatePHI(getType(ife.then.type)->llType, 2);
		phi->addIncoming(thenValue, thenBlock);
		phi->addIncoming(elseValue, elseBlock);
		return phi;
	} else {
		// TODO: Return some kind of empty value.
		return nullptr;
	}
}

Value* Generator::genCoerce(resolve::Expr& srce, resolve::Type* dst) {
	auto src = srce.type;
	auto llSrc = getType(src)->llType;
	auto llDst = getType(dst)->llType;
	auto expr = genExpr(srce);

	if(src == dst) return expr;

	if(src->isPointer() && dst->isPointer()) {
		// Pointer typecast.
		return builder.CreateBitCast(expr, llDst);
	} else if(dst->isPrimitive()) {
		auto d_typ = ((const resolve::PrimType*)dst)->type;

		if(src->isPointer()) {
			// Pointer to integer conversion.
            assert(d_typ < resolve::PrimitiveType::FirstFloat);
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
        assert(s_typ < resolve::PrimitiveType::FirstFloat);
	}

	fatalError("Invalid coercion between types.");
	return nullptr;
}

Value* Generator::genCoerceLV(resolve::Expr& src, resolve::Type* dst) {
    assert(src.type->isLvalue());
    assert(src.type->canonical == dst);

	// LValues are always implicitly pointers in the code generator.
	return builder.CreateLoad(genExpr(src));
}

Value* Generator::genWhile(resolve::WhileExpr& expr) {
    assert(expr.cond.type->isBool());

	// Create basic blocks for each branch.
	// Don't restore the previous one, as the next expression needs to be put in the continuation block.
	auto function = getFunction();
	auto testBlock = BasicBlock::Create(context, "test", function);

	// Create condition.
	builder.CreateBr(testBlock);
	builder.SetInsertPoint(testBlock);
	auto cond = genExpr(expr.cond);

	// We create these after the condition because the condition may produce blocks of its own.
	// Creating the blocks afterwards gives a more logical code order.
	auto loopBlock = BasicBlock::Create(context, "loop", function);
	auto contBlock = BasicBlock::Create(context, "cont", function);
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
	if(expr.container.type->isVariant()) {
		auto var = (resolve::VarType*)expr.container.type;
		if(expr.constructor == -1) {
			int index = ((VariantData*)getType(var)->data)->selectorIndex;
			if(index == -1) {
				return builder.getInt32(0);
			} else if(var->isEnum) {
				return builder.CreateCast(Instruction::ZExt, genExpr(expr.container), builder.getInt32Ty());
			} else {
				auto con = builder.CreateLoad(builder.CreateStructGEP(genExpr(expr.container), (U32)index));
				return builder.CreateCast(Instruction::ZExt, con, builder.getInt32Ty());
			}
		} else {
            assert(!var->isEnum);
			// Cast the variant data into a pointer to its constructor data.
			auto targetType = PointerType::getUnqual(getType(var->list[expr.constructor]->dataType)->llType);
			return builder.CreateLoad(builder.CreatePointerCast(genExpr(expr.container), targetType));
		}
	} else {
		auto container = genExpr(expr.container);
		if(container->getType()->isPointerTy()) {
			return builder.CreateStructGEP(container, expr.field->index);
		} else {
            assert(container->getType()->isAggregateType());
			return builder.CreateExtractValue(container, expr.field->index);
		}
	}
}

Value* Generator::genConstruct(resolve::ConstructExpr& expr) {
	if(expr.type->isPtrOrPrim()) {
		return genCoerce(expr.args[0].expr, expr.type);
	} else if(expr.type->isTuple()) {
		auto type = getType(expr.type)->llType;
		Value* v = UndefValue::get(type);
		for(auto a : expr.args) {
			v = builder.CreateInsertValue(v, genExpr(a.expr), a.index);
		}
		return v;
	} else if(expr.type->isVariant()) {
		auto var = (resolve::VarType*)expr.type;
		auto type = getType(expr.type);

		// There are several types of variants with differing semantics:
		// - Enum variants have multiple constructors without any data. They are represented as a single index.
		// - Single-constructor variants work just like the equivalent tuple, but are distinct types.
		// - General variants have multiple constructors with data. These are the most complex ones.
		if(var->isEnum) {
			// Enum variants are constructed as the index of their constructor.
			return ConstantInt::get(type->llType, expr.con->index, false);
		} else if(var->list.size() == 1) {
			// Single-constructor variants are constructed like the equivalent tuple.
			if(expr.args.size() > 1) {
				Value* v = UndefValue::get(type->llType);
				for(auto a : expr.args) {
					v = builder.CreateInsertValue(v, genExpr(a.expr), a.index);
				}
				return v;
			} else {
				return genExpr(expr.args[0].expr);
			}
		} else {
			// General variants are always stack-allocated, as they need to be bitcasted.
			auto pointer = builder.CreateAlloca(type->llType);
			auto stype = (StructType*)(type->llType);

			// Set the constructor index.
			auto last = stype->getStructNumElements() - 1;
			auto conIndex = ConstantInt::get(stype->getStructElementType(last), expr.con->index, false);
			auto idPointer = builder.CreateStructGEP(pointer, last);
			builder.CreateStore(conIndex, idPointer);

			// Set the constructor-specific data, if any.
			if(expr.args.size()) {
				auto conData = builder.CreatePointerCast(pointer, PointerType::getUnqual(((TypeData*)expr.con->codegen)->llType));
				if(expr.args.size() > 1) {
					for (auto &i : expr.args) {
						auto argPointer = builder.CreateStructGEP(conData, i.index);
						builder.CreateStore(genExpr(i.expr), argPointer);
					}
				} else {
					builder.CreateStore(genExpr(expr.args[0].expr), conData);
				}
			}

			return pointer;
		}
	} else {
		debugError("Not implemented");
		return nullptr;
	}
}

Value* Generator::genScoped(resolve::ScopedExpr& expr) {
	auto scope = genScope(expr.scope);

	// Discard the scope if it is empty.
	if(!scope->empty()) {
		scope->insertInto(getFunction());
		builder.CreateBr(scope);
		builder.SetInsertPoint(scope);
	}

	if (expr.contents) {
		return genExpr(*expr.contents);
	} else {
		return nullptr;
	}
}

Value* Generator::genLazyCond(resolve::PrimitiveOp op, resolve::ExprRef lhs, resolve::ExprRef rhs) {
	// Create basic blocks for each branch.
	// Don't restore the previous one, as the next expression needs to be put in the continuation block.
	auto function = getFunction();
	auto lhsBlock = builder.GetInsertBlock();
	auto rhsBlock = BasicBlock::Create(context, "cond.rhs", function);
	auto contBlock = BasicBlock::Create(context, "cond.cont", function);

	// Create lhs condition.
	auto left = genExpr(lhs);
	lhsBlock = builder.GetInsertBlock();
	if(op == resolve::PrimitiveOp::And) {
		// For and, we can early-out if the first condition is false.
		builder.CreateCondBr(left, rhsBlock, contBlock);
	} else {
		// For or, we can early-out if the first condition is true.
		builder.CreateCondBr(left, contBlock, rhsBlock);
	}

	// Create the rhs condition.
	builder.SetInsertPoint(rhsBlock);
	auto right = genExpr(rhs);
	rhsBlock = builder.GetInsertBlock();
	builder.CreateBr(contBlock);

	// Create the expression result.
	builder.SetInsertPoint(contBlock);
	auto result = builder.CreatePHI(left->getType(), 2);
	result->addIncoming(builder.getInt1(op == resolve::PrimitiveOp::Or), lhsBlock);
	result->addIncoming(right, rhsBlock);
	return result;
}

TypeData* Generator::genLlvmType(resolve::TypeRef type) {
	TypeData* data = new TypeData;
	data->data = nullptr;
	if(type->isUnit()) {data->llType = builder.getVoidTy(); return data;}
	if(type->isPrimitive()) {
		switch(((const resolve::PrimType*)type)->type) {
			case resolve::PrimitiveType::I64: data->llType = builder.getInt64Ty(); break;
			case resolve::PrimitiveType::I32: data->llType = builder.getInt32Ty(); break;
			case resolve::PrimitiveType::I16: data->llType = builder.getInt16Ty(); break;
			case resolve::PrimitiveType::I8 : data->llType = builder.getInt8Ty(); break;

			case resolve::PrimitiveType::U64: data->llType = builder.getInt64Ty(); break;
			case resolve::PrimitiveType::U32: data->llType = builder.getInt32Ty(); break;
			case resolve::PrimitiveType::U16: data->llType = builder.getInt16Ty(); break;
			case resolve::PrimitiveType::U8 : data->llType = builder.getInt8Ty(); break;

			case resolve::PrimitiveType::F64: data->llType = builder.getDoubleTy(); break;
			case resolve::PrimitiveType::F32: data->llType = builder.getFloatTy(); break;
			case resolve::PrimitiveType::F16: data->llType = builder.getHalfTy(); break;

			case resolve::PrimitiveType::Bool: data->llType = builder.getInt1Ty(); break;
			default: fatalError("Unsupported primitive type."); return nullptr;
		}
		return data;
	} else if(type->isPointer()) {
		auto llType = getType(((const resolve::PtrType*)type)->type);
		data->llType = PointerType::getUnqual(llType->llType);
		return data;
	} else if(type->isTuple()) {
		// Each tuple type is unique, so we can always generate and put it here.
		auto tuple = (resolve::TupleType*)type;

		// Generate the tuple contents.
		auto fCount = tuple->fields.size();
		auto fields = (Type**)alloca(sizeof(Type*) * fCount);
		for(U32 i=0; i<fCount; i++) {
			fields[i] = getType(tuple->fields[i].type)->llType;
		}

		data->llType = StructType::create(context, {fields, fCount}, "tuple");
		return data;
	} else if(type->isVariant()) {
		auto var = (resolve::VarType*)type;
		auto selectorTy = builder.getIntNTy(var->selectorBits);
		VariantData* varData = new VariantData;
		data->data = varData;

		if(var->isEnum) {
			varData->selectorIndex = 0;
			data->llType = selectorTy;
		} else {
			// Start by generating a type for each constructor.
			U32 totalSize = 0;
			U32 totalAlignment = 0;
			Type* baseType;
			U32 baseSize = 0;
			for (auto& con : var->list) {
				auto fCount = con->contents.size();
				if(fCount) {
					auto s = getType(con->dataType);
					auto dl = module.getDataLayout();
					auto align = dl->getPrefTypeAlignment(s->llType);
					auto size = dl->getTypeAllocSize(s->llType);
					if(align > totalAlignment || (align == totalAlignment && size > totalSize)) {
						totalAlignment = align;
						baseType = s->llType;
						baseSize = (U32)size;
					}

					totalSize = Tritium::Math::max(totalSize, (U32)size);
					con->codegen = s;
				} else {
					con->codegen = nullptr;
				}
			}

			if (var->list.size() == 1) {
				// Variants with a single constructor are represented as that constructor.
				data->llType = (Type*)var->list[0]->codegen;
				varData->selectorIndex = -1;
			} else {
				// Variants are represented as the type with the highest alignment and an array that fits the size of each constructor.
				if(totalSize == baseSize) {
					Type* fields[2];
					fields[0] = baseType;
					fields[1] = selectorTy;
					data->llType = StructType::create(context, {fields, 2}, "variant");
					varData->selectorIndex = 1;
				} else {
					Type* fields[3];
					fields[0] = baseType;
					fields[1] = ArrayType::get(builder.getInt32Ty(), (totalSize - baseSize + 3) / 4);
					fields[2] = selectorTy;
					data->llType = StructType::create(context, {fields, 3}, "variant");
					varData->selectorIndex = 2;
				}
				data->onStack = true;
			}
		}
		return data;
	}

	fatalError("Unsupported type.");
	return nullptr;
}

}} // namespace athena::gen