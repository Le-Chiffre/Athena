
#include "genHLSL.h"

namespace athena {
namespace gen {

GenHLSL::GenHLSL(ast::CompileContext& ccontext) : ccontext(ccontext) {

}

String GenHLSL::generate(resolve::Module& module) {
    walk([=](resolve::Id name, resolve::FunctionDecl* f) {
        genFunctionDecl(*f);
    }, module.functions);

    declarations.append(functions.text(), functions.size());
    return declarations.string();
}

void GenHLSL::genFunction(resolve::Function& decl) {

}

void GenHLSL::genFunctionDecl(resolve::FunctionDecl& function) {

}

void GenHLSL::genScope(resolve::Scope& scope) {

}

String GenHLSL::genExpr(resolve::ExprRef expr) {

}

String GenHLSL::genLiteral(resolve::Literal& literal, resolve::TypeRef type) {

}

String GenHLSL::genVar(resolve::Variable& var) {

}

String GenHLSL::genAssign(resolve::AssignExpr& assign) {

}

String GenHLSL::genLoad(resolve::Expr& target) {

}

void GenHLSL::genStore(resolve::StoreExpr& expr) {
    StringBuilder builder;
    builder << genExpr(expr.target);
    builder << " = ";
    builder << genExpr(expr.value);
}

String GenHLSL::genMulti(resolve::MultiExpr& expr) {

}

void GenHLSL::genRet(resolve::RetExpr& expr) {

}

String GenHLSL::genCall(resolve::FunctionDecl& function, resolve::ExprList* args) {

}

String GenHLSL::genPrimitiveCall(resolve::PrimitiveOp op, resolve::ExprList* args) {

}

String GenHLSL::genUnaryOp(resolve::PrimitiveOp op, resolve::PrimType type, llvm::Value* in) {

}

String GenHLSL::genBinaryOp(resolve::PrimitiveOp op, llvm::Value* lhs, llvm::Value* rhs, resolve::PrimType lt, resolve::PrimType rt) {

}

String GenHLSL::genPtrOp(resolve::PrimitiveOp op, llvm::Value* lhs, llvm::Value* rhs, resolve::PtrType type) {

}

String GenHLSL::genPtrOp(resolve::PrimitiveOp op, llvm::Value* lhs, llvm::Value* rhs, resolve::PtrType lt, resolve::PrimType rt) {

}

String GenHLSL::genCase(resolve::CaseExpr& casee) {

}

String GenHLSL::genIf(resolve::IfExpr& ife) {

}

String GenHLSL::genCoerce(resolve::Expr& src, resolve::Type* dst) {

}

String GenHLSL::genCoerceLV(resolve::Expr& src, resolve::Type* dst) {

}

String GenHLSL::genWhile(resolve::WhileExpr& expr) {
    StringBuilder b;
    b << "while(";
    b << genExpr(expr.cond);
    b << ") {\n";
    b << genExpr(expr.loop);
    b << "\n}";
}

String GenHLSL::genField(resolve::FieldExpr& expr) {
    
}

String GenHLSL::genConstruct(resolve::ConstructExpr& expr) {

}

String GenHLSL::genScoped(resolve::ScopedExpr& expr) {

}

String GenHLSL::genLazyCond(resolve::PrimitiveOp op, resolve::ExprRef lhs, resolve::ExprRef rhs) {

}

void GenHLSL::genVarDecl(resolve::Variable& var) {

}

void GenHLSL::genType(StringBuilder& target, resolve::TypeRef type) {
    if(!type->codegen) {
        genTypeDecl(declarations, type);
    }

    if(type->isUnit()) {
        target << "void ";
    } else if(type->isPrimitive()) {
        switch(((const resolve::PrimType*)type)->type) {
            case resolve::PrimitiveType::I64:
            case resolve::PrimitiveType::I32:
            case resolve::PrimitiveType::I16:
            case resolve::PrimitiveType::I8 :
                target << "int ";
                break;
            case resolve::PrimitiveType::U64:
            case resolve::PrimitiveType::U32:
            case resolve::PrimitiveType::U16:
            case resolve::PrimitiveType::U8 :
                target << "uint ";
                break;
            case resolve::PrimitiveType::F64:
                target << "double ";
                break;
            case resolve::PrimitiveType::F32:
                target << "float ";
                break;
            case resolve::PrimitiveType::F16:
                target << "half ";
                break;
            case resolve::PrimitiveType::Bool:
                target << "bool ";
                break;
            default:
                fatalError("Unsupported primitive type.");
        }
    } else if(type->isTuple()) {
        auto index = (Size)type->codegen;
        Char buffer[64];
        auto length = Tritium::show(index, buffer, 63) - buffer;
        buffer[length++] = ' ';
        target << "Tuple_";
        target.append(buffer, length);
    } else {
        fatalError("Unsupported type for HLSL");
    }
}

void GenHLSL::genTypeDecl(StringBuilder& target, resolve::TypeRef type) {
    StringBuilder builder;
    if(type->isUnit()) {
        type->codegen = (void*)1;
    } else if(type->isPrimitive()) {
        type->codegen = (void*)1;
    } else if(type->isTuple()) {
        auto index = tupleCounter++;
        Char buffer[64];
        auto length = Tritium::show(index, buffer, 64) - buffer;
        builder << "struct Tuple_";
        builder.append(buffer, length);
        builder << " {\n";
        for(const resolve::Field& f : ((resolve::TupleType*)type)->fields) {
            genType(builder, f.type);
            builder << ccontext.Find(f.name).name;
            builder << ";\n";
        }
        builder << "};";
    } else {
        fatalError("Unsupported type for HLSL");
    }
}

}} // namespace athena::gen
