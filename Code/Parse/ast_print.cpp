
#include "ast.h"
#include "lexer.h"
#include "parser.h"

namespace athena {
namespace ast {

struct Printer {
	Printer(CompileContext& context) : context(context) {}

	String toString(const Expr& expr) {
		switch(expr.type) {
			case Expr::Multi: toString((const MultiExpr&)expr); break;
			case Expr::Lit: toString((const LitExpr&)expr); break;
			case Expr::Var: toString((const VarExpr&)expr); break;
			case Expr::App: toString((const AppExpr&)expr); break;
			case Expr::Lam: toString((const LamExpr&)expr); break;
			case Expr::Infix: toString((const InfixExpr&)expr); break;
			case Expr::Prefix: toString((const PrefixExpr&)expr); break;
			case Expr::If: toString((const IfExpr&)expr); break;
			case Expr::MultiIf: toString((const MultiIfExpr&)expr); break;
			case Expr::Decl: toString((const DeclExpr&)expr); break;
			case Expr::While: toString((const WhileExpr&)expr); break;
			case Expr::Assign: toString((const AssignExpr&)expr); break;
			case Expr::Nested: toString((const NestedExpr&)expr); break;
			case Expr::Coerce: toString((const CoerceExpr&)expr); break;
			case Expr::Field: toString((const FieldExpr&)expr); break;
			case Expr::Construct: toString((const ConstructExpr&)expr); break;
			case Expr::TupleConstruct: toString((const TupleConstructExpr&)expr); break;
			case Expr::Format: toString((const FormatExpr&)expr); break;
			case Expr::Case: toString((const CaseExpr&)expr); break;
		}
		return string;
	}

	String toString(DeclRef decl) {
		switch(decl.kind) {
			case Decl::Function: toString((const FunDecl&)decl); break;
			case Decl::Type: toString((const TypeDecl&)decl); break;
			case Decl::Data: toString((const DataDecl&)decl); break;
			case Decl::Foreign: toString((const ForeignDecl&)decl); break;
		}
		return string;
	}

