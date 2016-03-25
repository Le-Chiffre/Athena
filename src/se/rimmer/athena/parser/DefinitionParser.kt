package se.rimmer.athena.parser

import java.math.BigDecimal
import java.math.BigInteger
import java.util.*

class DefinitionParser(text: String, diagnostics: Diagnostics): Parser(text, diagnostics) {
    fun parseModule(target: Module) {
        withLevel {
            // Parse zero or more imports.
            while(true) {
                while(token.type == Token.Type.Semicolon) eat()

                if(token.type == Token.Type.kwImport) {
                    target.imports.add(parseImport())
                } else {
                    break
                }
            }

            // After imports, parse declarations until the EOF.
            parseDeclaration(target.declarations)
            while(token.type == Token.Type.Semicolon) {
                eat()
                parseDeclaration(target.declarations)
            }
        }
    }

    fun parseImport(): Import {
        expect(Token.Type.kwImport, true)
        val name = parseQualifiedName()
        val asName = if(token.type == Token.Type.kwAs) {
            eat()
            parseConID()
        } else name.name

        return Import(name, asName)
    }

    fun parseDeclaration(list: MutableList<Decl>) {
        if(token.type == Token.Type.kwData) {
            list.add(parseDataDecl())
        } else if(token.type == Token.Type.kwType) {
            list.add(parseTypeDecl())
        } else if(token.type == Token.Type.kwForeign) {
            list.add(parseForeignDecl())
        } else {
            list.add(parseFunDecl())
        }
    }

    fun parseFunDecl(): FunDecl {
        val name = parseVar()
        val args = if(token.type == Token.Type.BracketL) {
            between(require(Token.Type.BracketL), require(Token.Type.BracketR)) {
                sepBy(Token.Type.Comma) {parseTupleField()}
            }
        } else emptyList<TupleField>()

        val type = if(token.type == Token.Type.opArrowR) {
            eat()
            parseType()
        } else null

        val f = if(token.type == Token.Type.opEquals) {
            eat()
            FunDecl(name, parseExpr(), null, args, type)
        } else if(token.type == Token.Type.opBar) {
            val cases = withLevel {
                sepBy1(Token.Type.Semicolon) {
                    expect(Token.Type.opBar, true)
                    val pats = many {parsePattern()}
                    expect(Token.Type.opEquals, true)
                    val expr = parseExpr()
                    FunCase(pats, expr)
                }
            }

            FunDecl(name, null, cases, args, type)
        } else {
            throw ParseError("expected a function definition")
        }

        if(token.type == Token.Type.kwWhere) {
            eat()
            f.locals.addAll(withLevel {sepBy(Token.Type.Semicolon) {parseFunDecl()}})
        }

        return f
    }

    fun parseForeignDecl(): ForeignDecl {
        expect(Token.Type.kwForeign, true)
        expect(Token.Type.kwImport, true)

        val cconv = if(token.type == Token.Type.VarID) {
            when(token.idPayload) {
                "ccall" -> ForeignConvention.C
                "stdcall" -> ForeignConvention.Stdcall
                "cpp" -> ForeignConvention.Cpp
                "js" -> ForeignConvention.JS
                "jvm" -> ForeignConvention.JVM
                else -> throw ParseError("unknown calling convention")
            }
        } else {
            ForeignConvention.C
        }


        val name = parseString("expected import name string")
        val importName = parseVarID("expected import identifier")
        expect(Token.Type.opColon, true)
        val type = parseType()
        return ForeignDecl(name, importName, type, cconv)
    }

    fun parseDataDecl(): DataDecl {
        expect(Token.Type.kwData, true)
        val type = parseSimpleType()
        expect(Token.Type.opEquals, true)
        val alts = sepBy1(Token.Type.opBar) { parseConstructor() }
        return DataDecl(type, alts)
    }

    fun parseTypeDecl(): TypeDecl {
        expect(Token.Type.kwType, true)
        val id = parseSimpleType()
        expect(Token.Type.opEquals, true)
        val type = parseType()
        return TypeDecl(id, type)
    }

    fun parseExpr(): Expr {
        val list = withLevel {
            sepBy1(Token.Type.Semicolon) {parseTypedExpr()}
        }
        return if(list.size > 1) BlockExpr(list) else list[0]
    }

    fun parseTypedExpr(): Expr {
        val exp = parseInfixExpr()
        return if(token.type == Token.Type.opColon) {
            eat()
            val target = parseType()
            CoerceExpr(exp, target)
        } else exp
    }

