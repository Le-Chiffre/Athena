package se.rimmer.athena.resolve

import se.rimmer.athena.parser.ForeignConvention
import se.rimmer.athena.parser.FunDecl
import java.util.*
import se.rimmer.athena.parser.Type as ASTType
import se.rimmer.athena.parser.FunType as ASTFunType

open class FunctionDecl(val name: String) {
    var codegen: Any? = null

    /*
	 * The following fields are invalid as long as the function has not been resolved.
	 */

    // The arguments this function takes.
    // Each argument also exists in the function scope as a variable.
    val arguments = ArrayList<Variable>()

    // The return type of this function.
    // Note that this is resolved lazily in most cases.
    var type: Type? = null

    // The next function overload with this name.
    var sibling: FunctionDecl? = null
}

/**
 * A function implemented in this module.
 * @param ast The source declaration of this function in the AST.
 * This will be set as long as the function has not been resolved.
 * Any function where this is set after resolving is either unused or an error.
 */
class Function(name: String, var ast: FunDecl?): FunctionDecl(name) {
    val scope = Scope(name, null, this)

    /*
	 * The following fields are invalid as long as the function has not been resolved.
	 */

    var expression: Expr? = null

    // This is true as long as the function contains any generic parameters or return type.
    // Generic functions must be instantiated before they can be called normally.
    var generic: Boolean = true

    var mangledName: String = name
}

class ForeignFunction(name: String, val importedName: String, val cconv: ForeignConvention, var ast: ASTFunType?): FunctionDecl(name)

interface ArgumentRef {
    val name: String
    val type: Type
}

interface FunctionRef {
    val arguments: List<ArgumentRef>
    val returnType: Type
}



class Variable(val name: String, val type: Type, val scope: Scope, val constant: Boolean, val funParam: Boolean = false) {
    var codegen: Any? = null

    fun isVar() = !constant && !funParam
}

class Alt(val cond: Expr, val result: Expr)

class Scope(val name: String, val parent: Scope? = null, val function: Function? = null) {
    /// Finds any variable with the provided name that is visible in this scope.
    Variable* findVar(Id name);

    /// Finds any local variable with the provided name that is declared within this scope.
    Variable* findLocalVar(Id name);

    TypeRef findType(Id name);
    VarConstructor* findConstructor(Id name);

    bool hasVariables();

    // Children of this scope.
    val children = ArrayList<Scope>()

    // The variables that were declared in this scope.
    val variables = ArrayList<Variable>()

    // The function parameters that were shadowed in this scope.
    val shadows = ArrayList<Variable>()

    // The variables from the parent function that are captured here.
    // If this list contains entries, the function is a closure.
    val captures = ArrayList<Variable>()

    // The functions that were declared in this scope.
    val functions = HashMap<String, Function>()

    // The types that were declared in this scope.
    val types = HashMap<String, Type>()

    // The constructor names that were declared in this scope.
    ConMap constructors;
}