	String toString(ModuleRef mod) {
		string += "Module ";
		uint max = mod.declarations.Count();
		if(max) {
			makeLevel();
			for(uint i = 0; i < max - 1; i++)
				toString(*mod.declarations[i], false);
			toString(*mod.declarations[max-1], true);
			removeLevel();
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
		switch(e.literal.type) {
			case Literal::Int:
				Core::NumberToString(e.literal.i, buffer, 32);
				string += &buffer[0];
				break;
			case Literal::Float:
				Core::NumberToString(e.literal.f, buffer, 32);
				string += &buffer[0];
				break;
			case Literal::Char:
				string += e.literal.c;
				break;
			case Literal::String: {
				string += '"';
				auto name = context.Find(e.literal.s).name;
				string.Append(name.ptr, name.length);
				string += '"';
				break;
			}
			case Literal::Bool:
				if(e.literal.i) string += "True";
				else string += "False";
				break;
		}
	}

	void toString(const VarExpr& e) {
		string += "VarExpr ";
		auto name = context.Find(e.name).name;
		string.Append(name.ptr, name.length);
	};

	void toString(const AppExpr& e) {
		string += "AppExpr ";
		makeLevel();
		toString(*e.callee, false);
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
		toString(*e.lhs,  false);
		toString(*e.rhs, true);
		removeLevel();
	}

	void toString(const PrefixExpr& e) {
		string += "PrefixExpr ";
		auto name = context.Find(e.op).name;
		string.Append(name.ptr, name.length);
		makeLevel();
		toString(*e.dst, true);
		removeLevel();
	}

	void toString(const IfExpr& e) {
		string += "IfExpr ";
		makeLevel();
		toString(*e.cond, false);
		if(e.otherwise) {
			toString(*e.then, false);
			toString(*e.otherwise, true);
		} else {
			toString(*e.then, true);
		}
		removeLevel();
	}

	void toString(const MultiIfExpr& e) {
		string += "MultiIfExpr ";
		makeLevel();
		auto a = e.cases;
		while(a) {
			if(a->next) toString(*a->item, false);
			else toString(*a->item, true);
			a = a->next;
		}
		removeLevel();
	}

	void toString(const DeclExpr& e) {
		string += "DeclExpr ";
		auto name = context.Find(e.name).name;
		string.Append(name.ptr, name.length);
		if(e.constant) string += " <const> ";
		if(e.content) {
			makeLevel();
			toString(*e.content, true);
			removeLevel();
		} else {
			string += " <empty> ";
		}
	}

	void toString(const WhileExpr& e) {
		string += "WhileExpr";
		makeLevel();
		toString(*e.cond, false);
		toString(*e.loop, true);
		removeLevel();
	}

	void toString(const AssignExpr& e) {
		string += "AssignExpr ";
		makeLevel();
		toString(*e.target, false);
		toString(*e.value, true);
		removeLevel();
	}

	void toString(const NestedExpr& e) {
		string += "NestedExpr ";
		makeLevel();
		toString(*e.expr, true);
		removeLevel();
	}

	void toString(const CoerceExpr& e) {
		string += "CoerceExpr ";
		string += '(';
		toString(e.kind);
		string += ')';
		makeLevel();
		toString(*e.target, true);
		removeLevel();
	}

	void toString(const FieldExpr& e) {
		string += "FieldExpr ";
		makeLevel();
		toString(*e.field, false);
		toString(*e.target, true);
		removeLevel();
	}

	void toString(const ConstructExpr& e) {
		string += "ConstructExpr ";
		//auto name = context.Find(e.name).name;
		//string.Append(name.ptr, name.length);
	}

	void toString(const TupleConstructExpr& e) {
		string += "TupleConstructExpr ";
		//auto name = context.Find(e.name).name;
		//string.Append(name.ptr, name.length);
	}

	void toString(const FormatExpr& e) {
		string += "FormatExpr ";
		makeLevel();
		auto chunk = &e.format;
		while(chunk) {
			toString(chunk->item, !chunk->next);
			chunk = chunk->next;
		}
		removeLevel();
	}
	
	void toString(const CaseExpr& e) {
		string += "CaseExpr ";
		makeLevel();
		auto a = e.alts;
		while(a) {
			if(a->next) toString(a->item, false);
			else toString(a->item, true);
			a = a->next;
		}
		removeLevel();
	}

	void toString(const LamExpr& e) {
		string += "LamExpr (";
		if(e.args) {
			auto arg = e.args->fields;
			while(arg) {
				String name;
				if(arg->item.name)
					name = context.Find(arg->item.name()).name;
				else
					name = "<unnamed>";

				string.Append(name.ptr, name.length);
				if(arg->next) string += ", ";
				arg = arg->next;
			}
		}
		string += ')';

		makeLevel();
		toString(*e.body, true);
		removeLevel();
	}
	
	void toString(const Alt& alt, bool last) {
		toStringIntro(last);
		string += "alt: ";
		toString(*alt.expr);
	}

	void toString(const FunDecl& e) {
		string += "FunDecl ";
		auto name = context.Find(e.name).name;
		string.Append(name.ptr, name.length);
		string += '(';
		if(e.args) {
			auto arg = e.args->fields;
			while(arg) {
				if(arg->item.name)
					name = context.Find(arg->item.name()).name;
				else
					name = "<unnamed>";
				
				string.Append(name.ptr, name.length);
				if(arg->next) string += ", ";
				arg = arg->next;
			}
		}
		string += ')';

		if(e.body) {
			makeLevel();
			toString(*e.body, true);
			removeLevel();
		}
	}

	void toString(const TypeDecl& e) {
		string += "TypeDecl ";
		auto name = context.Find(e.type->name).name;
		string.Append(name.ptr, name.length);
		string += " = ";
		toString(e.target);
	}

	void toString(const DataDecl& e) {
		string += "DataDecl ";
		toString(*e.type);
		makeLevel();
		auto con = e.constrs;
		while(con) {
			if(con->next) toString(*con->item, false);
			else toString(*con->item, true);
			con = con->next;
		}
		removeLevel();
	}

	void toString(const ForeignDecl& e) {
		string += "ForeignDecl ";
		auto name = context.Find(e.importedName).name;
		string.Append(name.ptr, name.length);
		string += " : ";
		toString(e.type);
	}

	void toString(const Field& f, bool last) {
		toStringIntro(last);

		string += "Field ";
		auto name = context.Find(f.name).name;
		string.Append(name.ptr, name.length);
		string += ' ';
		if(f.constant) string += "<const> ";
		if(f.type) {
			toString(f.type);
		} else {
			makeLevel();
			toString(*f.content, true);
			removeLevel();
		}
	}

	void toString(const FormatChunk& f, bool last) {
		auto name = context.Find(f.string).name;
		if(f.format) {
			toString(*f.format, name.length ? false : last);
		}

		if(name.length) {
			toStringIntro(last);
			string += "LitExpr \"";
			string.Append(name.ptr, name.length);
			string += '"';
		}
	}

	void toString(const IfCase& c, bool last) {
		toStringIntro(last);
		string += "IfCase ";
		makeLevel();
		toString(*c.cond, false);
		toString(*c.then, true);
		removeLevel();
	}

	void toString(const SimpleType& t) {
		auto name = context.Find(t.name).name;
		if(name.length) {
			string.Append(name.ptr, name.length);
			string += ' ';
		}
	}

	void toString(const Constr& c, bool last) {
		auto name = context.Find(c.name).name;
		if(name.length) {
			toStringIntro(last);
			string += "Constructor ";
			string.Append(name.ptr, name.length);
		}
	}

	void toStringIntro(bool last) {
		string += '\n';
		makeIndent(last);
		string.Append(indentStack, indentStart);
	}

	void toString(const Expr& expr, bool last) {
		toStringIntro(last);
		toString(expr);
	}

	void toString(DeclRef decl, bool last) {
		toStringIntro(last);
		toString(decl);
	}

	void toString(TypeRef type) {
		string += "type: ";
		if(type->kind == Type::Unit) {
			string += "()";
		} else if(type->kind == Type::Tup){
			string += "tuple";
		} else if(type->kind == Type::Fun) {
			string += "fun";
		} else if(type->kind == Type::App) {
			string += "app ";
			toString(((AppType*)type)->base);
		} else {
			if(type->kind == Type::Ptr)
				string += Parser::kPointerSigil;

			auto name = context.Find(type->con).name;
			string.Append(name.ptr, name.length);
		}
	}

	char indentStack[1024];
	uint indentStart = 0;

	CompileContext& context;
	Core::String string;
};

String toString(const Expr& e, CompileContext& c) {
	Printer p{c};
	return p.toString(e);
}

String toString(DeclRef d, CompileContext& c) {
	Printer p{c};
	return p.toString(d);
}

String toString(ModuleRef m, CompileContext& c) {
	Printer p{c};
	return p.toString(m);
}

}} // namespace athena::ast