    fun parseInfixExpr(): Expr {
        // Left-expression or binary operator.
        val lhs = parseLeftExpr()
        return if(token.type == Token.Type.opEquals) {
            eat()
            AssignExpr(lhs, parseInfixExpr())
        } else if(token.type == Token.Type.VarSym || token.type == Token.Type.Grave) {
            val op = parseQop()
            val rhs = parseInfixExpr()
            InfixExpr(lhs, rhs, op)
        } else lhs
    }

    fun parseLeftExpr(): Expr {
        return if(token.type == Token.Type.kwLet) {
            eat()
            parseVarDecl(true)
        } else if(token.type == Token.Type.kwVar) {
            eat()
            parseVarDecl(false)
        } else if(token.type == Token.Type.kwCase) {
            parseCaseExpr()
        } else if(token.type == Token.Type.kwIf) {
            eat()
            if(token.type == Token.Type.opBar) {
                val cases = withLevel {
                    sepBy(Token.Type.Semicolon) {
                        expect(Token.Type.opBar, true)
                        val cond = if(token.type == Token.Type.kw_) {
                            eat()
                            null
                        } else parseInfixExpr()
                        expect(Token.Type.opArrowR)
                        val then = parseExpr()
                        IfCase(cond, then)
                    }
                }

                MultiIfExpr(cases)
            } else {
                val cond = parseInfixExpr()

                // Allow statement ends within an if-expression to allow then/else with the same indentation as if.
                if(token.type == Token.Type.Semicolon) eat()

                expect(Token.Type.kwThen, true)
                val then = parseExpr()

                // else is optional.
                val otherwise = tryParse {parseElse()}
                IfExpr(cond, then, otherwise)
            }
        } else if(token.type == Token.Type.kwWhile) {
            eat()
            val cond = parseInfixExpr()
            expect(Token.Type.opArrowR, true)
            val body = parseExpr()
            WhileExpr(cond, body)
        } else if(token.type == Token.Type.opBackSlash) {
            eat()
            var args: List<TupleField>? = null
            var isCase = false
            if(token.type == Token.Type.VarID) {
                // Parse zero or more argument names.
                args = many1 {
                    val id = parseVarID()
                    TupleField(null, id, null)
                }
            } else if(token.type == Token.Type.BracketL) {
                // Parse the function arguments as a tuple.
                args = between(require(Token.Type.BracketL), require(Token.Type.BracketR)) {
                    sepBy1(Token.Type.Comma) {parseTupleField()}
                }
            } else if(token.type == Token.Type.kwCase) {
                eat()
                isCase = true
            } else {
                throw ParseError("expected function parameters")
            }

            expect(Token.Type.opArrowR, true)
            val e = parseInfixExpr()
            LamExpr(args, e, isCase)
        } else {
            parsePrefixExpr()
        }
    }

    fun parsePrefixExpr(): Expr {
        return if(token.type == Token.Type.VarSym) {
            val op = token.idPayload
            eat()
            PrefixExpr(parseCallExpr(), op)
        } else parseCallExpr()
    }

    fun parseCallExpr(): Expr {
        val callee = parseAppExpr()
        return if(token.type == Token.Type.BracketL) {
            AppExpr(callee, parseTupleConstruct())
        } else callee
    }

    fun parseAppExpr(): Expr {
        /*
         * aexp		→	bexp
         * 			|	bexp.bexp		(method call syntax)
         */
        val e = parseBaseExpr()
        return if(token.type == Token.Type.opDot) {
            eat()
            val app = parseBaseExpr()
            FieldExpr(e, app)
        } else e
    }

    fun parseCaseExpr(): Expr {
        expect(Token.Type.kwCase, true)
        val exp = parseTypedExpr()
        expect(Token.Type.kwOf, true)
        val alts = withLevel {
            sepBy1(Token.Type.Semicolon) { parseAlt() }
        }
        return CaseExpr(exp, alts)
    }

    fun parseBaseExpr(): Expr {
        return if(token.kind == Token.Kind.Literal) {
            parseLiteral()
        } else if(token.type == Token.Type.ParenL) {
            between(require(Token.Type.ParenL), require(Token.Type.ParenR)) { NestedExpr(parseExpr()) }
        } else if(token.type == Token.Type.BracketL) {
            parseTupleConstruct()
        } else if(token.type == Token.Type.ConID) {
            val id = token.idPayload
            eat()
            val args = if(token.type == Token.Type.BracketL) {
                parseTupleConstruct()
            } else UnitExpr()

            ConstructExpr(ConType(id), args)
        } else {
            try {
                VarExpr(parseVar())
            } catch(e: Exception) {
                throw ParseError("expected an expression")
            }
        }
    }

    fun parseLiteral(): Expr {
        return if(token.type == Token.Type.String) {
            parseStringLiteral()
        } else {
            val l = toLiteral(token)
            eat()
            LitExpr(l)
        }
    }

