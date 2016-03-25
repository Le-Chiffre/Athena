#ifndef Athena_Parser_parser_h
#define Athena_Parser_parser_h

#include "ast.h"
#include "lexer.h"
#include <Mem/StaticBuffer.h>

namespace athena {
namespace ast {

struct SourcePos {
	String file;
	U32 line;
	U32 column;
};

inline SourcePos initialPos(const String& file) {return {file, 1, 1};}

inline SourcePos updatePos(char c, SourcePos pos) {
	if(c == '\n') {
		pos.line++;
		pos.column = 1;
	} else if(c == '\t') {
		pos.column = pos.column + 8 - (pos.column - 1) % 8;
	} else {
		pos.column++;
	}

	return pos;
}

inline SourcePos updateStringPos(SourcePos pos, const String& string) {
	return fold(updatePos, pos, string);
}

struct Parser {
	static const char kPointerSigil = '*';

	Parser(CompileContext& context, Module& module, const char* text) : module(module), lexer(context, text, &token), buffer(4*1024*1024) {lexer.Next();}

	void parseModule();
	void parseDecl();
	Decl* parseFunDecl();
	void parseDataDecl();
	void parseTypeDecl();
	void parseForeignDecl();
    void parseShaderDecl();

	Expr* parseExpr();
	Expr* parseTypedExpr();
	Expr* parseInfixExpr();
	Expr* parsePrefixExpr();
	Expr* parseLeftExpr();
	Expr* parseCallExpr();
	Expr* parseAppExpr();
	Expr* parseCaseExpr();
	Expr* parseBaseExpr();

	/// Parses a literal token. The caller should ensure that the token is a literal.
	Expr* parseLiteral();

	/// Parses a string literal token. The caller should ensure that the token is a string literal.
	Expr* parseStringLiteral();

	Expr* parseVarDecl(bool constant);
	Expr* parseDeclExpr(bool constant);
	void parseFixity();
	Maybe<Alt> parseAlt();
	Maybe<Id> parseVar();
	Maybe<Id> parseQop();

	Type* parseType();
	Type* parseAType();
	SimpleType* parseSimpleType();
    Maybe<TupleField> parseTupleField();
	Maybe<TupleField> parseTupleConstructField();
	Type* parseTupleType();
	Expr* parseTupleConstruct();
	Field* parseField();
	Expr* parseElse();
	Constr* parseConstr();

	Pattern* parseLeftPattern();
	Pattern* parsePattern();

	void addFixity(Fixity f);
	Expr* error(const char* text);

	void eat() {lexer.Next();}

	template<class T> auto list(const T& t) {return new(buffer) ASTList<T>(t);}
	template<class T> auto listE(const T& t) {return list(getListElem(t));}

	auto tokenE(Token::Type type) {
		return [=] {
			if(token == type) {
				eat();
				return true;
			} else {
				// TODO: Token type printing.
				error("expected <type> token");
				return false;
			}
		};
	}

	template<class F>
	auto withLevel(F&& f) {
		IndentLevel level{token, lexer};
		auto r = f();
		level.end();
		if(token == Token::EndOfBlock) eat();
		return r;
	}

	template<class F, class Start, class End>
	auto between(F&& f, Start&& start, End&& end) -> decltype(f()) {
		if(!start()) return nullptr;
		auto res = f();
		if(!end()) return nullptr;
		return res;
	}

	template<class F> auto between(F&& f, Token::Type start, Token::Type end) {return between(f, tokenE(start), tokenE(end));}

	template<class F>
	auto many(F&& f) -> decltype(listE(f())) {
		if(auto expr = tryParse(f)) {
			auto list = listE(expr);
			auto p = list;

			while((expr = tryParse(f))) {
				auto l = listE(expr);
				p->next = l;
				p = l;
			}

			return list;
		} else {
			return nullptr;
		}
	}

	template<class F>
	auto many1(F&& f) -> decltype(listE(f())) {
		if(auto expr = f()) {
			auto list = listE(expr);
			auto p = list;

			while((expr = tryParse(f))) {
				auto l = listE(expr);
				p->next = l;
				p = l;
			}

			return list;
		} else {
			return nullptr;
		}
	}

	template<class F, class Sep>
	auto sepBy(F&& f, Sep&& sep) -> decltype(listE(f())) {
		if(auto expr = tryParse(f)) {
			auto list = listE(expr);
			auto p = list;

			while(sep()) {
				expr = f();
				if(!expr) return nullptr;

				auto l = listE(expr);
				p->next = l;
				p = l;
			}

			return list;
		} else {
			return nullptr;
		}
	}

	template<class F, class Sep>
	auto sepBy1(F&& f, Sep&& sep) -> decltype(listE(f())) {
		if(auto expr = f()) {
			auto list = listE(expr);
			auto p = list;

			while(sep()) {
				expr = f();
				if(!expr) return nullptr;

				auto l = listE(expr);
				p->next = l;
				p = l;
			}

			return list;
		} else {
			return nullptr;
		}
	}

	template<class F> auto sepBy1(F&& f, Token::Type sep) {return sepBy1(f, tokenE(sep));}
	template<class F> auto sepBy(F&& f, Token::Type sep) {return sepBy(f, tokenE(sep));}

	template<class F>
	auto tryParse(F&& f) {
		SaveLexer l{lexer};
		auto tok = token;
		auto v = f();
		if(!v) {
			l.restore();
			token = tok;
		}
		return v;
	}

	Module& module;
	Token token;
	Lexer lexer;
	Tritium::StaticBuffer buffer;
};


}} // namespace athena::ast

#endif // Athena_Parser_parser_h