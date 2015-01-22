
#define __STDC_CONSTANT_MACROS
#define __STDC_FORMAT_MACROS
#define __STDC_LIMIT_MACROS

#include <llvm/IR/Module.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/GenericValue.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/ExecutionEngine/Interpreter.h>
#include <core.h>
#include <Terminal.h>
#include "Parse/parser.h"

void CreateAddFunc(llvm::LLVMContext& context, llvm::Module* module)
{
	llvm::IRBuilder<> builder{context};
	auto c4 = llvm::ConstantFP::get(context, llvm::APFloat(4.f));

	llvm::Type* params[] = {builder.getFloatTy(), builder.getFloatTy()};
	auto funcType = llvm::FunctionType::get(builder.getFloatTy(), {params}, false);
	auto addFunc = llvm::Function::Create(funcType, llvm::Function::ExternalLinkage, "add", module);
	auto add = llvm::BasicBlock::Create(context, "", addFunc);
	builder.SetInsertPoint(add);

//	builder.CreateAlloca(<#(llvm::Type *)Ty#>, <#(llvm::Value *)ArraySize#>, <#(llvm::Twine const &)Name#>)
//	builder.CreateStore(<#(llvm::Value *)Val#>, <#(llvm::Value *)Ptr#>, <#(bool)isVolatile#>)

	//builder.CreateAdd(addFunc->)
}

void CoreMain(Core::ArgList& args)
{
	Core::Terminal.Show();
	athena::ast::CompileContext context;

	auto test = R"s(
f :: z =
    let x = 5
        y = 6
    x + y + z

g = f 5

h = if g > 0
    then 0
    else 1

loop_test =
    var i = 0
    while i < 10 do ++i
    i = 20
)s";

	athena::ast::Module module;
	athena::ast::Parser p(context, module, test);
	p.parseModule();
	Core::Terminal << athena::ast::toString(module, context);

	Core::Terminal.WaitForInput();

	/*llvm::LLVMContext context;
	auto module = std::make_unique<llvm::Module>("top", context);
	llvm::IRBuilder<> builder{context};

	auto funcType = llvm::FunctionType::get(builder.getVoidTy(), false);
	auto mainFunc = llvm::Function::Create(funcType, llvm::Function::ExternalLinkage, "main", module.get());
	auto entry = llvm::BasicBlock::Create(context, "entrypoint", mainFunc);
	builder.SetInsertPoint(entry);



	llvm::Type* putsArgs[] = {builder.getInt8Ty()->getPointerTo()};
	llvm::ArrayRef<llvm::Type*> argsRef{putsArgs};

	auto helloworld = builder.CreateGlobalStringPtr("hello world!\n");
	auto putsType = llvm::FunctionType::get(builder.getInt32Ty(), argsRef, false);
	auto putsFunc = module->getOrInsertFunction("puts", putsType);

	builder.CreateCall(putsFunc, helloworld);
	builder.CreateRetVoid();*/

	/*llvm::InitializeNativeTarget();
	llvm::InitializeNativeTargetAsmPrinter();

	std::string errors;
	auto engine = llvm::EngineBuilder(std::move(module)).setEngineKind(llvm::EngineKind::Interpreter).setErrorStr(&errors).create();
	engine->runFunction(mainFunc, {});*/
	//module->dump();
}