    fun parseStringLiteral(): Expr {
        val string = parseString()

        // Check if the string contains formatting.
        if(token.type == Token.Type.StartOfFormat) {
            // Parse one or more formatting expressions.
            // The first one consists of just the first string chunk.
            val list = ArrayList<FormatChunk>()
            list.add(FormatChunk(string, null))

            while(token.type == Token.Type.StartOfFormat) {
                eat()
                val expr = parseTypedExpr()
                expect(Token.Type.EndOfFormat, true, "Expected end of string format after this expression.")
                val segment = parseString()
                list.add(FormatChunk(segment, expr))
            }

            return FormatExpr(list)
        } else {
            return LitExpr(StringLiteral(string))
        }
    }

    fun parseVarDecl(constant: Boolean): Expr {
        val decls = withLevel {
            sepBy1(Token.Type.Semicolon) {
                sepBy1(Token.Type.Comma) {
                    parseDeclExpr(constant)
                }
            }
        }.flatMap {it}
        return if(decls.size > 1) BlockExpr(decls) else decls[0]
    }

    fun parseDeclExpr(constant: Boolean): Expr {
        val id = parseVarID()
        return if(token.type == Token.Type.opEquals) {
            eat()
            val value = parseTypedExpr()
            DeclExpr(id, value, constant)
        } else {
            DeclExpr(id, null, constant)
        }
    }

    fun parseTupleConstruct(): Expr {
        return between(require(Token.Type.BracketL), require(Token.Type.BracketR)) {
            val l = sepBy(Token.Type.Comma) {
                parseTupleConstructField()
            }

            if(l.isEmpty()) {
                UnitExpr()
            } else {
                TupExpr(l)
            }
        }
    }

    fun parseTupleConstructField(): TupleField {
        // If the token is a varid it can either be a generic or named parameter, depending on the token after it.
        return tryParse {
            val id = parseVarID()
            expect(Token.Type.opEquals, true)
            val expr = parseTypedExpr()
            TupleField(null, id, expr)
        } ?: TupleField(null, null, parseTypedExpr())
    }

    fun parseElse(): Expr {
        if(token.type == Token.Type.Semicolon) eat()
        expect(Token.Type.kwElse, true)
        return parseExpr()
    }

    fun toLiteral(token: Token): Literal {
        return when(token.type) {
            Token.Type.Integer -> IntLiteral(BigInteger.valueOf(token.intPayload))
            Token.Type.Float -> RealLiteral(BigDecimal.valueOf(token.floatPayload))
            Token.Type.String -> StringLiteral(token.idPayload)
            else -> throw ParseError("expected literal")
        }
    }

    fun parseAlt(): CaseAlt {
        val pat = parsePattern()
        expect(Token.Type.opArrowR, true)
        val exp = parseTypedExpr()
        return CaseAlt(pat, exp)
    }

    fun parsePattern(asVar: String? = null): Pattern {
        return if(token.singleMinus) {
            eat()
            val lit = toLiteral(token)
            eat()
            LitPattern(lit, null)
        } else if(token.type == Token.Type.ConID) {
            val id = token.idPayload
            eat()

            // Parse a pattern for each constructor element.
            val list = many {parseLeftPattern(asVar)}
            ConPattern(id, list, null)
        } else {
            parseLeftPattern(asVar)
        }
    }

    fun parseLeftPattern(asVar: String? = null): Pattern {
        return if(token.kind == Token.Kind.Literal) {
            val l = toLiteral(token)
            eat()
            LitPattern(l, asVar)
        } else if(token.type == Token.Type.kw_) {
            eat()
            AnyPattern(Unit, asVar)
        } else if(token.type == Token.Type.VarID) {
            val v = token.idPayload
            eat()
            if(token.type == Token.Type.opAt) {
                eat()
                parseLeftPattern(v)
            } else {
                VarPattern(v, asVar)
            }
        } else if(token.type == Token.Type.ParenL) {
            between(require(Token.Type.ParenL), require(Token.Type.ParenR)) { parsePattern(asVar) }
        } else if(token.type == Token.Type.ConID) {
            val id = token.idPayload
            eat()
            ConPattern(id, emptyList(), asVar)
        } else if(token.type == Token.Type.BracketL) {
            val e = between(require(Token.Type.BracketL), require(Token.Type.BracketR)) {
                sepBy(Token.Type.Comma) {
                    var name: String? = null
                    val pat = if(token.type == Token.Type.VarID) {
                        val id = token.idPayload
                        eat()
                        if(token.type == Token.Type.opEquals) {
                            eat()
                            name = id
                            parsePattern()
                        } else {
                            VarPattern(id, null)
                        }
                    } else {
                        parsePattern()
                    }

                    FieldPat(name, pat)
                }
            }
            TupPattern(e, asVar)
        } else {
            throw ParseError("expected pattern")
        }
    }

