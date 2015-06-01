#include "resolve_ast.h"
#include "resolve.h"

namespace athena {
namespace resolve {

Expr* Resolver::createRet(ExprRef e) {
    return build<RetExpr>(*getRV(e));
}

Expr* Resolver::createBool(bool b) {
    Literal lit;
    lit.i = b ? 1 : 0;
    lit.type = Literal::Bool;
    return build<LitExpr>(lit, types.getBool());
}

Expr* Resolver::createInt(int i) {
    Literal lit;
    lit.i = (uint)i; // The code generator makes no distinction between signed and unsigned.
    lit.type = Literal::Int;
    return build<LitExpr>(lit, types.getInt());
}

Expr* Resolver::createCompare(Scope& scope, ExprRef left, ExprRef right) {
    return resolveBinaryCall(scope, context.AddUnqualifiedName("=="), left, right);
}

Expr* Resolver::createIf(ExprRef cond, ExprRef then, const Expr* otherwise, bool used) {
    return createIf(IfConds{IfCond{nullptr, (Expr*)&cond}}, then, otherwise, used, CondMode::And);
}

Expr* Resolver::createIf(IfConds&& conds, ExprRef then_, const Expr* otherwise_, bool used, CondMode mode) {
    for(auto& c : conds) {
        if(c.cond) c.cond = getRV(*c.cond);
    }
    auto then = getRV(then_);
    Expr* otherwise = otherwise_ ? getRV(*otherwise_) : nullptr;

    // Find the type of the expression.
    // If-expressions without an else-part can fail and never return a value.
    // If there is an else-part then both branches must return the same type.
    auto type = types.getUnit();
    if(otherwise) {
        if(then->type->isKnown() && otherwise->type->isKnown()) {
            // If the branches don't have the same type, try to convert them into each other.
            // This means that cases like one branch returning an int32 and the other returning an int8 will return int32.
            if(then->type == otherwise->type) {
                type = then->type;
            } else if(typeCheck.compatible(*then, otherwise->type)) {
                type = otherwise->type;
                then = implicitCoerce(*then, type);
            } else if(typeCheck.compatible(*otherwise, then->type)) {
                type = then->type;
                then = implicitCoerce(*otherwise, type);
            } else {
                error("the then and else branches of an if-expression must be compatible when used as an expression");
            }
        } else {
            type = types.getUnknown();
        }
    } else if(used) {
        error("this expression may not return a value");
    }

    return build<IfExpr>(Move(conds), *then, otherwise, type, used, mode);
}

Expr* Resolver::createField(ExprRef pivot, Field* field) {
    auto type = field->type;
    if(pivot.type->isLvalue() || pivot.type->isPointer()) type = types.getLV(type);
    return build<FieldExpr>(pivot, field, type);
}

Expr* Resolver::createField(ExprRef pivot, int field) {
    auto stype = getEffectiveType(pivot.type);
    if(stype->isTuple()) {
        auto ttype = (TupleType*)stype;
        if(ttype->fields.Count() <= field) {
            error("field does not exist");
            return nullptr;
        }

        auto type = ttype->fields[field].type;
        if(pivot.type->isLvalue()) type = types.getLV(type);
        return build<FieldExpr>(pivot, &ttype->fields[field], type);
    } else if(stype->isVariant()) {
        if(field == -1) {
            return build<FieldExpr>(pivot, -1, types.getInt());
        }

        auto vtype = (VarType*)stype;
        if(vtype->list.Count() <= field) {
            error("constructor does not exist");
            return nullptr;
        }

        auto type = vtype->list[field]->dataType;
        if(pivot.type->isLvalue()) type = types.getLV(type);
        return build<FieldExpr>(pivot, field, type);
    } else {
        ASSERT("cannot retrieve field from this type" == 0);
        return nullptr;
    }
}

Expr* Resolver::createGetCon(ExprRef variant) {
    if(variant.type->isVariant()) {
        return build<FieldExpr>(variant, -1, types.getInt());
    } else {
        return createInt(0);
    }
}

}} // namespace athena::resolve
