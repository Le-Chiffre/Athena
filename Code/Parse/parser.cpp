
#include "parser.h"

namespace athena {
namespace ast {

static const Fixity kDefaultFixity{Fixity::Left, 9};

inline Literal toLiteral(Token& tok) {
	Literal l;
	l.d = tok.data.floating;
	return l;
}

void Parser::parseModule() {
	while(token != Token::EndOfFile) {
		parseDecl();
	}
}

void Parser::parseDecl() {
	/*
	 * fundecl		→	var :: args = expr
	 * args			→	arg0 arg1 ... argn		(n ≥ 0)
	 * arg			→	varid
	 */
	if(auto var = tryParse(&Parser::parseVar)) {
		if(token == Token::opColonColon) {
			eat();

			// Parse zero or more arguments.
			ArgList* args = nullptr;
			if(token == Token::VarID) {
				args = build<ArgList>(token.data.id);
				auto p = args;
				eat();
				while(token == Token::VarID) {
					p = build<ArgList>(token.data.id);
					eat();
					args->next = p;
					args = p;
				}
			}

			if(token == Token::opEquals) {
				eat();

				// Parse the function body.
				if(auto expr = parseExpr()) {
					module->declarations += build<FunDecl>(var(), *expr, args);
				} else {
					error("Expected a function body expression.");
				}
			} else {
				error("Expected '=' after a function declaration.");
			}
		} else {
			error("Expected '::' after a function name declaration.");
		}
	}
}

Expr* Parser::parseExpr() {
	/*
	 * infixexp		→	lexp qop infixexp		(infix operator application)
	 * 				|	varsym infixexp			(prefix operator application)
	 *				|	lexp
	 */

	// Prefix operator.
	if(token == Token::VarSym) {
		auto op = token.data.id;
		eat();
		if(auto expr = parseExpr()) {
			return build<PrefixExpr>(op, *expr);
		} else {
			return (Expr*)error("Expected expression after a prefix operator.");
		}
	}

	// Left-expression or binary operator.
	if(auto lhs = parseLeftExpr()) {
		if(auto op = tryParse(&Parser::parseQop)) {
			// Binary operator.
			if(auto rhs = parseExpr()) {
				return build<InfixExpr>(op, *lhs, *rhs);
			} else {
				return (Expr*)error("Expected a right-hand side for a binary operator.");
			}
		} else {
			// Single expression.
			return lhs;
		}
	} else {
		return (Expr*)error("Expected an expression.");
	}
}

Expr* Parser::parseLeftExpr() {
	return parseCallExpr();
}

Expr* Parser::parseCallExpr() {
	/*
	 * fexp		→	[fexp] aexp		(function application)
	 */
	if(auto callee = parseAppExpr()) {
		// Parse any arguments applied to the callee.
		if(auto app = tryParse(&Parser::parseAppExpr)) {
			auto list = build<ExprList>(app);
			auto p = list;

			while((app = tryParse(&Parser::parseAppExpr))) {
				auto l = build<ExprList>(app);
				p->next = l;
				p = l;
			}

			return build<AppExpr>(*callee, list);
		} else {
			return callee;
		}
	} else {
		return (Expr*)error("Expected an expression.");
	}
}

Expr* Parser::parseAppExpr() {
	/*
	 * aexp		→	qvar			(variable or function without args)
	 *			|	literal
	 *			|	( exp )			(parenthesized expression)
	 */
	if(token == Token::Literal) {
		return parseLiteral();
	} else if(token == Token::ParenL) {
		eat();
		if(auto exp = parseExpr()) {
			if(token == Token::ParenR) {
				eat();
				return exp;
			} else {
				return (Expr*)error("Expected ')' after '(' and an expression.");
			}
		} else {
			return (Expr*)error("Expected expression after '('.");
		}
	} else if(auto var = tryParse(&Parser::parseVar)) {
		return build<VarExpr>(var());
	} else {
		return (Expr*)error("Expected an expression.");
	}
}

Expr* Parser::parseLiteral() {
	if(token == Token::Literal) {
		auto expr = build<LitExpr>(toLiteral(token));
		eat();
		return expr;
	} else {
		return (Expr*)error("Expected a literal value.");
	}
}

void Parser::parseFixity() {
	/*
	 * fixity	→	fixity [integer] ops
	 * ops		→	op1, …, opn	    		(n ≥ 1)
	 */
	Fixity f;

	// ´infixl´ and ´infix´ both produce left association.
	if(token == Token::kwInfix || token == Token::kwInfixL)
		f.kind = Fixity::Left;
	else if(token == Token::kwInfixR)
		f.kind = Fixity::Right;
	else if(token == Token::kwPrefix)
		f.kind = Fixity::Prefix;
	else
		return;

	eat();

	// Check if a precedence for these operators was applied.
	// If no precedence is provided, we use the default of 9 as defined by the standard.
	if(token == Token::Integer) {
		f.prec = (uint8)token.data.integer;
		eat();
	} else {
		f.prec = kDefaultFixity.prec;
	}

	/*
	 * Parse a list of operators affected by this, and add them to the module.
	 * At least one operator must be provided.
	 */

	// Parse the first, required operator.
	addFixity(f);

	// Parse any others.
	while(token == Token::Comma) {
		addFixity(f);
	}
}

void Parser::addFixity(Fixity f) {
	if(token == Token::VarSym) {
		Fixity* pf;
		if(module->operators.AddGet(token.data.id, pf)) {
			error("This operator has already had its precedence defined.");
		} else {
			*pf = f;
			eat();
		}
	} else {
		error("Expected one or more operators after a fixity declaration or ','.");
	}
}

Maybe<Id> Parser::parseVar() {
	/*
	 * var	→	varid | ( varsym )
	 */
	if(token == Token::VarID) {
		auto id = token.data.id;
		eat();
		return id;
	} else if(token == Token::ParenL) {
		eat();
		if(token == Token::VarSym) {
			auto id = token.data.id;
			eat();
			if(token == Token::ParenR) {
				eat();
				return id;
			}
		}
	}

	return Nothing;
}

Maybe<Id> Parser::parseQop() {
	/*
	 * qop	→	qvarsym | `qvarid`
	 */
	if(token == Token::VarSym) {
		auto id = token.data.id;
		eat();
		return id;
	} else if(token == Token::Grave) {
		eat();
		if(token == Token::VarID) {
			auto id = token.data.id;
			eat();
			if(token == Token::Grave) {
				eat();
				return id;
			}
		}
	}

	return Nothing;
}

nullptr_t Parser::error(const char* text) {
	Core::LogError(text);
	return nullptr;
}

}} // namespace athena::ast