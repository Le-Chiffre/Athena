
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
#include <File.h>
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
main =
	let path = Path "hello.txt"
		display = path.display
	File.open path >>= readToString >>= print "{display} contains {str}"
	let x = "Time taken: {let time = timer.tick} ms ({1000/time} fps)."
(+) :: a b = a `add` b

cast :: a = truncate a : *I8

gg = var i : Int
	 i.x = 0

data X =
	var a : Y
	var b : Z
	let c = 0
)s";

	athena::ast::Module module;
	athena::ast::Parser p(context, module, test);
	p.parseModule();
	Core::File f;
	f.Open("ast.txt", {false, true}, Core::FileCreate::CreateAlways);
	auto str = athena::ast::toString(module, context);
	f.Write({str.ptr, str.length});

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