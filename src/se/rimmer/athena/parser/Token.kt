package se.rimmer.athena.parser

class Token {
    enum class Type {
        EndOfFile,
        Comment,
        Whitespace,
        EndOfBlock,
        StartOfFormat,
        EndOfFormat,

        /* Special symbols */
        ParenL,
        ParenR,
        Comma,
        Semicolon,
        BracketL,
        BracketR,
        Grave,
        BraceL,
        BraceR,

        /* Literals */
        Integer,
        Float,
        String,
        Char,

        /* Identifiers */
        VarID,
        ConID,
        VarSym,
        ConSym,

        /* Keywords */
        kwAs,
        kwCase,
        kwClass,
        kwData,
        kwDeriving,
        kwDo,
        kwElse,
        kwFor,
        kwForeign,
        kwIf,
        kwImport,
        kwIn,
        kwInfix,
        kwInfixL,
        kwInfixR,
        kwPrefix,
        kwInstance,
        kwLet,
        kwModule,
        kwNewType,
        kwOf,
        kwThen,
        kwType,
        kwVar,
        kwWhere,
        kwWhile,
        kw_,

        /* Reserved operators */
        opDot,
        opDotDot,
        opColon,
        opColonColon,
        opEquals,
        opBackSlash, // also λ
        opBar,
        opArrowL, // <- and ←
        opArrowR, // -> and →
        opAt,
        opDollar,
        opTilde,
        opArrowD,
    };

    enum class Kind {
        Literal,
        Special,
        Identifier,
        Keyword
    };

    var sourceLine = 0
    var sourceColumn = 0
    var length = 0
    var type = Type.EndOfFile
    var kind = Kind.Special

    var intPayload = 0L
    var floatPayload = 0.0
    var charPayload = ' '
    var idPayload = ""

    // Special case for VarSym, used to find unary minus more easily.
    // Undefined value if the type is not VarSym.
    var singleMinus = false

    fun copy(): Token {
        val t = Token()
        t.sourceLine = sourceLine
        t.sourceColumn = sourceColumn
        t.length = length
        t.type = type
        t.kind = kind
        t.intPayload = intPayload
        t.floatPayload = floatPayload
        t.charPayload = charPayload
        t.idPayload = idPayload
        t.singleMinus = singleMinus
        return t
    }
}