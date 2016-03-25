package se.rimmer.athena.resolve

fun Resolver.createIf(cond: Expr, then: Expr, otherwise: Expr?, used: Boolean): Expr {
    return createIf(listOf(IfCond(null, cond)), then, otherwise, used, CondMode.And)
}

fun Resolver.createIf(conds: List<IfCond>, then: Expr, otherwise: Expr?, used: Boolean, mode: CondMode): Expr {
    val mappedConds = conds.map {
        if(it.cond != null) {

        }
    }
}