    fun parseType(): Type {
        val list = sepBy1(Token.Type.opArrowR) {
            val apps = many1 {parseAType()}
            if(apps.size > 1) {
                AppType(apps[0], apps.drop(1))
            } else {
                apps[0]
            }
        }

        return if(list.size > 1) {
            FunType(list)
        } else {
            list[0]
        }
    }

    fun parseAType(): Type {
        return if(token.type == Token.Type.VarSym) {
            if(token.idPayload == "*") {
                eat()
                PtrType(parseAType())
            } else {
                throw ParseError("operator types are not supported yet")
            }
        } else if(token.type == Token.Type.ConID) {
            val id = token.idPayload
            eat()
            ConType(id)
        } else if(token.type == Token.Type.VarID) {
            val id = token.idPayload
            eat()
            GenType(id)
        } else if(token.type == Token.Type.BracketL) {
            // Also handles unit type.
            parseTupleType()
        } else if(token.type == Token.Type.ParenL) {
            between(require(Token.Type.ParenL), require(Token.Type.ParenR)) { parseType() }
        } else {
            throw ParseError("expected type")
        }
    }

    fun parseTupleType(): Type {
        return between(require(Token.Type.BracketL), require(Token.Type.BracketR)) {
            val list = sepBy(Token.Type.Comma) {
                parseTupleField()
            }

            if(list.isEmpty()) {
                UnitType()
            } else {
                TupType(list)
            }
        }
    }

    fun parseTupleField(): TupleField {
        var name: String? = null
        var type: Type?

        // If the token is a varid it can either be a generic or named parameter, depending on the token after it.
        if(token.type == Token.Type.VarID) {
            name = token.idPayload
            eat()
            type = tryParse { parseType() }
        } else {
            type = parseType()
        }

        // Parse default value.
        val default = if(token.type == Token.Type.opEquals) {
            eat()
            parseTypedExpr()
        } else null

        if(type == null && default == null) {
            throw ParseError("fields must have either a defined type or an inferred type")
        }

        return TupleField(type, name, default)
    }

    fun parseConstructor(): Constructor {
        val name = parseConID()
        val types = many { parseAType() }
        return Constructor(name, types)
    }

    fun parseSimpleType(): SimpleType {
        val id = parseConID()
        val apps = many { parseVarID() }
        return SimpleType(id, apps)
    }

    fun parseVar(): String {
        return if(token.type == Token.Type.VarID) {
            parseVarID()
        } else if(token.type == Token.Type.ParenL) {
            eat()
            expect(Token.Type.VarSym)
            val id = token.idPayload
            eat()
            expect(Token.Type.ParenR, true)
            return id
        } else {
            throw ParseError("expected varid or (varsym)")
        }
    }

    fun parseQop(): String {
        return if(token.type == Token.Type.VarSym) {
            val id = token.idPayload
            eat()
            id
        } else if(token.type == Token.Type.Grave) {
            between(require(Token.Type.Grave), require(Token.Type.Grave)) { parseVarID() }
        } else {
            throw ParseError("expected operator symbol")
        }
    }

    fun parseQualifiedName(qualifier: Qualified? = null): Qualified {
        if(token.type != Token.Type.VarID && token.type != Token.Type.String && token.type != Token.Type.ConID) {
            throw ParseError("Expected qualified name")
        }

        val name = parseIdentifier()
        return if(token.type == Token.Type.opDot) {
            parseQualifiedName(Qualified(name, qualifier))
        } else {
            Qualified(name, qualifier)
        }
    }

    fun parseString(error: String = "expected a string"): String {
        if(token.type != Token.Type.String) {
            throw ParseError(error)
        }

        val id = token.idPayload
        eat()
        return id
    }

    fun parseIdentifier(): String {
        if(token.type != Token.Type.VarID && token.type != Token.Type.String && token.type != Token.Type.ConID) {
            throw ParseError("expected variable name")
        }

        val id = token.idPayload
        eat()
        return id
    }

    fun parseConID(): String {
        expect(Token.Type.ConID)
        val id = token.idPayload
        eat()
        return id
    }

    fun parseVarID(error: String = "expected variable name"): String {
        // A VarID can be a string literal as well, in order to be able to use keyword variable names.
        if(token.type != Token.Type.VarID && token.type != Token.Type.String) {
            throw ParseError(error)
        }

        val id = token.idPayload
        eat()
        return id
    }
}