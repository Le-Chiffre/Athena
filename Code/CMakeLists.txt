cmake_minimum_required(VERSION 2.8.4)
project(Athena)

include_directories(../../llvm/include)
include_directories(../../Tritium/Code/Tritium/Core)
link_directories(../../Tritium/Debug)
link_directories(../../llvm/build/lib)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -msse4.1 -mfpmath=sse -Wall")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -msse4.1 -mfpmath=sse -Wall -std=c++14 -fno-rtti -fno-exceptions -fno-threadsafe-statics -dl -pthread")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -D_DEBUG -DDEBUG")
set(CMAKE_LINKER_FLAGS "${CMAKE_LINKER_FLAGS}")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/../Bin/Linux")

set(SOURCE_FILES
    main.cpp

    Parse/ast.h
    Parse/ast_print.cpp
    Parse/lexer.h
    Parse/lexer.cpp
    Parse/parser.h
    Parse/parser.cpp

    Resolve/resolve_call.cpp
    Resolve/resolve_expression.cpp
    Resolve/resolve_function.cpp
    Resolve/resolve_primitive.cpp
    Resolve/resolve_type.cpp
    Resolve/resolve_ast.cpp
    Resolve/resolve_ast.h
    Resolve/resolve.h
    Resolve/resolve.cpp
    Resolve/mangle.h
    Resolve/mangle.cpp
    Resolve/typecheck.cpp
    Resolve/typecheck.h

    Generate/generate.cpp
    Generate/generate.h
    General/types.h
    General/targets.h
    General/maybe.h
    General/compiler.h
    General/mem.h
    General/mem.cpp
    General/string.h
    Parse/context.h
    General/hash.h
    General/hash.cpp
    General/compiler.cpp
    General/map.h
    General/pool.h
    General/array.h)


add_executable(Athena ${SOURCE_FILES} Resolve/resolve_create.cpp)
target_link_libraries(Athena
LLVMInstrumentation LLVMInterpreter LLVMTableGen LLVMXCoreDisassembler LLVMXCoreCodeGen LLVMXCoreDesc LLVMXCoreInfo LLVMXCoreAsmPrinter LLVMSystemZDisassembler
LLVMSystemZCodeGen LLVMSystemZAsmParser LLVMSystemZDesc LLVMSystemZInfo LLVMSystemZAsmPrinter LLVMSparcDisassembler LLVMSparcCodeGen LLVMSparcAsmParser
LLVMSparcDesc LLVMSparcInfo LLVMSparcAsmPrinter LLVMPowerPCDisassembler
LLVMPowerPCCodeGen LLVMPowerPCAsmParser LLVMPowerPCDesc LLVMPowerPCInfo LLVMPowerPCAsmPrinter LLVMNVPTXCodeGen LLVMNVPTXDesc LLVMNVPTXInfo
LLVMNVPTXAsmPrinter LLVMMSP430CodeGen LLVMMSP430Desc LLVMMSP430Info LLVMMSP430AsmPrinter LLVMMipsDisassembler LLVMMipsCodeGen LLVMMipsAsmParser
LLVMMipsDesc LLVMMipsInfo LLVMMipsAsmPrinter LLVMHexagonDisassembler LLVMHexagonCodeGen LLVMHexagonDesc LLVMHexagonInfo
LLVMARMDisassembler LLVMARMCodeGen LLVMARMAsmParser LLVMARMDesc LLVMARMInfo LLVMARMAsmPrinter LLVMAArch64Disassembler LLVMAArch64CodeGen
LLVMAArch64AsmParser LLVMAArch64Desc LLVMAArch64Info LLVMAArch64AsmPrinter LLVMAArch64Utils LLVMLTO LLVMObjCARCOpts LLVMLinker LLVMipo LLVMVectorize
LLVMBitWriter LLVMX86Disassembler LLVMX86AsmParser LLVMMCJIT LLVMRuntimeDyld LLVMExecutionEngine LLVMOption gtest_main gtest LLVMX86CodeGen
LLVMX86Desc LLVMX86Info LLVMMCDisassembler LLVMX86AsmPrinter LLVMX86Utils LLVMSelectionDAG LLVMAsmPrinter LLVMCodeGen LLVMTarget LLVMScalarOpts
LLVMProfileData LLVMInstCombine LLVMTransformUtils LLVMAnalysis LLVMLineEditor LLVMIRReader LLVMAsmParser LLVMObject
LLVMMCParser LLVMMC LLVMBitReader LLVMCore LLVMSupport LLVMDemangle dl pthread)