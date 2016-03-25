package se.rimmer.athena.resolve

import se.rimmer.athena.parser.Fixity
import se.rimmer.athena.parser.OpFixity
import se.rimmer.athena.parser.Expr as ASTExpr
import se.rimmer.athena.parser.UnitExpr as ASTUnitExpr
import se.rimmer.athena.parser.NestedExpr as ASTNestedExpr
import se.rimmer.athena.parser.BlockExpr as ASTBlockExpr
import se.rimmer.athena.parser.LitExpr as ASTLitExpr
import se.rimmer.athena.parser.VarExpr as ASTVarExpr
import se.rimmer.athena.parser.AppExpr as ASTAppExpr
import se.rimmer.athena.parser.LamExpr as ASTLamExpr
import se.rimmer.athena.parser.InfixExpr as ASTInfixExpr
import se.rimmer.athena.parser.PrefixExpr as ASTPrefixExpr
import se.rimmer.athena.parser.IfExpr as ASTIfExpr
import se.rimmer.athena.parser.MultiIfExpr as ASTMultiIfExpr
import se.rimmer.athena.parser.DeclExpr as ASTDeclExpr
import se.rimmer.athena.parser.WhileExpr as ASTWhileExpr
import se.rimmer.athena.parser.AssignExpr as ASTAssignExpr
import se.rimmer.athena.parser.CoerceExpr as ASTCoerceExpr
import se.rimmer.athena.parser.FieldExpr as ASTFieldExpr
import se.rimmer.athena.parser.ConstructExpr as ASTConstructExpr
import se.rimmer.athena.parser.TupExpr as ASTTupExpr
import se.rimmer.athena.parser.FormatExpr as ASTFormatExpr
import se.rimmer.athena.parser.CaseExpr as ASTCaseExpr

fun Resolver.resolveExpression(scope: Scope, ast: ASTExpr, used: Boolean): Expr = when(ast) {
    is ASTUnitExpr -> EmptyExpr(types)
    is ASTBlockExpr -> resolveBlock(scope, ast, used)
    is ASTLitExpr -> LitExpr(ast.literal, types)
    is ASTAppExpr -> resolveCall(scope, ast)
    is ASTLamExpr -> resolveLambda(scope, ast)
    is ASTInfixExpr -> resolveInfix(scope, ast)
    is ASTPrefixExpr -> resolvePrefix(scope, ast)
    is ASTVarExpr -> resolveVar(scope, ast)
    is ASTIfExpr -> resolveIf(scope, ast, used)
    is ASTMultiIfExpr -> resolveMultiIf(scope, ast, used)
    is ASTDeclExpr -> resolveDecl(scope, ast)
    is ASTAssignExpr -> resolveAssign(scope, ast)
    is ASTWhileExpr -> resolveWhile(scope, ast)
    is ASTNestedExpr -> resolveNested(scope, ast, used)
    is ASTCoerceExpr -> resolveCoerce(scope, ast)
    is ASTFieldExpr -> resolveField(scope, ast)
    is ASTConstructExpr -> resolveConstruct(scope, ast)
    is ASTTupExpr -> resolveTuple(scope, ast)
    is ASTCaseExpr -> resolveCase(scope, ast, used)
    else -> throw ResolveException("unknown expression type $ast")
}

fun Resolver.resolveBlock(scope: Scope, ast: ASTBlockExpr, used: Boolean): Expr {
    // Expressions that are part of a statement list are never used, unless they are the last in the list.
    return MultiExpr(ast.block.mapLast({resolveExpression(scope, ast, false)}, {resolveExpression(scope, ast, used)}))
}

fun Resolver.resolveCall(scope: Scope, ast: ASTAppExpr): Expr {

}

fun Resolver.resolveBinaryCall(scope: Scope, op: String, lhs: Expr, rhs: Expr): Expr {

}

