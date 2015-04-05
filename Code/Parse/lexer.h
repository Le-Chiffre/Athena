#ifndef Athena_Parser_lexer_h
#define Athena_Parser_lexer_h

#include <core.h>
#include <Map.h>
#include <Hash.h>

namespace athena {
namespace ast {

using Core::Maybe;
using Core::Nothing;
typedef Core::StringRef String;

struct Diagnostics {
	template<class... P>
	void Error(const char* format, P... args) {Core::LogError(format, args...);}

	template<class... P>
	void Warning(const char* format, P... args) {Core::LogWarning(format, args...);}

	void Enable() {mEnabled = true;}

	void Disable() {mEnabled = false;}

private:
	bool mEnabled = true;
};

struct Qualified {
	Qualified* qualifier = nullptr;
	String name;
};

enum class Assoc : uint16 {
	Left,
	Right
};

struct OpProperties {
	uint16 precedence;
	Assoc associativity;
};

typedef uint32 ID;

struct CompileSettings {

};

struct CompileContext {
	CompileSettings settings;

	/**
	 * Adds an operator to the list with unknown precedence and associativity.
	 * These properties can be updated later.
	 */
	void AddOp(ID op) {
		OpProperties prop{9, Assoc::Left};
		mOPs.Add(op, prop, false);
	}

	/**
	 * Adds an operator to the list with the provided properties,
	 * or updates the properties of an existing operator.
	 */
	void AddOp(ID op, uint prec, Assoc assoc) {
		OpProperties prop{(uint16)prec, assoc};
		mOPs.Add(op, prop, true);
	}

	/**
	 * Returns the properties of the provided operator.
	 * The operator must exist.
	 */
	OpProperties FindOp(ID op) {
		auto res = mOPs.Get(op);
		if(res)
			return *res;
		else
			return {9, Assoc::Left};
	}

	OpProperties* TryFindOp(ID op) {
		return mOPs.Get(op);
	}

	Qualified& Find(ID id) {
		auto res = mNames.Get(id);
		ASSERT(res != nullptr);
		return *res;
	}

	ID AddUnqualifiedName(String str) {
		return AddUnqualifiedName(str.ptr, str.length);
	}

	ID AddUnqualifiedName(const char* chars, uint_ptr count) {
		Qualified q;
		q.name.ptr = chars;
		q.name.length = count;
		return AddName(&q);
	}

	ID AddName(Qualified* q) {
		Core::Hasher h;

		auto qu = q;
		while(qu) {
			h.AddData(qu->name.ptr, (uint)(qu->name.length * sizeof(*qu->name.ptr)));
			qu = qu->qualifier;
		}

		return AddName(ID(h), q);
	}

	ID AddName(ID id, Qualified* q) {
		//In debug mode, we check for collisions.
#if 0 //_DEBUG
		Qualified* p;
		bool res = mNames.Add(id, q, &p, false);
		if(res && p->name.length == q->name.length)
		{
			if(Core::Compare(p->name.ptr, q->name.ptr, p->name.length) != 0)
				DebugError("HsCompile: A name collision occured in the compiler context.");
		}
#else
		mNames.Add(id, *q, false);
#endif
		return id;
	}

	/**
	 * Allocates memory from the current parsing context.
	 */
	void* Alloc(uint_ptr size) {
		//TODO: Fix this.
		return Core::HeapAlloc(size);
	}

	/**
	 * Allocates memory from the current parsing context
	 * and constructs an object with the provided parameters.
	 */
	template<class T, class... P>
	T* New(P&&... p) {
		auto obj = (T*)Alloc(sizeof(T));
		new (obj) T(p...);
		return obj;
	}

private:
	Core::NumberMap<Qualified, ID> mNames{256};
	Core::NumberMap<OpProperties, ID> mOPs{64};
};

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
	
	uint sourceLine;
	uint sourceColumn;
	uint length;
	Type type;
	Kind kind;
	union {
		uint integer;
		double floating;
		wchar32 character;
		ID id;
	} data;
	
	//Special case for VarSym, used to find unary minus more easily.
	//Undefined value if the type is not VarSym.
	bool singleMinus = false;
};

/**
 * A lexer for Haskell 2010 with unlimited lookahead.
 * This is implemented through the function PeekNext(), which returns the token after the provided one.
 * The lexer implements the layout rules by inserting the '{', ';' and '}' tokens according to the spec.
 * If no module specification is found at the start of the file, 
 * it assumes that the base indentation level is the indentation of the first token.
 */
struct Lexer
{
	Lexer(CompileContext& context, const char* text, Token* tok);
	
	/**
	 * Returns the next token from the stream.
	 * On the next call to Next(), the returned token is overwritten with the data from that call.
	 */
	Token* Next();

	CompileContext& GetContext() {
		return mContext;
	}
	
private:
	
	/**
	 * Increments mP until it no longer points to whitespace.
	 * Updates the line statistics.
	 */
	void SkipWhitespace();
	
