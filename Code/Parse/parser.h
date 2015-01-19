#ifndef Athena_Parser_parser_h
#define Athena_Parser_parser_h

#include "ast.h"
#include "lexer.h"
#include <StaticBuffer.h>

namespace athena {
namespace ast {

using Core::Maybe;

struct SourcePos {
	Core::StringRef file;
	uint line;
	uint column;
};

inline SourcePos initialPos(Core::StringRef file) {return {file, 1, 1};}

inline SourcePos updatePos(SourcePos pos, char c) {
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

inline SourcePos updateStringPos(SourcePos pos, Core::StringRef string) {
	return fold(updatePos, pos, string);
}


struct Parser {
	Parser(CompileContext& context, const char* text) : lexer(context, text, &token), buffer(4*1024*1024) {}

	void parseModule();
	void parseDecl();

	Expr* parseExpr();
	Expr* parseLeftExpr();
	Expr* parseCallExpr();
	Expr* parseAppExpr();
	Expr* parseLiteral();
	void  parseFixity();
	Maybe<Id> parseVar();
	Maybe<Id> parseQop();

	void addFixity(Fixity f);
	nullptr_t error(const char* text);

	void eat() {
		lexer.Next();
	}

	template<class Ret>
	auto tryParse(Ret (Parser::*f)()) {
		SaveLexer l{lexer};
		auto v = (this->*f)();
		if(!v) l.restore();
		return v;
	}

	template<class T, class... P>
	T* build(P&&... p) {
		return buffer.New<T>(Core::Forward<P>(p)...);
	}

	Module* module;
	Token token;
	Lexer lexer;
	Core::StaticBuffer buffer;
};


}} // namespace athena::ast

#endif // Athena_Parser_parser_h