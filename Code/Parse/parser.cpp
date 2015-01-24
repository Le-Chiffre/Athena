#include "parser.h"

namespace athena {
namespace ast {

static const Fixity kDefaultFixity{Fixity::Left, 9};

inline Literal toLiteral(Token& tok) {
	Literal l;
    switch(tok.type) {
        case Token::Integer:
            l.i = tok.data.integer;
            l.type = Literal::Int;
            break;
        case Token::Float:
            l.f = tok.data.floating;
            l.type = Literal::Float;
            break;
        case Token::Char:
            l.c = tok.data.character;
            l.type = Literal::Char;
            break;
        case Token::String:
            l.s = tok.data.id;
            l.type = Literal::String;
            break;
        default: FatalError("Invalid literal type.");
    }
	return l;
}

void Parser::parseModule() {
	IndentLevel level{token, lexer};
	parseDecl();
	while(token == Token::EndOfStmt) {
		eat();
		parseDecl();
	}

	if(token != Token::EndOfBlock) {
		error("Expected end of statement block.");
	}

	level.end();
	eat();
}

void Parser::parseDecl() {
	/*
	 * fundecl		→	var :: args = expr
	 * 				|	var = expr
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
					module.declarations += build<FunDecl>(var(), *expr, args);
				} else {
					error("Expected a function body expression.");
				}
			} else {
				error("Expected '=' after a function declaration.");
			}
		} else if(token == Token::opEquals) {
			eat();

			// Parse the function body.
			if(auto expr = parseExpr()) {
				module.declarations += build<FunDecl>(var(), *expr);
			} else {
				error("Expected a function body expression.");
			}
		} else {
			error("Expected '::' or '=' after a function name declaration.");
		}
	}
}

Expr* Parser::parseExpr() {
	/*
	 * expr			→	infixexpr
	 * 				|	infixexpr0, …, infixexprn	(statements, n ≥ 2)
	 */

	// Start a new indentation block.
	IndentLevel level{token, lexer};
	if(auto expr = parseInfixExpr()) {
		if(token == Token::EndOfStmt) {
			auto list = build<ExprList>(expr);
			auto p = list;
			while (token == Token::EndOfStmt) {
				eat();
				if((expr = parseInfixExpr())) {
					p->next = build<ExprList>(expr);
					p = p->next;
				} else {
					return (Expr*)error("Expected an expression.");
				}
			}

			if(token != Token::EndOfBlock) {
				error("Expected end of statement block.");
				return nullptr;
			}

			level.end();
			eat();
			return build<MultiExpr>(list);
		} else {
			ASSERT(token == Token::EndOfBlock);
			level.end();
			eat();
			return expr;
		}
	} else {
		return (Expr*)error("Expected an expression.");
	}
}

Expr* Parser::parseInfixExpr() {
	/*
	 * infixexp		→	lexp qop infixexp			(infix operator application)
	 * 				|	varsym infixexp				(prefix operator application)
	 *				|	lexp
	 */

	// Prefix operator.
	if(token == Token::VarSym) {
		auto op = token.data.id;
		eat();
		if(auto expr = parseInfixExpr()) {
			return build<PrefixExpr>(op, *expr);
		} else {
			return (Expr*)error("Expected expression after a prefix operator.");
		}
	}

	// Left-expression or binary operator.
	if(auto lhs = parseLeftExpr()) {
		if(auto op = tryParse(&Parser::parseQop)) {
			// Binary operator.
			if(auto rhs = parseInfixExpr()) {
				return build<InfixExpr>(op(), *lhs, *rhs);
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
	/*
	 * lexp		→	\ apat1 … apatn -> exp					(lambda abstraction, n ≥ 1)
	 *			|	let decls [in exp]						(let expression)
	 *			|	var decls [in exp]						(var expression)
	 *			|	if exp [;] then exp [;] else exp	    (conditional)
	 *			|	case exp of { alts }					(case expression)
	 *			|	while exp do exp						(while loop)
	 *			|	do { stmts }							(do expression)
	 *			|	fexp = infixexp							(assignment)
	 *			|	fexp
	 */
	if(token == Token::kwLet) {
		eat();
		return parseVarDecl(true);
	} else if(token == Token::kwVar) {
		eat();
		return parseVarDecl(false);
	} else if(token == Token::kwCase) {
		eat();
		if(auto exp = parseInfixExpr()) {
			if(token == Token::kwOf) {
				eat();
				// TODO: Parse alts.
			} else {
				error("Expected 'of' after case-expression.");
			}
		} else {
			error("Expected an expression after 'case'.");
		}
	} else if(token == Token::kwIf) {
		eat();
		if(auto cond = parseInfixExpr()) {
			// Allow statement ends within an if-expression to allow then/else with the same indentation as if.
			if(token == Token::EndOfStmt) eat();

			if(token == Token::kwThen) {
				eat();
				if(auto then = parseExpr()) {
					// else is optional.
					Expr* otherwise = nullptr;

					if(token == Token::EndOfStmt) eat();
					if(token == Token::kwElse) {
						eat();
						otherwise = parseExpr();
					}

					return build<IfExpr>(*cond, *then, otherwise);
				}
			} else {
				error("Expected 'then' after if-expression.");
			}
		} else {
			error("Expected an expression after 'if'.");
		}
	} else if(token == Token::kwWhile) {
		eat();
		if(auto cond = parseInfixExpr()) {
			if(token == Token::kwDo) {
				eat();
				if(auto loop = parseExpr()) {
					return build<WhileExpr>(*cond, *loop);
				} else {
					error("Expected expression after 'do'");
				}
			} else {
				error("Expected 'do' after while-expression.");
			}
		} else {
			error("Expected expression after 'while'");
		}
	} else {
		if(auto expr = parseCallExpr()) {
			if(token == Token::opEquals) {
				eat();
				if(auto value = parseInfixExpr()) {
					return build<AssignExpr>(*expr, *value);
				} else {
					error("Expected an expression after assignment.");
				}
			} else {
				return expr;
			}
		}
	}

	return nullptr;
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

Expr* Parser::parseVarDecl(bool constant) {
	// Parse one or more declarations, separated as statements.
	IndentLevel level(token, lexer);

	if(auto expr = parseDeclExpr(constant)) {
		if(token == Token::EndOfStmt) {
			auto list = build<ExprList>(expr);
			auto p = list;
			while(token == Token::EndOfStmt) {
				eat();
				if((expr = parseDeclExpr(constant))) {
					p->next = build<ExprList>(expr);
					p = p->next;
				} else {
					error("Expected declaration after 'var' or 'let'.");
					return nullptr;
				}
			}

			if(token != Token::EndOfBlock) {
				error("Expected end of statement block.");
				return nullptr;
			}

			level.end();
			eat();
			return build<MultiExpr>(list);
		} else {
			ASSERT(token == Token::EndOfBlock);
			level.end();
			eat();
			return expr;
		}
	} else {
		error("Expected declaration after 'var' or 'let'.");
		return nullptr;
	}
}

Expr* Parser::parseDeclExpr(bool constant) {
	/*
	 * declexpr		→	varid = expr
	 */
	if(token == Token::VarID) {
		auto id = token.data.id;
		eat();
		if(token == Token::opEquals) {
			eat();
			if(auto expr = parseInfixExpr()) {
				return build<DeclExpr>(id, *expr, constant);
			} else {
				error("Expected expression.");
			}
		} else {
			error("Expected '=' after a declaration.");
		}
	} else {
		error("Expected identifier.");
	}

	return nullptr;
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
		if(module.operators.AddGet(token.data.id, pf)) {
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