package se.rimmer.athena.resolve

import se.rimmer.athena.parser.IntLiteral
import se.rimmer.athena.parser.Literal
import se.rimmer.athena.parser.RealLiteral
import se.rimmer.athena.parser.StringLiteral
import java.util.*

class TypeManager {
    fun ptr(content: Type) = ptrs.getOrPut(content) {PtrType(content)}
    fun prim(prim: Prim, bits: Int) = primMap.getOrPut(prim to bits) {PrimType(prim, bits)}
    fun bool() = prim(Prim.Bool, 1)
    fun float() = prim(Prim.Float, 32)
    fun double() = prim(Prim.Double, 64)
    fun int() = prim(Prim.Signed, 32)
    fun string() = ptr(prim(Prim.Unsigned, 8))
    fun u8() = prim(Prim.Unsigned, 8)

    fun lv(t: Type) = lvalues.getOrPut(t) {LVType(t)}
    fun rv(t: Type): Type = if(t is LVType) rv(t.type) else t

    fun tupleFromFields(fields: List<Field>): Type {
        val name = StringBuilder()
        for(f in fields) {
            name.append(f.type.hashCode())
            if(f.name != null) name.append(f.name)
        }

        return tuples.getOrPut(name.toString()) {
            TupleType(fields.map {Field(it.name, it.index, it.type, it.container, it.content, it.constant)})
        }
    }

    fun tupleFromTypes(types: List<Type>): Type {
        val name = StringBuilder()
        for(t in types) {
            name.append(t.hashCode())
        }

        return tuples.getOrPut(name.toString()) {
            val list = ArrayList<Field>()
            val type = TupleType(list)
            list.addAll(types.mapIndexed {i, t -> Field(null, i, t, type, null, true)})
            type
        }
    }

    fun literal(lit: Literal) = when(lit) {
        is IntLiteral -> int()
        is RealLiteral -> float()
        is StringLiteral -> string()
        else -> throw ResolveException("unknown literal type")
    }

    val primMap = HashMap<Pair<Prim, Int>, Type>()
    val ptrs = HashMap<Type, PtrType>()
    val tuples = HashMap<String, TupleType>()
    val lvalues = HashMap<Type, LVType>()

    val unitType = UnitType()
    val unknownType = UnknownType()
}