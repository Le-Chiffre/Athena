
#include "ast.h"
#include "lexer.h"

namespace athena {
namespace ast {

struct Printer {
	Printer(CompileContext& context) : context(context) {}

	void toString(ExprRef expr) {
		switch(expr.type) {
			case Expr::Lit: toString((const LitExpr&)expr); break;
			case Expr::Var: toString((const VarExpr&)expr); break;
			case Expr::App: toString((const AppExpr&)expr); break;
			case Expr::Infix: toString((const InfixExpr&)expr); break;
			case Expr::Prefix: toString((const PrefixExpr&)expr); break;
		}
	}

private:
	void makeIndent(bool lower, bool isLast) {
		char f, s;
		if(isLast) {
			f = '`';
			s = '-';
		} else {
			f = '|';
			s = '-';
		}

		if(lower) indentStart -= 2;
		if(indentStart) indentStack[indentStart-1] = ' ';

		indentStack[indentStart] = f;
		indentStack[indentStart+1] = s;
		indentStack[indentStart+2] = 0;
		indentStart += 2;
	}

	void toString(const LitExpr& e) {
		string += "LitExpr ";

		char buffer[32];
		Core::NumberToString(e.literal.d, buffer, 32);
		string += &buffer[0];
	}

	void toString(const VarExpr& e) {
		string += "VarExpr ";
		auto name = context.Find(e.name).name;
		string.Append(name.ptr, name.length);
	};

	void toString(const AppExpr& e) {
		string += "AppExpr ";
		toString(e.callee);
		for(auto arg : *e.args) {
			toString(arg);
		}
	}

	void toString(const InfixExpr& e) {
		string += "InfixExpr ";
		auto name = context.Find(e.op).name;
		string.Append(name.ptr, name.length);
		toString(e.lhs);
		toString(e.rhs);
	}

	void toString(const PrefixExpr& e) {
		string += "PrefixExpr ";
		auto name = context.Find(e.op).name;
		string.Append(name.ptr, name.length);
		toString(e.dst);
	}

	void toStringIntro(bool first, bool last) {
		string += '\n';
		makeIndent(first, last);
		string.Append(indentStack, indentStart);
	}

	void toString(ExprRef expr, bool first, bool last) {
		toStringIntro(first, last);
		toString(expr);
	}

	char indentStack[1024];
	uint indentStart = 0;

	CompileContext& context;
	Core::String string;
};

}} // namespace athena::ast
