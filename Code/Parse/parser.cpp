#include "parser.h"
#include "lexer.h"

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
	} else if(token == Token::kwForeign) {
		parseForeignDecl();
	} else if(auto var = tryParse([=] {return parseVar();})) {
		TupleType* args = nullptr;
		if(token == Token::VarID) {
			// Parse zero or more argument names.
			args = build<TupleType>(many1([=] {
				if(token == Token::VarID) return just(TupleField{nullptr, token.data.id, nullptr});
				else return nothing<TupleField>();
			}));
		} else if(token == Token::BracketL) {
			// Parse the function arguments as a tuple.
			args = (TupleType*)parseTupleType();
		}

		// Parse optional return type.
		Type* type = nullptr;
		if(token == Token::opArrowR) {
			eat();
			type = parseType();
		}

		if(token == Token::opEquals) {
			eat();

			// Parse the function body.
			if(auto expr = parseExpr()) {
				module.declarations += build<FunDecl>(var(), expr, args, type);
			} else {
				error("expected a function body expression.");
			}
		} else if(token == Token::opBar) {
			auto cases = withLevel([=] {
				return sepBy1([=] {
					if(token == Token::opBar) eat();
					else {error("expected '|'"); return (FunCase*)nullptr;}

					auto pats = many([=] {return parsePattern();});

					if(token == Token::opEquals) eat();
					else {error("expected '='"); return (FunCase*)nullptr;}

					if(auto expr = parseExpr()) {
						return build<FunCase>(pats, expr);
					} else {
						error("expected function body");
						return (FunCase*)nullptr;
					}
				}, Token::EndOfStmt);
			});

			if(cases) {
				module.declarations += build<FunDecl>(var(), cases, args, type);
			} else {
				error("expected a function pattern");
			}
		} else {
			error("expected a function definition");
		}
	}
}

void Parser::parseDataDecl() {
	/*
	 * datadecl		→	data simpletype = constrs
	 * constrs		→	constr1 | … | constrn		(n ≥ 1)
	 * constr		→	conid atype1 … atypen
	 */
	if(token == Token::kwData) {
		eat();
		auto type = parseSimpleType();
		if(token == Token::opEquals) {
			eat();
			auto cs = sepBy1([=] {return parseConstr();}, Token::opBar);
			if(!cs) error("expected at least one constructor definition");
			else module.declarations += build<DataDecl>(type, cs);
		} else {
			error("Expected '=' after type name");
		}
	}
}

void Parser::parseTypeDecl() {
	/*
	 * typedecl		→	type varid = type
	 */
	if(token == Token::kwType) {
		eat();
		if(auto t = parseSimpleType()) {
			if(token == Token::opEquals) {
				eat();
				if(auto type = parseType()) {
					module.declarations += build<TypeDecl>(t, type);
				} else {
					error("expected type after 'type t ='.");
				}
			} else {
				error("expected type after 'type t'.");
			}
		} else {
			error("expected simple type after 'type'.");
		}
	} else {
		error("expected 'type'.");
	}
}

void Parser::parseForeignDecl() {
	/*
	 * topdecl	→	foreign fdecl
	 * fdecl	→	import callconv [safety] impent var : ftype	   	 	(define variable)
	 * 			|	export callconv expent var : ftype	    			(expose variable)
	 * callconv	→	ccall | stdcall | cplusplus | js	    			(calling convention)
	 * impent	→	[string]
	 * expent	→	[string]
	 * safety	→	unsafe | safe
	 */
	if(token == Token::kwForeign) {
		eat();
		if(token == Token::kwImport) {
			eat();

			// Optional calling convention. Otherwise, default to ccall.
			auto convention = ForeignConvention::CCall;
			if(token == Token::VarID) {
				auto& name = lexer.GetContext().Find(token.data.id);
				if(name.name == "ccall") {
					convention = ForeignConvention::CCall;
				} else if(name.name == "stdcall") {
					convention = ForeignConvention::Stdcall;
				} else if(name.name == "cpp") {
					convention = ForeignConvention::Cpp;
				} else if(name.name == "js") {
					convention = ForeignConvention::JS;
				} else {
					error("unknown calling convention.");
				}

				eat();
			}

			Id name = 0;
			if(token == Token::String) {
				name = token.data.id;
				eat();
			} else {
				error("expected name string.");
			}

			Id importName = 0;
			if(token == Token::VarID) {
				importName = token.data.id;
				eat();
			} else {
				error("expected an identifier");
			}

			if(token == Token::opColon) {
				eat();
			} else {
				error("expected ':'.");
			}

			auto type = parseType();
			module.declarations += build<ForeignDecl>(name, importName, type, convention);
		} else {
			error("expected 'import'.");
		}
	} else {
		error("expected 'foreign'.");
	}
}

