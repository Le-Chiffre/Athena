package se.rimmer.athena.resolve

import se.rimmer.athena.parser.Fixity
import se.rimmer.athena.parser.OpFixity
import java.util.*

class ResolveException(text: String): Exception(text)

class Resolver {
    val types = TypeManager()
}

fun Resolver.makeRV(e: Expr) = if(e.type is LVType) CoerceLVExpr(e, types.rv(e.type)) else e

fun Resolver.findOp(op: String): OpFixity {
    return OpFixity(Fixity.Left, 9)
}

inline fun <E> Iterable<E>.iterateLast(element: (E) -> Unit, last: (E) -> Unit) {
    val i = iterator()
    if(i.hasNext()) {
        while(true) {
            val e = i.next()
            if(i.hasNext()) {
                element(e)
            } else {
                last(e)
                break
            }
        }
    }
}

inline fun <E, T> Iterable<E>.mapLast(element: (E) -> T, last: (E) -> T): List<T> {
    val list = ArrayList<T>()
    val i = iterator()
    if(i.hasNext()) {
        while(true) {
            val e = i.next()
            if(i.hasNext()) {
                list.add(element(e))
            } else {
                list.add(last(e))
                break
            }
        }
    }
    return list
}