package se.rimmer.athena.parser

import java.util.*

open class Parser(text: String, val diagnostics: Diagnostics) {
    inner class ParseError(text: String): Exception("line ${token.sourceLine}, column ${token.sourceColumn}: $text")

    fun <T> withLevel(f: () -> T): T {
        val level = IndentLevel(token, lexer)
        val v = f()
        level.end()
        if(token.type == Token.Type.EndOfBlock) {
            eat()
        }
        return v
    }

    /** Parses an item between two other items. */
    fun <T> between(start: () -> Unit, end: () -> Unit, f: () -> T): T {
        start()
        val v = f()
        end()
        return v
    }

    /** Parses one or more items from the provided parser. */
    fun <T> many1(f: () -> T): List<T> {
        val first = f()
        val list = ArrayList<T>()
        list.add(first)
        while(true) {
            val v = tryParse(f) ?: break
            list.add(v)
        }
        return list
    }

    /** Parses zero or more items from the provided parser. */
    fun <T> many(f: () -> T): List<T> {
        val list = ArrayList<T>()
        while(true) {
            val v = tryParse(f) ?: break
            list.add(v)
        }
        return list
    }

    /** Parses one or more items separated by the sep predicate. */
    fun <T> sepBy1(sep: () -> Boolean, f: () -> T): List<T> {
        val first = f()
        val list = ArrayList<T>()
        list.add(first)
        while(sep()) {
            list.add(f())
        }
        return list
    }

    /** Parses one or more items separated by the sep token. */
    fun <T> sepBy1(sep: Token.Type, f: () -> T) = sepBy1({
        if(token.type == sep) {
            eat()
            true
        } else false
    }, f)

    /** Parses zero or more items separated by the sep predicate. */
    fun <T> sepBy(sep: () -> Boolean, f: () -> T): List<T> {
        val first = tryParse(f) ?: return emptyList()
        val list = ArrayList<T>()
        list.add(first)
        while(sep()) {
            list.add(f())
        }
        return list
    }

    /** Parses zero or more items separated by the sep token. */
    fun <T> sepBy(sep: Token.Type, f: () -> T) = sepBy({
        if(token.type == sep) {
            eat()
            true
        } else false
    }, f)

    fun <T> tryParse(f: () -> T): T? {
        val save = SaveLexer(lexer)
        val token = token.copy()
        val v = try {
            f()
        } catch(e: ParseError) {
            save.restore()
            this.token = token
            lexer.token = token
            println("TryParse failed with \"$e\"")
            return null
        }
        if(v == null) {
            save.restore()
            this.token = token
            lexer.token = token
        }
        return v
    }

    fun require(type: Token.Type) = {
        if(token.type == type) {
            eat(); Unit
        } else throw ParseError("expected $type")
    }

    fun expectConID(id: String, discard: Boolean = false) {
        if(token.type != Token.Type.ConID || token.idPayload != id) {
            throw ParseError("expected conid $id")
        }

        if(discard) eat()
    }

    fun expectVarID(id: String, discard: Boolean = false) {
        if(token.type != Token.Type.VarID || token.idPayload != id) {
            throw ParseError("expected varid $id")
        }

        if(discard) eat()
    }

    fun expect(type: Token.Type, discard: Boolean = false, error: String = "expected $type") {
        if(token.type != type) {
            throw ParseError(error)
        }

        if(discard) eat()
    }

    fun expect(kind: Token.Kind, discard: Boolean = false) {
        if(token.kind != kind) {
            throw ParseError("expected $kind")
        }

        if(discard) eat()
    }

    fun eat() = lexer.next()

    var token = Token()
    val lexer = Lexer(text, token, diagnostics)

    init {
        lexer.next()
    }
}

class IndentLevel(start: Token, val lexer: Lexer) {
    val previous = lexer.mIdent

    init {
        lexer.mIdent = start.sourceColumn
        lexer.mBlockCount++
    }

    fun end() {
        lexer.mIdent = previous
        assert(lexer.mBlockCount > 0)
        lexer.mBlockCount--
    }
}