	/**
	 * Parses mP as a UTF-8 code point and returns it as UTF-32.
	 * If mP doesn't contain valid UTF-32, warnings are generated and ' ' is returned.
	 */
	wchar32 NextCodePoint();
	
	/**
	 * Indicates that the current source pointer is the start of a new line,
	 * and updates the location.
	 */
	void NextLine();
	
	/**
	 * Checks if the current source character is white.
	 * If it is a newline, the current source location is updated.
	 */
	bool WhiteChar_UpdateLine();
	
	/**
	 * Parses a string literal.
	 * mP must point to the first character after the start of the literal (").
	 * If the literal is invalid, warnings or errors are generated and an empty string is returned.
	 */
	String ParseStringLiteral();
	
	/**
	 * Parses a character literal.
	 * mP must point to the first character after the start of the literal (').
	 * If the literal is invalid, warnings are generated and ' ' is returned.
	 */
	wchar32 ParseCharLiteral();
	
	/**
	 * Parses an escape sequence from a character literal.
	 * mP must point to the first character after '\'.
	 * @return The code point generated. If the sequence is invalid, warnings are generated and ' ' is returned.
	 */
	wchar32 ParseEscapedLiteral();
	
	/**
	 * Parses a numeric literal into the current token.
	 * mP must point to the first digit of the literal.
	 */
	void ParseNumericLiteral();
	
	/**
	 * Parses a constuctor operator, reserved operator or variable operator.
	 * mP must point to the first symbol of the operator.
	 */
	void ParseSymbol();

	/**
	 * Parses any special unicode symbols.
	 */
	bool ParseUniSymbol();
	
	/**
	 * Parses a special symbol.
	 * mP must point to the first symbol of the sequence.
	 */
	void ParseSpecial();
	
	/**
	 * Parses a qualified id (qVarID, qConID, qVarSym, qConSym) or constructor.
	 */
	void ParseQualifier();
	
	/**
	 * Parses a variable id or reserved id.
	 */
	void ParseVariable();
	
	/**
	 * Parses the next token into mToken.
	 * Updates mLocation with the new position of mP.
	 * If we have reached the end of the file, this will produce EOF tokens indefinitely.
	 */
	void ParseToken();
	
	/**
	 * Allocates memory from the current parsing context.
	 */
	void* Alloc(uint_ptr size) {
		return mContext.Alloc(size);
	}
	
	/**
	 * Allocates memory from the current parsing context 
	 * and constructs an object with the provided parameters.
	 */
	template<class T, class... P>
	T* New(P... p) {
		return mContext.New<T>(p...);
	}

	friend struct SaveLexer;
	friend struct IndentLevel;

	static const uint kTabWidth = 4;
	static const char kFormatStart = '{';
	static const char kFormatEnd = '}';

	Token* mToken; //The token currently being parsed.
	uint mIdent = 0; //The current indentation level.
	uint mBlockCount = 0; // The current number of indentation blocks.
	const char* mText; //The full source code.
	const char* mP; //The current source pointer.
	const char* mL; //The first character of the current line.
	Qualified mQualifier; //The current qualified name being built up.
	uint mLine = 0; //The current source line.
	uint mTabs = 0; // The number of tabs processed on the current line.
	bool mNewItem = false; //Indicates that a new item was started by the previous token.
	uint8 mFormatting = 0; // Indicates that we are currently inside a formatting string literal.
	Diagnostics mDiag;
	CompileContext& mContext;
};

struct IndentLevel {
	IndentLevel(Token& start, Lexer& lexer) : lexer(lexer), previous(lexer.mIdent) {
		lexer.mIdent = start.sourceColumn;
		lexer.mBlockCount++;
	}

	void end() {
		lexer.mIdent = previous;
		ASSERT(lexer.mBlockCount > 0);
		lexer.mBlockCount--;
	}

	Lexer& lexer;
	const uint previous;
};

struct SaveLexer {
	SaveLexer(Lexer& lexer) :
		lexer(lexer),
		p(lexer.mP),
		l(lexer.mL),
		line(lexer.mLine),
		indent(lexer.mIdent),
		blocks(lexer.mBlockCount),
		tabs(lexer.mTabs),
		formatting(lexer.mFormatting),
		newItem(lexer.mNewItem) {}

	void restore() {
		lexer.mP = p;
		lexer.mL = l;
		lexer.mLine = line;
		lexer.mIdent = indent;
		lexer.mNewItem = newItem;
		lexer.mTabs = tabs;
		lexer.mBlockCount = blocks;
		lexer.mFormatting = formatting;
	}

	Lexer& lexer;
	const char* p;
	const char* l;
	uint line;
	uint indent;
	uint blocks;
	uint tabs;
	uint8 formatting;
	bool newItem;
};

}} // namespace athena::ast

#endif // Athena_Parser_lexer_h