fun Resolver.resolveUnaryCall(scope: Scope, op: String, v: Expr): Expr {
    // Check if this can be a primitive operation.
    // Note that primitive operations can be both functions and operators.
    if(v.type.isPtrOrPrim()) {
        if(auto op = tryPrimitiveUnaryOp(function)) {
            // This means that built-in unary operators cannot be overloaded for any pointer or primitive type.
            if(*op >= PrimitiveOp::FirstUnary)
            if(auto e = resolvePrimitiveOp(scope, *op, target))
            return e;
        }
    }

    auto args = list(&target);

    // If the argument has an incomplete type, create a generic call.
    if(!target.type->resolved) {
        constrain(target.type, FunConstraint(function, 0));
        return build<GenAppExpr>(function, args, build<GenType>(0));
    }

    // Otherwise, create a normal function call.
    if(auto func = findFunction(scope, function, args)) {
        args->item = implicitCoerce(*args->item, func->arguments[0]->type);
        return build<AppExpr>(*func, args);
    } else {
        // No need for an error; this is done by findFunction.
        return nullptr;
    }
}

fun Resolver.resolveLambda(scope: Scope, ast: ASTLamExpr): Expr {

}

fun Resolver.resolveInfix(scope: Scope, ast: ASTInfixExpr): Expr {
    val e = if(ast.ordered) ast else reorderAst(ast)
    return resolveBinaryCall(scope, e.op,
        makeRV(resolveExpression(scope, e.lhs, true)),
        makeRV(resolveExpression(scope, e.rhs, true))
    )
}

fun Resolver.resolvePrefix(scope: Scope, ast: ASTPrefixExpr): Expr {
    return resolveUnaryCall(scope, ast.op, makeRV(resolveExpression(scope, ast.dst, true)))
}

fun Resolver.resolveVar(scope: Scope, ast: ASTVarExpr): Expr {

}

fun Resolver.resolveIf(scope: Scope, ast: ASTIfExpr, used: Boolean): Expr {

}

fun Resolver.resolveMultiIf(scope: Scope, ast: ASTMultiIfExpr, used: Boolean): Expr {

}

fun Resolver.resolveDecl(scope: Scope, ast: ASTDeclExpr): Expr {

}

fun Resolver.resolveAssign(scope: Scope, ast: ASTAssignExpr): Expr {

}

fun Resolver.resolveWhile(scope: Scope, ast: ASTWhileExpr): Expr {

}

fun Resolver.resolveNested(scope: Scope, ast: ASTNestedExpr, used: Boolean): Expr {

}

fun Resolver.resolveCoerce(scope: Scope, ast: ASTCoerceExpr): Expr {

}

fun Resolver.resolveField(scope: Scope, ast: ASTFieldExpr): Expr {

}

fun Resolver.resolveConstruct(scope: Scope, ast: ASTConstructExpr): Expr {

}

fun Resolver.resolveTuple(scope: Scope, ast: ASTTupExpr): Expr {

}

fun Resolver.resolveCase(scope: Scope, ast: ASTCaseExpr, used: Boolean): Expr {

}

fun Resolver.reorderAst(ast: ASTInfixExpr, minPrec: Int = 0): ASTInfixExpr {
    var lhs = ast
    while(lhs.rhs is ASTInfixExpr && !lhs.ordered) {
        val first = findOp(lhs.op)
        if(first.precedence < minPrec) break

        val rhs = lhs.rhs as ASTInfixExpr
        val second = findOp(rhs.op)
        if(second.precedence > first.precedence || (second.precedence == first.precedence && second.fixity == Fixity.Right)) {
            lhs.rhs = reorderAst(rhs, second.precedence)
            if(lhs.rhs === rhs) {
                lhs.ordered = true
                break
            }
        } else {
            lhs.ordered = true
            lhs.rhs = rhs.lhs
            rhs.lhs = lhs
            lhs = rhs
        }
    }
    return lhs
}