package se.rimmer.athena.resolve

import se.rimmer.athena.parser.DataDecl
import java.util.*

abstract class Type {
    var codegen: Any? = null
    var canonical: Type = this
}

class UnitType: Type()
class UnknownType: Type()

/// A primitive type, where a primitive is a "native" type that represents a raw number in some form.
/// Note that pointers are separate types, because they contain additional information about what they point to.
class PrimType(val prim: Prim, val bits: Int): Type()

/// A pointer type representing a memory address to some data type.
/// Pointers must have a concrete type; pointers to the unit type are not supported.
class PtrType(val type: Type): Type()

/// Represents an lvalue (as opposed to rvalue).
class LVType(val type: Type): Type()

class TupleType(val fields: List<Field>): Type()
class Field(val name: String?, val index: Int, val type: Type, val container: Type, val content: Expr?, val constant: Boolean): Type()

class VarType(val name: String, val ast: DataDecl): Type() {
    val genericCount: Int get() = ast.type.kind.size
    val constructors = ArrayList<VarConstructor>()
}

class VarConstructor(val name: String, val index: Int, val parentType: Type, val ast: List<Type>) {
    var dataType: Type? = null
    var codegen: Any? = null
    val contents = ArrayList<Type>()
}
