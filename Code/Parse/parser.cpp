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

inline Literal toStringLiteral(Id name) {
	Literal l;
	l.s = name;
	l.type = Literal::String;
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
	 * decl			→	fundecl
	 * 				|	typedecl
	 * 				|	datadecl
	 * fundecl		→	var : args = expr
	 * 				|	var tuptype [→ type] = expr
	 * 				|	var [→ type] = expr
	 * args			→	arg0 arg1 ... argn		(n ≥ 0)
	 * arg			→	varid
	 */
	if(token == Token::kwType) {
		parseTypeDecl();
	} else if(token == Token::kwData) {
		parseDataDecl();
	} else if(auto var = tryParse(&Parser::parseVar)) {
		if(token == Token::opColon) {
			eat();

			// Parse zero or more arguments.
			TupleFieldList* arg = nullptr;
			if(token == Token::VarID) {
				arg = build<TupleFieldList>(TupleField{nullptr, token.data.id, nullptr});
				auto p = arg;
				eat();
				while(token == Token::VarID) {
					auto pp = build<TupleFieldList>(TupleField{nullptr, token.data.id, nullptr});
					eat();
					p->next = pp;
					p = pp;
				}
			}
			TupleType* args = build<TupleType>(arg);

			if(token == Token::opEquals) {
				eat();

				// Parse the function body.
				if(auto expr = parseExpr()) {
					module.declarations += build<FunDecl>(var(), expr, args, nullptr);
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
				module.declarations += build<FunDecl>(var(), expr, nullptr, nullptr);
			} else {
				error("Expected a function body expression.");
			}
		} else if(token == Token::BraceL) {
            // Parse the function arguments as a tuple.
            auto args = (TupleType*)parseTupleType();
			Type* type = nullptr;

			// Parse optional return type.
			if(token == Token::opArrowR) {
				eat();
				type = parseType();
			}

			if(token != Token::opEquals) {
				error("Expected '=' after a function signature.");
				return;
			}
			eat();

            // Parse the function body.
            if(auto expr = parseExpr()) {
                module.declarations += build<FunDecl>(var(), expr, args, type);
            } else {
                error("Expected a function body expression.");
            }
        } else if(token == Token::opArrowR) {
			eat();

			// Parse the return type.
			auto type = parseType();
			if(token != Token::opEquals) {
				error("Expected '=' after a function signature.");
				return;
			}
			eat();

			// Parse the function body.
			if(auto expr = parseExpr()) {
				module.declarations += build<FunDecl>(var(), expr, nullptr, type);
			} else {
				error("Expected a function body expression.");
			}
		} else {
			error("Expected ':' or '=' after a function name declaration.");
		}
	}
}

void Parser::parseDataDecl() {
	/*
	 * datadecl		→	data varid = fields
	 * fields		→	field0, ..., fieldn 	(n >= 0)
	 * field		→	var varid = expr
	 * 				|	var varid : type
	 * 				|	let varid = expr
	 * 				|	let varid : type
	 */
	if(token == Token::kwData) {
		eat();
		if(token == Token::ConID) {
			auto id = token.data.id;
			eat();
			if(token == Token::opEquals) {
				eat();
				IndentLevel level{token, lexer};
				if(token == Token::kwLet || token == Token::kwVar) {
					auto list = build<FieldList>(parseField());
					auto p = list;
					while(token == Token::EndOfStmt) {
						eat();
						p->next = build<FieldList>(parseField());
						p = p->next;
					}

					level.end();
					if(token == Token::EndOfBlock) {
						eat();
						module.declarations += build<DataDecl>(id, list);
					} else {
						error("Expected end of block.");
					}
				} else {
					// TODO: Parse methods.
					error("Expected field declaration.");
				}
			} else {
				error("Expected '=' after 'data name'");
			}
		} else {
			error("Expected identifier after 'data'.");
		}
	} else {
		error("Expected 'data'.");
	}
}

void Parser::parseTypeDecl() {
	/*
	 * typedecl		→	type varid = type
	 */
	if(token == Token::kwType) {
		eat();
		if(token == Token::ConID) {
			auto id = token.data.id;
			eat();
			if(token == Token::opEquals) {
				eat();
				if(auto type = parseType()) {
					module.declarations += build<TypeDecl>(id, type);
				} else {
					error("expected type after 'type t ='.");
				}
			} else {
				error("expected type after 'type t'.");
			}
		} else {
			error("expected identifier after 'type'.");
		}
	} else {
		error("expected 'type'.");
	}
}

Expr* Parser::parseExpr() {
	/*
	 * expr			→	typedexpr
	 * 				|	typedexpr0, …, typedexprn	(statements, n ≥ 2)
	 */

	// Start a new indentation block.
	IndentLevel level{token, lexer};
	if(auto expr = parseTypedExpr()) {
		if(token == Token::EndOfStmt) {
			auto list = build<ExprList>(expr);
			auto p = list;
			while (token == Token::EndOfStmt) {
				eat();
				if((expr = parseTypedExpr())) {
					p->next = build<ExprList>(expr);
					p = p->next;
				} else {
					return (Expr*)error("Expected an expression.");
				}
			}

			level.end();
			if(token == Token::EndOfBlock) eat();
			return build<MultiExpr>(list);
		} else {
			level.end();
			if(token == Token::EndOfBlock) eat();
			return expr;
		}
	} else {
		return (Expr*)error("Expected an expression.");
	}
}

Expr* Parser::parseTypedExpr() {
	/*
	 * typedexpr	→	infixexpr : type
	 *				|	infixexpr
	 */

	auto expr = parseInfixExpr();
	if(!expr) return nullptr;

	if(token == Token::opColon) {
		eat();
		if(auto type = parseType()) {
			return build<CoerceExpr>(expr, type);
		} else {
			return nullptr;
		}
	} else {
		return expr;
	}
}

Expr* Parser::parseInfixExpr() {
	/*
	 * infixexp		→	pexp qop infixexp			(infix operator application)
	 * 				|	pexp = infixexp				(assignment)
	 * 				|	pexp $ infixexp				(application shortcut)
	 *				|	pexp
	 */

	// Left-expression or binary operator.
	if(auto lhs = parsePrefixExpr()) {
		if(token == Token::opEquals) {
			eat();
			if(auto value = parseInfixExpr()) {
				return build<AssignExpr>(lhs, value);
			} else {
				error("Expected an expression after assignment.");
				return nullptr;
			}
		} else if(token == Token::opDollar) {
			eat();
			if(auto value = parseInfixExpr()) {
				return build<AppExpr>(lhs, build<ExprList>(value));
			} else {
				error("Expected a right-hand side for a binary operator.");
				return nullptr;
			}
		} else if(auto op = tryParse(&Parser::parseQop)) {
			// Binary operator.
			if(auto rhs = parseInfixExpr()) {
				return build<InfixExpr>(op(), lhs, rhs);
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

Expr* Parser::parsePrefixExpr() {
	/*
	 * pexp		→	varsym lexp				(prefix operator application)
	 *			|	lexp
	 */

	// Prefix operator.
	if(token == Token::VarSym) {
		auto op = token.data.id;
		eat();
		if(auto expr = parseLeftExpr()) {
			return build<PrefixExpr>(op, expr);
		} else {
			return (Expr*)error("Expected expression after a prefix operator.");
		}
	} else {
		return parseLeftExpr();
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
					return build<IfExpr>(cond, then, tryParse(&Parser::parseElse));
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
			if(token == Token::kwIn) {
				eat();
				if(auto loop = parseExpr()) {
					return build<WhileExpr>(cond, loop);
				} else {
					error("Expected expression after 'in'");
				}
			} else {
				error("Expected 'in' after while-expression.");
			}
		} else {
			error("Expected expression after 'while'");
		}
	} else {
		return parseCallExpr();
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

			return build<AppExpr>(callee, list);
		} else {
			return callee;
		}
	} else {
		return (Expr*)error("Expected an expression.");
	}
}

Expr* Parser::parseAppExpr() {
	/*
	 * aexp		→	bexp
	 * 			|	bexp.bexp		(method call syntax)
	 */
	auto e = parseBaseExpr();
	if(!e) return nullptr;

	if(token == Token::opDot) {
		eat();
		auto app = parseBaseExpr();
		if(!app) return nullptr;

		return build<FieldExpr>(e, app);
	} else {
		return e;
	}
}

Expr* Parser::parseBaseExpr() {
	/*
	 * bexp		→	qvar			(variable or function without args)
	 * 			|	qcon			(object construction)
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
				// Parenthesized expressions have a separate type to preserve ordering constraints.
				return build<NestedExpr>(exp);
			} else {
				return (Expr*)error("Expected ')' after '(' and an expression.");
			}
		} else {
			return (Expr*)error("Expected expression after '('.");
		}
	} else if(token == Token::ConID) {
		auto name = token.data.id;
		eat();
		return build<ConstructExpr>(name);
	} else if(auto var = tryParse(&Parser::parseVar)) {
		return build<VarExpr>(var());
	} else {
		return (Expr*)error("Expected an expression.");
	}
}

Expr* Parser::parseLiteral() {
	ASSERT(token == Token::Literal);
	if(token == Token::String) {
		return parseStringLiteral();
	} else {
		auto expr = build<LitExpr>(toLiteral(token));
		eat();
		return expr;
	}
}

Expr* Parser::parseStringLiteral() {
	ASSERT(token == Token::String);
	auto string = token.data.id;
	eat();

	// Check if the string contains formatting.
	if(token == Token::StartOfFormat) {
		// Parse one or more formatting expressions.
		// The first one consists of just the first string chunk.
		FormatList list{FormatChunk{string, nullptr}};
		auto p = &list;
		while(token == Token::StartOfFormat) {
			eat();
			auto expr = parseInfixExpr();
			if(!expr)
				return nullptr;

			if(token != Token::EndOfFormat)
				return (Expr*)error("Expected end of string format after this expression.");

			eat();
			ASSERT(token == Token::String);
			p->next = build<FormatList>(FormatChunk{token.data.id, expr});
			p = p->next;
			eat();
		}

		return build<FormatExpr>(list);
	} else {
		return build<LitExpr>(toStringLiteral(string));
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
			level.end();
			if(token == Token::EndOfBlock) eat();
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
				return build<DeclExpr>(id, expr, constant);
			} else {
				error("Expected expression.");
			}
		} else {
			return build<DeclExpr>(id, nullptr, constant);
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

Type* Parser::parseType() {
	if(token == Token::ParenL) {
		eat();
		if(token == Token::ParenR) {
			eat();
			return build<Type>(Type::Unit);
		} else {
			error("Expected ')' after '(' in type.");
		}
	} else if(token == Token::VarSym) {
		auto name = lexer.GetContext().Find(token.data.id).name;
		if(name.length == 1 && name.ptr[0] == kPointerSigil) {
			eat();
			if(auto type = parseType()) {
				type->kind = Type::Ptr;
				return type;
			} else {
				return nullptr;
			}
		}
	} else if(token == Token::ConID) {
		auto id = token.data.id;
		eat();
		return build<Type>(Type::Con, id);
	} else if(token == Token::BraceL) {
        return parseTupleType();
    }

	error("Expected a type.");
	return nullptr;
}

Type* Parser::parseTupleType() {
    /*
     * tuptype  →   { tupfield1, ..., tupfieldn }       (n ≥ 1)
     */

	if(token == Token::BraceL) {
		eat();
        if(auto f = parseTupleField()) {
            auto list = build<TupleFieldList>(f());
            auto p = list;

            while(token == Token::Comma) {
                eat();
                auto field = parseTupleField();
                if(!field) return nullptr;

                p->next = build<TupleFieldList>(field());
                p = p->next;
            }

            if(token == Token::BraceR) {
                eat();
                return build<TupleType>(list);
            } else {
                error("Expected '}");
            }
        } else {
            error("Expected one or more tuple fields");
        }
	} else {
		error("Expected '{'");
	}

	return nullptr;
}

Maybe<TupleField> Parser::parseTupleField() {
    /*
     * tupfield →   varid [: type]
     *          |   varid [= infixexpr]
     *          |   type [= infixexpr]
     * (The last one may not be valid in any context, but may be used in the future)
     */

    TypeRef type = nullptr;
    Maybe<Id> name = Nothing;
    ExprRef def = nullptr;

    // If the token is a varid, it can either be a generic or named parameter, depending on the token after it.
    if(token == Token::VarID) {
        auto id = token.data.id;
        eat();
        if(token == Token::opColon) {
            // This was the parameter name.
            eat();
            type = parseType();
            name = id;
        } else if(token == Token::opEquals) {
            name = id;
        } else {
            // This was the type.
            type = build<Type>(Type::Gen, id);
        }
    } else {
        type = parseType();
    }

    if(!type) return Nothing;

    // Parse default value.
    if(!name && token == Token::opEquals) {
        eat();
        def = parseInfixExpr();
    }

    return TupleField{type, name, def};
}


Field* Parser::parseField() {
	bool constant;
	if(token == Token::kwLet) {
		constant = true;
	} else if(token == Token::kwVar) {
		constant = false;
	} else {
		error("expected 'let' or 'var'.");
		return nullptr;
	}

	eat();
	if(token == Token::VarID) {
		auto id = token.data.id;
		Expr* content = nullptr;
		Type* type = nullptr;
		eat();
		if(token == Token::opEquals) {
			eat();
			content = parseExpr();
		} else if(token == Token::opColon) {
			eat();
			type = parseType();
		} else {
			error("expected ':' or '=' after a field name.");
		}

		if(content || type) {
			return build<Field>(id, type, content, constant);
		} else {
			error("expected a type or field initializer.");
		}
	} else {
		error("expected a field name.");
	}

	return nullptr;
}
	
Expr* Parser::parseElse() {
	if(token == Token::EndOfStmt) eat();
	if(token == Token::kwElse) {
		eat();
		return parseExpr();
	} else {
		return nullptr;
	}
}

nullptr_t Parser::error(const char* text) {
	Core::LogError(text);
	return nullptr;
}

}} // namespace athena::ast