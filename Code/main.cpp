
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
#include <llvm/Support/raw_os_ostream.h>
#include <core.h>
#include <Terminal.h>
#include <File.h>
#include <iostream>
#include <fstream>
#include "Parse/parser.h"
#include "Resolve/resolve.h"
#include "Generate/generate.h"

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
(+) : a b = a `add` b

cast : a = truncate a : *I8

test {a: Int, b: Int} = a + b

gg = var i : Int
	 i.x = 0

data X =
	var a : Y
	var b : Z
	let c = 0
)s";

	auto test2 = R"s(
one = 1 : Float
two = one + one
power {base: Int, exponent: Int} -> Int =
	if exponent == 0 then base
	else power (base * base) (exponent - 1)
add {a: Int, b: Int} = a + b
max {a: Int, b: Int} = if a > b then a else b
main = add 4 5 * max 100 200
)s";
	
	auto test3 = R"s(
main {a: Int, b: Int} =
	var x = a*b
	x = x*2
	if x > 5 then x = 5
	x
)s";

	auto test4 = R"s(
type Float2 = {x: Float, y: Float}

dot {a: Float2, b: Float2} =
	var res = a.x * b.x + a.y * b.y
	res
	
stuff {x: *Float2} = x.y
)s";
	
	auto test5 = R"s(
testfun {a: *Int} =
	let b = a + 1
	*b = 2
	*b
)s";

	athena::ast::Module module;
	athena::ast::Parser p(context, module, test5);
	p.parseModule();

	{
		Core::File f;
		f.Open("ast.txt", {false, true}, Core::FileCreate::CreateAlways);
		auto str = athena::ast::toString(module, context);
		f.Write({str.ptr, str.length});
	}

	athena::resolve::Resolver resolver{context, module};
	auto resolved = resolver.resolve();

	Core::Terminal << athena::ast::toString(module, context);

	llvm::LLVMContext& llcontext = llvm::getGlobalContext();
	llvm::Module* llmodule = new llvm::Module("top", llcontext);

	athena::gen::Generator gen{context, llcontext, *llmodule};
	gen.generate(*resolved);

	std::ofstream ss;
	ss.open("out.ll", std::ios::out | std::ios::trunc);
	llvm::raw_os_ostream stream{ss};
	llmodule->print(stream, nullptr);

	Core::Terminal.WaitForInput();
}