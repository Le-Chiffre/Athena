cmake_minimum_required(VERSION 2.8.4)
project(Athena)

include_directories(../../llvm/include)
include_directories(../../Core/Code/Include)
link_directories(../../Core/Bin/Win32)
link_directories(../../llvm/build/lib)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -msse4.1 -mfpmath=sse -Wall")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -msse4.1 -mfpmath=sse -Wall -std=c++14 -fno-rtti -fno-exceptions -fno-threadsafe-statics")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -D_DEBUG -DDEBUG")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static -mwindows -nostartfiles -Wl,-eWinMainCRTStartup")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/../Bin/Win32")

set(SOURCE_FILES
    main.cpp
    Parse/ast.h
    Parse/ast_print.cpp
    Parse/lexer.h
    Parse/lexer.cpp
    Parse/parser.h
    Parse/parser.cpp
    Resolve/resolve_ast.cpp
    Resolve/resolve_ast.h
    Resolve/resolve.h
    Resolve/resolve.cpp
    Resolve/mangle.h
    Resolve/mangle.cpp

    Generate/generate.cpp
    Generate/generate.h
)


add_executable(Athena ${SOURCE_FILES})
target_link_libraries(Athena
LLVMInstrumentation LLVMInterpreter LLVMTableGen LLVMXCoreDisassembler LLVMXCoreCodeGen LLVMXCoreDesc LLVMXCoreInfo LLVMXCoreAsmPrinter LLVMSystemZDisassembler
LLVMSystemZCodeGen LLVMSystemZAsmParser LLVMSystemZDesc LLVMSystemZInfo LLVMSystemZAsmPrinter LLVMSparcDisassembler LLVMSparcCodeGen LLVMSparcAsmParser
LLVMSparcDesc LLVMSparcInfo LLVMSparcAsmPrinter LLVMR600CodeGen LLVMR600AsmParser LLVMR600Desc LLVMR600Info LLVMR600AsmPrinter LLVMPowerPCDisassembler
LLVMPowerPCCodeGen LLVMPowerPCAsmParser LLVMPowerPCDesc LLVMPowerPCInfo LLVMPowerPCAsmPrinter LLVMNVPTXCodeGen LLVMNVPTXDesc LLVMNVPTXInfo
LLVMNVPTXAsmPrinter LLVMMSP430CodeGen LLVMMSP430Desc LLVMMSP430Info LLVMMSP430AsmPrinter LLVMMipsDisassembler LLVMMipsCodeGen LLVMMipsAsmParser
LLVMMipsDesc LLVMMipsInfo LLVMMipsAsmPrinter LLVMHexagonDisassembler LLVMHexagonCodeGen LLVMHexagonDesc LLVMHexagonInfo LLVMCppBackendCodeGen
LLVMCppBackendInfo LLVMARMDisassembler LLVMARMCodeGen LLVMARMAsmParser LLVMARMDesc LLVMARMInfo LLVMARMAsmPrinter LLVMAArch64Disassembler LLVMAArch64CodeGen
LLVMAArch64AsmParser LLVMAArch64Desc LLVMAArch64Info LLVMAArch64AsmPrinter LLVMAArch64Utils LLVMLTO LLVMObjCARCOpts LLVMLinker LLVMipo LLVMVectorize
LLVMBitWriter LLVMX86Disassembler LLVMX86AsmParser LLVMMCJIT LLVMRuntimeDyld LLVMExecutionEngine LLVMOption gtest_main gtest LLVMX86CodeGen
LLVMX86Desc LLVMX86Info LLVMMCDisassembler LLVMX86AsmPrinter LLVMX86Utils LLVMSelectionDAG LLVMAsmPrinter LLVMCodeGen LLVMTarget LLVMScalarOpts
LLVMProfileData LLVMInstCombine LLVMTransformUtils LLVMipa LLVMAnalysis LLVMLineEditor LLVMIRReader LLVMAsmParser LLVMDebugInfo LLVMObject
LLVMMCParser LLVMMC LLVMBitReader LLVMCore LLVMSupport
CoreD psapi ws2_32 shlwapi advapi32 DbgHelp)