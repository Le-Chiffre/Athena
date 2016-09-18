#ifndef Athena_Parser_lexer_h
#define Athena_Parser_lexer_h

#include "../General/compiler.h"
#include "../General/string.h"
#include "context.h"

namespace athena {
namespace ast {

struct Token {
	enum Type {
		EndOfFile,
		Comment,
		Whitespace,
		EndOfBlock,
		StartOfFormat,
		EndOfFormat,

		/* Special symbols */
		ParenL = '(',
		ParenR = ')',
		Comma = ',',
		EndOfStmt = Comma,
		Semicolon = ';',
		BracketL = '[',
		BracketR = ']',
		Grave = '`',
		BraceL = '{',
		BraceR = '}',

		/* Literals */
		Integer = 128,
		Float,
		String,
		Char,

		/* Identifiers */
		VarID,
		ConID,
		VarSym,
		ConSym,

		/* Keywords */
		kwCase,
		kwClass,
		kwData,
		kwDefault,
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
        kwResource, // Only when compiling shaders.
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

	enum Kind {
		Literal,
		Special,
		Identifier,
		Keyword
	};

	bool operator == (Type t) {return type == t;}
	bool operator == (Kind c) {return kind == c;}
	bool operator != (Type t) {return !(*this == t);}
	bool operator != (Kind c) {return !(*this == c);}

	U32 sourceLine;
	U32 sourceColumn;
	U32 length;
	Type type;
	Kind kind;
	union {
		U32 integer;
		double floating;
		WChar32 character;
		Id id;
	} data;

	// Special case for VarSym, used to find unary minus more easily.
	// Undefined value if the type is not VarSym.
	bool singleMinus = false;
};

/**
 * A lexer for Haskell 2010 with unlimited lookahead.
 * This is implemented through the function PeekNext(), which returns the token after the provided one.
 * The lexer implements the layout rules by inserting the '{', ';' and '}' tokens according to the spec.
 * If no module specification is found at the start of the file,
 * it assumes that the base indentation level is the indentation of the first token.
 */
struct Lexer {
	Lexer(CompileContext& context, Diagnostics& diag, const char* text, Token* tok);

	/**
	 * Returns the next token from the stream.
	 * On the next call to Next(), the returned token is overwritten with the data from that call.
	 */
	Token* next();

private:

	/**
	 * Increments mP until it no longer points to whitespace.
	 * Updates the line statistics.
	 */
	void skipWhitespace();

	/**
	 * Parses mP as a UTF-8 code point and returns it as UTF-32.
	 * If mP doesn't contain valid UTF-32, warnings are generated and ' ' is returned.
	 */
	U32 nextCodePoint();

	/**
	 * Indicates that the current source pointer is the start of a new line,
	 * and updates the location.
	 */
	void nextLine();

	/**
	 * Checks if the current source character is white.
	 * If it is a newline, the current source location is updated.
	 */
	bool whiteChar_UpdateLine();

	/**
	 * Parses a string literal.
	 * mP must point to the first character after the start of the literal (").
	 * If the literal is invalid, warnings or errors are generated and an empty string is returned.
	 */
	std::string parseStringLiteral();

	/**
	 * Parses a character literal.
	 * mP must point to the first character after the start of the literal (').
	 * If the literal is invalid, warnings are generated and ' ' is returned.
	 */
	U32 parseCharLiteral();

	/**
	 * Parses an escape sequence from a character literal.
	 * mP must point to the first character after '\'.
	 * @return The code point generated. If the sequence is invalid, warnings are generated and ' ' is returned.
	 */
	U32 parseEscapedLiteral();

	/**
	 * Parses a numeric literal into the current token.
	 * mP must point to the first digit of the literal.
	 */
	void parseNumericLiteral();

	/**
	 * Parses a constuctor operator, reserved operator or variable operator.
	 * mP must point to the first symbol of the operator.
	 */
	void parseSymbol();

	/**
	 * Parses a special symbol.
	 * mP must point to the first symbol of the sequence.
	 */
	void parseSpecial();

	/**
	 * Parses a qualified id (qVarID, qConID, qVarSym, qConSym) or constructor.
	 */
	void parseQualifier();

	/**
	 * Parses a variable id or reserved id.
	 */
	void parseVariable();

	/**
	 * Parses the next token into mToken.
	 * Updates mLocation with the new position of mP.
	 * If we have reached the end of the file, this will produce EOF tokens indefinitely.
	 */
	void parseToken();

	/**
	 * Allocates memory from the current parsing context.
	 */
	void* alloc(Size size) {
		return context.alloc(size);
	}

	/**
	 * Allocates memory from the current parsing context
	 * and constructs an object with the provided parameters.
	 */
	template<class T, class... P>
	T* build(P... p) {
		return context.build<T>(p...);
	}

	friend struct SaveLexer;
	friend struct IndentLevel;

	static const U32 kTabWidth = 4;
	static const char kFormatStart = '`';
	static const char kFormatEnd = '`';

	Token* token; //The token currently being parsed.
	U32 ident = 0; //The current indentation level.
	U32 blockCount = 0; // The current number of indentation blocks.
	const char* text; //The full source code.
	const char* p; //The current source pointer.
	const char* l; //The first character of the current line.
	Qualified qualifier; //The current qualified name being built up.
	U32 line = 0; //The current source line.
	U32 tabs = 0; // The number of tabs processed on the current line.
	bool newItem = false; //Indicates that a new item was started by the previous token.
	Byte formatting = 0; // Indicates that we are currently inside a formatting string literal.

public:
	CompileContext& context;
	Diagnostics& diag;
};

struct IndentLevel {
	IndentLevel(Token& start, Lexer& lexer) : lexer(lexer), previous(lexer.ident) {
		lexer.ident = start.sourceColumn;
		lexer.blockCount++;
	}

	void end() {
		lexer.ident = previous;
		assert(lexer.blockCount > 0);
		lexer.blockCount--;
	}

	Lexer& lexer;
	const U32 previous;
};

struct SaveLexer {
	SaveLexer(Lexer& lexer) :
		lexer(lexer),
		p(lexer.p),
		l(lexer.l),
		line(lexer.line),
		indent(lexer.ident),
		blockCount(lexer.blockCount),
		tabs(lexer.tabs),
		formatting(lexer.formatting),
		newItem(lexer.newItem) {}

	void restore() {
		lexer.p = p;
		lexer.l = l;
		lexer.line = line;
		lexer.ident = indent;
		lexer.newItem = newItem;
		lexer.tabs = tabs;
		lexer.blockCount = blockCount;
		lexer.formatting = formatting;
	}

	Lexer& lexer;
	const char* p;
	const char* l;
	U32 line;
	U32 indent;
	U32 blockCount;
	U32 tabs;
	Byte formatting;
	bool newItem;
};

}} // namespace athena::ast

#endif // Athena_Parser_lexer_h