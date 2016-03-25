package se.rimmer.athena.parser

import java.math.BigDecimal
import java.math.BigInteger
import java.util.*

interface Literal
data class IntLiteral(val i: BigInteger): Literal
data class RealLiteral(val f: BigDecimal): Literal
data class StringLiteral(val s: String): Literal

interface Type
data class UnitType(val v: Unit = Unit): Type
data class ConType(val con: String): Type
data class PtrType(val type: Type): Type
data class ArrayType(val type: Type): Type
data class GenType(val id: String): Type
data class TupType(val fields: List<TupleField>): Type
data class FunType(val types: List<Type>): Type
data class AppType(val base: Type, val apps: List<Type>): Type

interface Expr
data class UnitExpr(val v: Unit = Unit): Expr
data class NestedExpr(val expr: Expr): Expr
data class BlockExpr(val block: List<Expr>): Expr
data class LitExpr(val literal: Literal): Expr
data class VarExpr(val name: String): Expr
data class AppExpr(val callee: Expr, val args: Expr): Expr
data class LamExpr(val args: List<TupleField>?, val body: Expr, val isCase: Boolean): Expr
data class InfixExpr(var lhs: Expr, var rhs: Expr, val op: String): Expr {var ordered = false}
data class PrefixExpr(val dst: Expr, val op: String): Expr
data class IfExpr(val cond: Expr, val then: Expr, val otherwise: Expr?): Expr
data class MultiIfExpr(val cases: List<IfCase>): Expr
data class DeclExpr(val name: String, val content: Expr?, val constant: Boolean): Expr
data class WhileExpr(val cond: Expr, val body: Expr): Expr
data class AssignExpr(val target: Expr, val value: Expr): Expr
data class CoerceExpr(val target: Expr, val type: Type): Expr
data class FieldExpr(val target: Expr, val field: Expr): Expr
data class ConstructExpr(val type: Type, val args: Expr): Expr
data class TupExpr(val args: List<TupleField>): Expr
data class FormatExpr(val chunks: List<FormatChunk>): Expr
data class CaseExpr(val pivot: Expr, val alts: List<CaseAlt>): Expr

interface Pattern
data class VarPattern(val name: String, val asVar: String?): Pattern
data class LitPattern(val lit: Literal, val asVar: String?): Pattern
data class AnyPattern(val v: Unit = Unit, val asVar: String? = null): Pattern
data class TupPattern(val fields: List<FieldPat>, val asVar: String?): Pattern
data class ConPattern(val constructor: String, val patterns: List<Pattern>, val asVar: String?): Pattern

interface Decl
data class FunDecl(val name: String, val body: Expr?, val cases: List<FunCase>?, val args: List<TupleField>, val ret: Type?): Decl {val locals = ArrayList<Decl>()}
data class TypeDecl(val type: SimpleType, val target: Type): Decl
data class DataDecl(val type: SimpleType, val constructors: List<Constructor>): Decl
data class ForeignDecl(val importName: String, val importedName: String, val type: Type, val cconv: ForeignConvention): Decl

enum class ForeignConvention {
    C,
    Stdcall,
    Cpp,
    JS,
    JVM
}

enum class Fixity {
    Left, Right, Prefix
}

data class OpFixity(val fixity: Fixity, val precedence: Int)

data class FieldPat(val field: String?, val pat: Pattern)
data class CaseAlt(val pattern: Pattern, val body: Expr)
data class FormatChunk(val string: String, val format: Expr?)
data class IfCase(val cond: Expr?, val then: Expr)
data class FunCase(val patterns: List<Pattern>, val body: Expr)

data class TupleField(val type: Type?, val name: String?, val default: Expr?)
data class SimpleType(val name: String, val kind: List<String>)

data class Constructor(val name: String, val types: List<Type>)
data class Qualified(var name: String, var qualifier: Qualified?)
data class Import(val source: Qualified, val name: String)

class Module(val name: String) {
    val imports = ArrayList<Import>()
    val declarations = ArrayList<Decl>()
    val operators = HashMap<String, OpFixity>()
}