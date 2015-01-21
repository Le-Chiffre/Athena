
#include "ast.h"
#include "lexer.h"

namespace athena {
namespace ast {

struct Printer {
	Printer(CompileContext& context) : context(context) {}

	String toString(ExprRef expr) {
		switch(expr.type) {
			case Expr::Multi: toString((const MultiExpr&)expr); break;
			case Expr::Lit: toString((const LitExpr&)expr); break;
			case Expr::Var: toString((const VarExpr&)expr); break;
			case Expr::App: toString((const AppExpr&)expr); break;
			case Expr::Infix: toString((const InfixExpr&)expr); break;
			case Expr::Prefix: toString((const PrefixExpr&)expr); break;
			case Expr::If: toString((const IfExpr&)expr); break;
			case Expr::Decl: toString((const DeclExpr&)expr); break;
		}
		return string;
	}

private:
	void makeIndent(bool isLast) {
		char f, s;
		if(isLast) {
			f = '`';
			s = '-';
		} else {
			f = '|';
			s = '-';
		}

		indentStack[indentStart-2] = f;
		indentStack[indentStart-1] = s;
	}

	void makeLevel() {
		if(indentStart) {
			indentStack[indentStart-1] = ' ';
			if(indentStack[indentStart-2] == '`') indentStack[indentStart-2] = ' ';
		}
		indentStack[indentStart] = ' ';
		indentStack[indentStart+1] = ' ';
		indentStack[indentStart+2] = 0;
		indentStart += 2;
	}

	void removeLevel() {
		indentStart -= 2;
	}

	void toString(const MultiExpr& e) {
		string += "MultiExpr ";
		makeLevel();
		auto expr = e.exprs;
		while(expr) {
			if(expr->next) toString(*expr->item, false);
			else toString(*expr->item, true);
			expr = expr->next;
		}
		removeLevel();
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
		makeLevel();
		toString(e.callee, false);
		auto arg = e.args;
		while(arg) {
			if(arg->next) toString(*arg->item, false);
			else toString(*arg->item, true);
			arg = arg->next;
		}
		removeLevel();
	}

	void toString(const InfixExpr& e) {
		string += "InfixExpr ";
		auto name = context.Find(e.op).name;
		string.Append(name.ptr, name.length);
		makeLevel();
		toString(e.lhs,  false);
		toString(e.rhs, true);
		removeLevel();
	}

	void toString(const PrefixExpr& e) {
		string += "PrefixExpr ";
		auto name = context.Find(e.op).name;
		string.Append(name.ptr, name.length);
		makeLevel();
		toString(e.dst, true);
		removeLevel();
	}

	void toString(const IfExpr& e) {
		string += "IfExpr ";
		makeLevel();
		toString(e.cond, false);
		if(e.otherwise) {
			toString(e.then, false);
			toString(*e.otherwise, true);
		} else {
			toString(e.then, true);
		}
		removeLevel();
	}

	void toString(const DeclExpr& e) {
		string += "DeclExpr ";
		auto name = context.Find(e.name).name;
		string.Append(name.ptr, name.length);
		makeLevel();
		toString(e.content, true);
		removeLevel();
	}

	void toStringIntro(bool last) {
		string += '\n';
		makeIndent(last);
		string.Append(indentStack, indentStart);
	}

	void toString(ExprRef expr, bool last) {
		toStringIntro(last);
		toString(expr);
	}

	char indentStack[1024];
	uint indentStart = 0;

	CompileContext& context;
	Core::String string;
};

String toString(ExprRef e, CompileContext& c) {
	Printer p{c};
	return p.toString(e);
}

}} // namespace athena::ast
