package se.rimmer.athena.resolve

enum class Prim {
    Signed,
    Unsigned,
    Half,
    Float,
    Double,
    Bool
}

enum class PrimOp {
    // Binary
    Add,
    Sub,
    Mul,
    Div,
    Rem,

    Shl,
    Shr,
    And,
    Or,
    Xor,

    CmpEq,
    CmpNeq,
    CmpGt,
    CmpGe,
    CmpLt,
    CmpLe,

    // Unary
    Neg,
    Not,
    Ref,
    Deref;

    /// Returns true if this operation takes exactly one parameter.
    val isUnary: Boolean get() = this >= Neg

    /// Returns true if this operation takes exactly two parameters.
    val isBinary: Boolean get() = this < Neg

    /// Returns true if this operation is logical and/or.
    val isAndOr: Boolean get() = this == And || this == Or
}

val primitiveBinaryMap = mapOf(
    "+" to PrimOp.Add,
    "-" to PrimOp.Sub,
    "*" to PrimOp.Mul,
    "/" to PrimOp.Div,
    "mod" to PrimOp.Rem,
    "shl" to PrimOp.Shl,
    "shr" to PrimOp.Shr,
    "and" to PrimOp.And,
    "or" to PrimOp.Or,
    "xor" to PrimOp.Xor,
    "==" to PrimOp.CmpEq,
    "!=" to PrimOp.CmpNeq,
    ">" to PrimOp.CmpGt,
    ">=" to PrimOp.CmpGe,
    "<" to PrimOp.CmpLt,
    "<=" to PrimOp.CmpLe
)

val primitiveUnaryMap = mapOf(
    "-" to PrimOp.Neg,
    "not" to PrimOp.Not,
    "&" to PrimOp.Ref,
    "*" to PrimOp.Deref
)