Expr* Parser::parseExpr() {
	/*
	 * expr			→	typedexpr
	 * 				|	typedexpr0, …, typedexprn	(statements, n ≥ 2)
	 */
	auto list = withLevel([=] {return sepBy1([=] {return parseTypedExpr();}, Token::EndOfStmt);});
	if(!list) return error("Expected an expression");
	else if(!list->next) return list->item;
	else return build<MultiExpr>(list);
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
				return build<AppExpr>(lhs, list(value));
			} else {
				error("Expected a right-hand side for a binary operator.");
				return nullptr;
			}
		} else if(auto op = tryParse([=] {return parseQop();})) {
			// Binary operator.
			if(auto rhs = parseInfixExpr()) {
				return build<InfixExpr>(op(), lhs, rhs);
			} else {
				return error("Expected a right-hand side for a binary operator.");
			}
		} else {
			// Single expression.
			return lhs;
		}
	} else {
		return error("Expected an expression.");
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
			return error("Expected expression after a prefix operator.");
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
		return parseCaseExpr();
	} else if(token == Token::kwIf) {
		eat();
		if(token == Token::opBar) {
			// Multi-way if (erlang style).
			auto list = withLevel([=]{ return sepBy([=] {
				if(token == Token::opBar) eat();
				else return (IfCase*)error("expected '|'");
				Expr* cond;
				if(token == Token::kw_) {
					eat();
					cond = build<LitExpr>(trueLit());
				} else {
					cond = parseInfixExpr();
				}

				if(token == Token::opArrowR) eat();
				else return (IfCase*)error("expected '->'");
				auto then = parseExpr();
				return build<IfCase>(cond, then);
			}, Token::EndOfStmt);});
			return build<MultiIfExpr>(list);
		} else {
			if(auto cond = parseInfixExpr()) {
				// Allow statement ends within an if-expression to allow then/else with the same indentation as if.
				if(token == Token::EndOfStmt) eat();

				if(token == Token::kwThen) {
					eat();
					if(auto then = parseExpr()) {
						// else is optional.
						return build<IfExpr>(cond, then, tryParse([=] { return parseElse(); }));
					}
				} else {
					error("Expected 'then' after if-expression.");
				}
			} else {
				error("Expected an expression after 'if'.");
			}
		}
	} else if(token == Token::kwWhile) {
		eat();
		if(auto cond = parseInfixExpr()) {
			if(token == Token::opArrowR) {
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
	} else if(token == Token::opBackSlash) {
		eat();
		auto vars = many([=]{
			if(token == Token::VarID) {
				auto id = token.data.id;
				eat();
				return just(id);
			} else {
				error("expected variable name");
				return nothing<Id>();
			}
		});

		if(token == Token::opArrowR) {
			eat();
			if(auto e = parseInfixExpr()) {
				return build<LamExpr>(vars, e);
			} else {
				return error("expected expression");
			}
		} else {
			return error("expected '->'");
		}
	} else {
		return parseCallExpr();
	}

	return nullptr;
}

Expr* Parser::parseCallExpr() {
	/*
	 * fexp		→	[fexp] aexp		(function application)
	 *
	 * This function contains a special case;
	 * a bexp can be a construction expression, which looks like a normal function call.
	 * Instead of making the resolver search for patterns of "CallExpr (ConstructExpr type) args",
	 * we simply put the arguments inside the constructor.
	 */
	if(auto callee = parseAppExpr()) {
		// Parse any arguments applied to the callee.
		if(auto list = many([=] {return parseAppExpr();})) {
			// Special case for construction; see above.
			if(callee->type == Expr::Construct) {
				((ConstructExpr*)callee)->args = list;
				return callee;
			} else {
				return build<AppExpr>(callee, list);
			}
		} else {
			return callee;
		}
	} else {
		return error("Expected an expression.");
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

Expr* Parser::parseCaseExpr() {
	/*
	 * expr → 	case expr of alts
	 * alts	→	alt1 ; … ; altn	    		(n ≥ 1)
	 * alt	→	pat -> exp [where decls]
	 * 		|	pat gdpat [where decls]
	 * 		|		    					(empty alternative)
	 */
	if(token == Token::kwCase) {
		eat();
		if(auto exp = parseTypedExpr()) {
			if(token == Token::kwOf) {
				eat();
				auto alts = withLevel([=] {return sepBy1([=] {return parseAlt();}, Token::EndOfStmt);});
				return build<CaseExpr>(exp, alts);
			} else {
				error("Expected 'of' after case-expression.");
			}
		} else {
			error("Expected an expression after 'case'.");
		}
	} else {
		error("expected 'case'");
	}
	
	return nullptr;
}

Expr* Parser::parseBaseExpr() {
	/*
	 * bexp		→	qvar				(variable or function without args)
	 * 			|	qcon				(object construction)
	 *			|	literal
	 *			|	( exp )				(parenthesized expression)
	 *			|	{ exp, ..., exp }	(tuple construction / unit)
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
				return error("Expected ')' after '(' and an expression.");
			}
		} else {
			return error("Expected expression after '('.");
		}
	} else if(token == Token::BracketL) {
		return parseTupleConstruct();
	} else if(token == Token::ConID) {
		auto name = token.data.id;
		eat();
		return build<ConstructExpr>(build<Type>(Type::Con, name), nullptr);
	} else if(auto var = tryParse([=] {return parseVar();} )) {
		return build<VarExpr>(var());
	} else {
		return error("Expected an expression.");
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
				return error("Expected end of string format after this expression.");

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
	auto list = withLevel([=] {return sepBy1([=] {return parseDeclExpr(constant);}, Token::EndOfStmt);});
	if(!list) return error("Expected declaration after 'var' or 'let'");
	else if(!list->next) return list->item;
	else return build<MultiExpr>(list);
}

Expr* Parser::parseDeclExpr(bool constant) {
	/*
	 * declexpr		→	varid [= expr]
	 */
	if(token == Token::VarID) {
		auto id = token.data.id;
		eat();
		if(token == Token::opEquals) {
			eat();
			if(auto expr = parseTypedExpr()) {
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

Maybe<Alt> Parser::parseAlt() {
	/*
	 * alt	→	pat -> exp [where decls]
	 * 		|	pat gdpat [where decls]
	 * 		|		    					(empty alternative)
	 */
	auto pat = parsePattern();
	if(!pat) return Nothing;

	if(token == Token::opArrowR) eat();
	else {error("expected '->'"); return Nothing;}

	auto exp = parseTypedExpr();
	if(!exp) return Nothing;

	return Alt{pat, exp};
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
	if(auto list = sepBy1([=] {
		if(auto list = many1([=]{return parseAType();})) {
			if(list->next) {
				return (Type*)build<AppType>(list->item, list->next);
			} else {
				return list->item;
			}
		} else {
			return (Type*)nullptr;
		}
	}, Token::opArrowR)) {
		if (list->next) {
			return build<FunType>(list);
		} else {
			return list->item;
		}
	} else {
		return nullptr;
	}
}

Type* Parser::parseAType() {
	if(token == Token::VarSym) {
		auto name = lexer.GetContext().Find(token.data.id).name;
		if(name.length == 1 && name.ptr[0] == kPointerSigil) {
			eat();
			if(auto type = parseAType()) {
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
	} else if(token == Token::VarID) {
		auto id = token.data.id;
		eat();
		return build<Type>(Type::Gen, id);
	} else if(token == Token::BracketL) {
		// Also handles unit type.
		return parseTupleType();
	} else if(token == Token::ParenL) {
		eat();
		auto t = parseType();
		if(token == Token::ParenR) eat();
		else error("expected ')'");

		return t;
	}

	error("Expected a type.");
	return nullptr;
}

SimpleType* Parser::parseSimpleType() {
	if(token == Token::ConID) {
		auto id = token.data.id;
		eat();
		return build<SimpleType>(id, many([=]{
			if(token == Token::VarID) {
				auto id = token.data.id; eat(); return just(id);
			} else {
				return nothing<Id>();
			}
		}));
	} else {
		error("expected type name");
	}

	return nullptr;
}

Type* Parser::parseTupleType() {
    /*
     * tuptype  →   { tupfield1, ..., tupfieldn }       (n ≥ 0)
     */
	auto type = between([=] {
		auto l = sepBy([=] {return parseTupleField();}, Token::Comma);
		if(l) return (Type*)build<TupleType>(l);
		else return build<Type>(Type::Unit);
	}, Token::BracketL, Token::BracketR);

	if(type) return type;
	else return (Type*)error("Expected one or more tuple fields");
}

Expr* Parser::parseTupleConstruct() {
	auto expr = between([=] {
		auto l = sepBy([=] {return parseTupleConstructField();}, Token::Comma);
		if(l) return (Expr*)build<TupleConstructExpr>(nullptr, l);
		else return build<Expr>(Expr::Unit);
	}, Token::BracketL, Token::BracketR);

	if(expr) return expr;
	else return error("Expected one or more tuple fields");
}

Maybe<TupleField> Parser::parseTupleField() {
    /*
     * tupfield →   varid [: type]
     *          |   varid [= typedexpr]
     *          |   type [= typedexpr]
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

    // Parse default value.
    if(token == Token::opEquals) {
        eat();
        def = parseTypedExpr();
    }

	if(!type && !def) return Nothing;

    return TupleField{type, name, def};
}

Maybe<TupleField> Parser::parseTupleConstructField() {
	/*
     * tupcfield 	→  typedexpr
     *          	|   varid [= typedexpr]
     */

	Maybe<Id> name = Nothing;
	ExprRef def = nullptr;

	// If the token is a varid, it can either be a generic or named parameter, depending on the token after it.
	if(token == Token::VarID) {
		auto id = token.data.id;
		eat();
		if(token == Token::opEquals) {
			name = id;
			eat();
			def = parseTypedExpr();
		} else {
			def = build<VarExpr>(id);
		}
	} else {
		def = parseTypedExpr();
	}

	if(!def) return Nothing;
	return TupleField{nullptr, name, def};
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

Constr* Parser::parseConstr() {
	/*
	 * constr		→	conid atype1 … atypen		(n ≥ 0)
	 */
	if(token == Token::ConID) {
		auto name = token.data.id;
		eat();
		auto types = many([=] {return parseAType();});
		return build<Constr>(name, types);
	} else {
		error("expected constructor name");
	}

	return nullptr;
}

Pattern* Parser::parseLeftPattern() {
	if(token == Token::Literal) {
		auto p = build<LitPattern>(toLiteral(token));
		eat();
		return p;
	} else if(token == Token::kw_) {
		eat();
		return build<Pattern>(Pattern::Any);
	} else if(token == Token::VarID) {
		Id var = token.data.id;
		eat();
		if(token == Token::opAt) {
			eat();
			auto pat = parseLeftPattern();
			pat->asVar = var;
			return pat;
		} else {
			return build<VarPattern>(var);
		}
	} else if(token == Token::ParenL) {
		auto pat = parsePattern();
		if(token == Token::ParenR) eat();
		else error("expected ')'");

		return pat;
	} else if(token == Token::ConID) {
		// lpat can only contain a single constructor name.
		auto id = token.data.id;
		eat();
		return build<ConPattern>(id, nullptr);
	} else {
		error("expected pattern");
		return nullptr;
	}
}

Pattern* Parser::parsePattern() {
	if(token.singleMinus) {
		eat();
		if(token == Token::Integer || token == Token::Float) {
			auto lit = toLiteral(token);
			if(token == Token::Integer) lit.i = -lit.i;
			else lit.f = -lit.f;
			eat();
			return build<LitPattern>(lit);
		} else {
			error("expected integer or float literal");
			return nullptr;
		}
	} else if(token == Token::ConID) {
		auto id = token.data.id;
		eat();

		// Parse a pattern for each constructor element.
		auto list = many([=] {return parseLeftPattern();});
		return build<ConPattern>(id, list);
	} else {
		return parseLeftPattern();
	}
}

Expr* Parser::error(const char* text) {
	Core::LogError(text);
	return nullptr;
}

}} // namespace athena::ast