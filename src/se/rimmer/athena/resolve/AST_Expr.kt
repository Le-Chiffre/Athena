package se.rimmer.athena.resolve

import se.rimmer.athena.parser.Literal

abstract class Expr(val type: Type) {
    var codegen: Any? = null
}

class EmptyExpr(types: TypeManager): Expr(types.unitType)
class MultiExpr(val list: List<Expr>): Expr(list.last().type)
class LitExpr(val lit: Literal, types: TypeManager): Expr(types.literal(lit))
class VarExpr(val v: Variable, type: Type): Expr(type)
class AppExpr(val callee: FunctionRef, val args: List<Expr>): Expr(callee.returnType)
class AppPExpr(val op: PrimOp, val args: List<Expr>, type: Type): Expr(type)

class CoerceExpr(val src: Expr, dst: Type): Expr(dst)
class CoerceLVExpr(val src: Expr, dst: Type): Expr(dst)
class RetExpr(val expr: Expr): Expr(expr.type)

enum class CondMode {And, Or}
class IfCond(val scope: Expr?, val cond: Expr?)

class IfExpr(
    val conds: List<IfCond>,
    val mode: CondMode,
    val then: Expr,
    val otherwise: Expr?,
    type: Type,
    val returnResult: Boolean,
    val alwaysTrue: Boolean
): Expr(type)
