
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
#include <llvm/IR/Verifier.h>
#include <Core.h>
#include <iostream>
#include <fstream>
#include "Parse/parser.h"
#include "Resolve/resolve.h"
#include "Generate/generate.h"
#include <File/FileApi.h>

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

int main()
{
	athena::ast::CompileContext context{athena::ast::CompileSettings{athena::ast::CompileAthena}};

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

main {x: Float} =
	let a = {x, 6: Float}
	dot a {1: Float, 6: Float}
)s";

	auto test5 = R"s(
h [x: Int] = x*2
g [x: Int] = x*4
f [x: Int] = x*8
e = f $ g $ h 1
)s";

	auto foreigntest = R"s(
foreign import "strlen" stringLength : *U8 -> Int
foreign import "printf" print : *U8 -> Int

fluff = print "hello world!"
;)s";

	auto memtest = R"s(
copyMem [src *U8, dst *U8, count Int] =
	var p = 0
	while p < count ->
		src.*p = dst.*p
		p = p + 1

fillMem [mem *U8, count Int, pattern U8] =
	var p = 0
	while p < count ->
		*(mem+p) = pattern
		p = p + 1

zeroMem [mem *U8, count Int] = fillMem mem count 0

compareMem [a *U8, b *U8, count Int] =
	var p = 0
	while p < count `and` *(a+p) == *(b+p) -> p = p + 1
	*(a+p) - *(b+p)

strlen [str *U8] =
	var str, l = 0
	while *str > 0 ->
		str = str + 1
		l = l + 1
	l

parseNum [ch U8] = ch - 48

parseInt [string *U8] =
	var string
	var i = 0 : U32
	while *string != 0 ->
		i = i * 10 + parseNum (*string)
		string = string + 1
	i
)s";

	auto gentest = R"s(
id x = x
)s";

	athena::ast::Module module;
	athena::ast::Parser p(context, module, gentest);
	p.parseModule();

	{
        if(auto file = Tritium::File::createFile("ast.txt", {false, true}, Tritium::FileCreate::CreateAlways)) {
            auto string = athena::ast::toString(module, context).string();
            Tritium::File::writeFile(file.forceR(), string.text(), string.size());
        }
	}

	athena::resolve::Resolver resolver{context, module};
	auto resolved = resolver.resolve();

	llvm::LLVMContext& llcontext = llvm::getGlobalContext();
	llvm::Module* llmodule = new llvm::Module("top", llcontext);
	llmodule->setDataLayout("e-S128");
	llmodule->setTargetTriple(LLVM_HOST_TRIPLE);

	athena::gen::Generator gen{context, llcontext, *llmodule};
	gen.generate(*resolved);

	std::ofstream ss;
	ss.open("out.ll", std::ios::out | std::ios::trunc);
	llvm::raw_os_ostream stream{ss};
	llmodule->print(stream, nullptr);
	llvm::verifyModule(*llmodule, &stream);

    return 0;
}