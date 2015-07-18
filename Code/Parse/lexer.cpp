
#include "lexer.h"

namespace athena {
namespace ast {

/**
 * Compares source code to a string constant.
 * @param source The source code to compare. Must point to the first character of the string.
 * If the strings are equal, source is set to the first character after the part that is equal.
 * @param constant The constant string to compare to.
 */
bool CompareConstString(const char*& source, const char* constant)
{
	auto src = source;
	while(*constant == *source)
	{
		constant++;
		source++;
	}

	if(*constant == 0) return true;
	else {
		source = src;
		return false;
	}
}

/**
 * Parses the provided character as a hexit, to an integer in the range 0..15.
 * @return The parsed number. Returns Nothing if the character is not a valid number.
 */
Maybe<U32> ParseHexit(WChar32 c)
{
	//We use a small lookup table for this,
	//since the number of branches would be ridiculous otherwise.
	static const Byte table[] = {
		0,  1,  2,  3,  4,  5,  6,  7,  8,  9,	/* 0..9 */
		255,255,255,255,255,255,255,			/* :..@ */
		10, 11, 12, 13, 14, 15,					/* A..F */
		255,255,255,255,255,255,255,			/* G..` */
		255,255,255,255,255,255,255,
		255,255,255,255,255,255,255,
		255,255,255,255,255,255,
		10, 11, 12, 13, 14, 15,					/* a..f */
	};

	//Anything lower than '0' will underflow, giving some large number above 54.
	U32 ch = c;
	U32 index = ch - '0';

	if(index > 54) return Nothing();

	U32 res = table[index];
	if(res > 15) return Nothing();

	return Just(res);
}

/**
 * Parses the provided character as an octit, to an integer in the range 0..7.
 * @return The parsed number. Returns Nothing if the character is not a valid number.
 */
Maybe<U32> ParseOctit(WChar32 c)
{
	//Anything lower than '0' will underflow, giving some large number above 7.
	U32 ch = c;
	U32 index = ch - '0';

	if(index > 7) return Nothing();
	else return Just(index);
}

/**
 * Parses the provided character as a digit, to an integer in the range 0..9.
 * @return The parsed number. Returns Nothing if the character is not a valid number.
 */
Maybe<U32> ParseDigit(WChar32 c)
{
	U32 ch = c;
	U32 index = ch - '0';
	if(index > 9) return Nothing();
	else return Just(index);
}

/**
 * Parses the provided character as a bit, to an integer in the range 0..1.
 * @return The parsed number. Returns Nothing if the character is not a valid number.
 */
Maybe<U32> ParseBit(WChar32 c)
{
	U32 ch = c;
	U32 index = ch - '0';
	if(index > 1) return Nothing();
	else return Just(index);
}

/**
 * Parses the provided character to a single numeric in the provided base.
 * Supported bases are 2, 8, 10, 16.
 * Returns Nothing if the character doesn't represent a numeric in Base.
 */
template<U32 Base>
Maybe<U32> ParseNumericAtom(WChar32);

template<>
Maybe<U32> ParseNumericAtom<16>(WChar32 p) {return ParseHexit(p);}

template<>
Maybe<U32> ParseNumericAtom<10>(WChar32 p) {return ParseDigit(p);}

template<>
Maybe<U32> ParseNumericAtom<8>(WChar32 p) {return ParseOctit(p);}

template<>
Maybe<U32> ParseNumericAtom<2>(WChar32 p) {return ParseBit(p);}

/**
 * Returns the name of the provided numeric base.
 * Supported bases are 2, 8, 10, 16.
 */
template<U32 Base>
const char* GetBaseName();

template<>
const char* GetBaseName<16>() {return "hexadecimal";}

template<>
const char* GetBaseName<10>() {return "decimal";}

template<>
const char* GetBaseName<8>() {return "octal";}

template<>
const char* GetBaseName<2>() {return "binary";}

/**
 * Parses a character literal from a text sequence with a certain base.
 * Supported bases are 2, 8, 10, 16.
 * @param p A pointer to the first numeric character.
 * This pointer is increased to the first character after the number.
 * @param numChars The maximum number of characters to parse.
 * @param max The maximum value supported. If the literal exceeds this value, a warning is generated.
 * @param diag The diagnostics to which problems will be written.
 * @return The code point generated from the sequence.
 */
template<U32 Base>
WChar32 ParseIntSequence(const char*& p, U32 numChars, U32 max, Diagnostics* diag)
{
    U32 res = 0;
	for(U32 i=0; i<numChars; i++) {
		char c = *p;
		if(auto num = ParseNumericAtom<Base>(c)) {
			res *= Base;
			res += num.force();
			p++;
		} else {
			break;
		}
	}

	if(res > max) diag->Warning("%@ escape sequence out of range", GetBaseName<Base>());
	return res;
}

/**
 * Parses an integer literal with a custom base.
 * Supported bases are 2, 8, 10, 16.
 * @param p A pointer to the first numeric character.
 * This pointer is increased to the first character after the number.
 * @return The parsed number.
 */
template<U32 Base>
U32 ParseIntLiteral(const char*& p)
{
	U32 res = 0;
	while(auto c = ParseNumericAtom<Base>(*p)) {
		res *= Base;
		res += c.force();
		p++;
	}
	return res;
}

/**
 * Parses a floating point literal.
 * The literal must have the following form:
 *    decimal -> digit{digit}
 *    exponent -> (e|E)[+|-] decimal
 *    float -> decimal . decimal[exponent] | decimal exponent
 * @param p A pointer to the first numeric character.
 * This pointer is increased to the first character after the number.
 * @return The parsed number.
 */
double ParseFloatLiteral(const char*& p)
{
	return Tritium::read<Float>(p);
}

/**
 * Returns true if this is an uppercase character.
 * TODO: Currently, only characters in the ASCII range are considered.
 */
bool IsUpperCase(WChar32 c)
{
	U32 ch = c;
	U32 index = ch - 'A';
	return index <= ('Z' - 'A');
}

/**
 * Returns true if this is a lowercase character.
 * TODO: Currently, only characters in the ASCII range are considered.
 */
bool IsLowerCase(WChar32 c)
{
	U32 ch = c;
	U32 index = ch - 'a';
	return index <= ('z' - 'a');
}

/**
 * Returns true if this is a lowercase or uppercase character.
 */
bool IsAlpha(WChar32 c)
{
	return IsUpperCase(c) || IsLowerCase(c);
}

/**
 * Returns true if this is a bit.
 */
bool IsBit(WChar32 c)
{
	U32 ch = c;
	U32 index = ch - '0';
	return index <= 1;
}

/**
 * Returns true if this is a digit.
 */
bool IsDigit(WChar32 c)
{
	U32 ch = c;
	U32 index = ch - '0';
	return index <= 9;
}

/**
 * Returns true if this is an octit.
 */
bool IsOctit(WChar32 c)
{
	U32 ch = c;
	U32 index = ch - '0';
	return index <= 7;
}

/**
 * Returns true if this is a hexit.
 */
bool IsHexit(WChar32 c)
{
	//We use a small lookup table for this,
	//since the number of branches would be ridiculous otherwise.
	static const bool table[] = {
		true, true, true, true, true, true, true, true, true, true,	/* 0..9 */
		false,false,false,false,false,false,false,					/* :..@ */
		true, true, true, true, true, true,							/* A..F */
		false,false,false,false,false,false,false,					/* G..` */
		false,false,false,false,false,false,false,
		false,false,false,false,false,false,false,
		false,false,false,false,false,false,
		true, true, true, true, true, true,							/* a..f */
	};

	//Anything lower than '0' will underflow, giving some large number above 54.
	U32 ch = c;
	U32 index = ch - '0';

	if(index > 54) return false;
	else return table[index];
}

/**
 * Returns true if the provided character is alpha-numeric.
 */
bool IsAlphaNumeric(WChar32 c)
{
	return IsAlpha(c) || IsDigit(c);
}

/**
 * Returns true if the provided character is valid as part of an identifier (VarID or ConID).
 */
bool IsIdentifier(WChar32 c)
{
	static const bool table[] = {
		false, /* ' */
		false, /* ( */
		false, /* ) */
		false, /* * */
		false, /* + */
		false, /* , */
		false, /* - */
		false, /* . */
		false, /* / */
		true, true, true, true, true, true, true, true, true, true,	/* 0..9 */
		false,false,false,false,false,false,false,					/* :..@ */
		true, true, true, true, true, true, true, true, true, true, /* A..Z */
		true, true, true, true, true, true, true, true, true, true,
		true, true, true, true, true, true,
		false, /* [ */
		false, /* \ */
		false, /* ] */
		false, /* ^ */
		true, /* _ */
		false, /* ` */
		true, true, true, true, true, true, true, true, true, true, /* a..z */
		true, true, true, true, true, true, true, true, true, true,
		true, true, true, true, true, true
	};

	//Anything lower than ' will underflow, giving some large number above 83.
	U32 ch = c;
	U32 index = ch - '\'';

	if(index > 83) return false;
	else return table[index];
}

/**
 * Checks if the provided character is a symbol, as specified in section 2.2 of the Haskell spec.
 * TODO: Currently, only characters in the ASCII range are considered valid.
 */
bool IsSymbol(WChar32 c)
{
	//We use a small lookup table for this,
	//since the number of branches would be ridiculous otherwise.
	static const bool table[] = {
		true, /* ! */
		false, /* " */
		true, /* # */
		true, /* $ */
		true, /* % */
		true, /* & */
		false, /* ' */
		false, /* ( */
		false, /* ) */
		true, /* * */
		true, /* + */
		false, /* , */
		true, /* - */
		true, /* . */
		true, /* / */
		false, false, false, false, false, false, false, false, false, false, /* 0..9 */
		true, /* : */
		false, /* ; */
		true, /* < */
		true, /* = */
		true, /* > */
		true, /* ? */
		true, /* @ */
		false, false, false, false, false, false, false, false, false, false, /* A..Z */
		false, false, false, false, false, false, false, false, false, false,
		false, false, false, false, false, false,
		false, /* [ */
		true, /* \ */
		false, /* ] */
		true, /* ^ */
		false, /* _ */
		false, /* ` */
		false, false, false, false, false, false, false, false, false, false, /* a..z */
		false, false, false, false, false, false, false, false, false, false,
		false, false, false, false, false, false,
		false, /* { */
		true, /* | */
		false, /* } */
		true /* ~ */
	};

	U32 ch = c;
	U32 index = ch - '!';
	if(index > 93) return false;
	else return table[index];
}

/**
 * Checks if the provided character is special, as specified in section 2.2 of the Haskell spec.
 */
bool IsSpecial(WChar32 c)
{
	//We use a small lookup table for this,
	//since the number of branches would be ridiculous otherwise.
	static const bool table[] = {
		true, /* ( */
		true, /* ) */
		false, /* * */
		false, /* + */
		true, /* , */
		false, /* - */
		false, /* . */
		false, /* / */
		false, false, false, false, false, false, false, false, false, false, /* 0..9 */
		false, /* : */
		true, /* ; */
		false, /* < */
		false, /* = */
		false, /* > */
		false, /* ? */
		false, /* @ */
		false, false, false, false, false, false, false, false, false, false, /* A..Z */
		false, false, false, false, false, false, false, false, false, false,
		false, false, false, false, false, false,
		true, /* [ */
		false, /* \ */
		true, /* ] */
		false, /* ^ */
		false, /* _ */
		true, /* ` */
		false, false, false, false, false, false, false, false, false, false, /* a..z */
		false, false, false, false, false, false, false, false, false, false,
		false, false, false, false, false, false,
		true, /* { */
		false, /* | */
		true /* } */
	};

	U32 ch = c;
	U32 index = ch - '(';
	if(index > 85) return false;
	else return table[index];
}

/**
 * Checks if the provided character is white, as specified in section 2.2 of the Haskell spec.
 * TODO: Currently, only characters in the ASCII range are considered valid.
 */
bool IsWhiteChar(WChar32 c)
{
	//Spaces are handled separately.
	//All other white characters are in the same range.
	//Anything lower than TAB will underflow, giving some large number above 4.
	U32 ch = c;
	U32 index = ch - 9;
	return index <= 4 || c == ' ';
}

/**
 * Checks if the provided character is a graphic, as specified in section 2.2 of the Haskell spec.
 * TODO: Currently, only characters in the ASCII range are considered valid.
 */
bool IsGraphic(WChar32 c)
{
	U32 ch = c;
	U32 index = ch - '!';
	return index <= 93;
}

//------------------------------------------------------------------------------

Lexer::Lexer(CompileContext& context, const char* text, Token* tok) :
	mToken(tok), mText(text), mP(text), mL(text), mNewItem(true), mContext(context)
{
	// The first indentation level of a file should be 0.
	mIdent = 0;
}

Token* Lexer::Next()
{
	ParseToken();
	return mToken;
}

WChar32 Lexer::NextCodePoint()
{
    WChar32 c;
	if(Tritium::Unicode::convertNextPoint(mP, &c)) {
		return c;
	} else {
		mDiag.Warning("Invalid UTF-8 sequence");
		return ' ';
	}
}

void Lexer::NextLine()
{
	mL = mP + 1;
	mLine++;
	mTabs = 0;
}

bool Lexer::WhiteChar_UpdateLine()
{
	if(*mP == '\n')
	{
		NextLine();
		return true;
	}

	if(*mP == '\t')
	{
		mTabs++;
		return true;
	}

	return IsWhiteChar(*mP);
}

void Lexer::SkipWhitespace()
{
	auto& p = mP;
	while(*p)
	{
		//Skip whitespace.
		if(!WhiteChar_UpdateLine())
		{
			//Check for single-line comments.
			if(*p == '-' && p[1] == '-' && !IsSymbol(p[2]))
			{
				//Skip the current line.
				p += 2;
				while(*p && *p != '\n') p++;

				//If this is a newline, we update the location.
				//If it is the file end, the caller will take care of it.
				if(*p == '\n')
				{
					NextLine();
					p++;
				}
			}

			//Check for multi-line comments.
			else if(*p == '{' && p[1] == '-')
			{
				//The current nested comment depth.
				U32 level = 1;

				//Skip until the comment end.
				p += 2;
				while(*p)
				{
					//Update the source location if needed.
					if(*p == '\n') NextLine();

					//Check for nested comments.
					if(*p == '{' && p[1] == '-') level++;

					//Check for comment end.
					if(*p == '-' && p[1] == '}')
					{
						level--;
						if(level == 0)
						{
							p += 2;
							break;
						}
					}
				}

				//mP now points to the first character after the comment, or the file end.
				//Check if the comments were nested correctly.
				if(level)
					mDiag.Warning("Incorrectly nested comment: missing %@ comment terminator(s).", level);
			}

			//No comment or whitespace - we are done.
			break;
		}

		//Check the next character.
		p++;
	}
}

String Lexer::ParseStringLiteral()
{
	//There is no real limit on the length of a string literal, so we use a dynamic array while parsing.
	Array<char> chars(128);

	mP++;
	while(1) {
		if(*mP == '\\') {
			//This is an escape sequence or gap.
			mP++;
			if(WhiteChar_UpdateLine()) {
				//This is a gap - we skip characters until the next '\'.
				//Update the current source line if needed.
				mP++;
				while(WhiteChar_UpdateLine()) mP++;

				if(*mP != '\\') {
					//The first character after a gap must be '\'.
					mDiag.Warning("Missing gap end in string literal");
				}

				//Continue parsing the string.
				mP++;
			} else {
				chars << ParseEscapedLiteral();
			}
		} else if(*mP == kFormatStart) {
			// Start a string format sequence.
			mFormatting = 1;
			mP++;
			break;
		} else {
			if(*mP == '\"') {
				//Terminate the string.
				mP++;
				break;
			} else if(!*mP || *mP == '\n') {
				//If the line ends without terminating the string, we issue a warning.
				mDiag.Warning("Missing terminating quote in string literal");
				break;
			} else {
				//Add this UTF-8 character to the string.
				chars << NextCodePoint();
			}
		}
	}

	//Create a new buffer for this string.
	U32 count = chars.size();
	auto buffer = (char*)Alloc(count * sizeof(char));
	Tritium::copy(chars.begin().p, buffer, count);
	return {buffer, chars.size()};
}

WChar32 Lexer::ParseCharLiteral()
{
	mP++;
    WChar32 c;

	if(*mP == '\\') {
		//This is an escape sequence.
		mP++;
		c = ParseEscapedLiteral();
	} else {
		//This is a char literal.
		c = NextCodePoint();
	}

	//Ignore any remaining characters in the literal.
	//It needs to end on this line.
	if(*mP++ != '\'') {
		mDiag.Warning("Multi-character character constant");
		while(*mP != '\'') {
			if(*mP == '\n' || *mP == 0) {
				mDiag.Warning("Missing terminating ' character in char literal");
				break;
			}
			mP++;
		}
	}
	return c;
}

WChar32 Lexer::ParseEscapedLiteral()
{
	char c = *mP++;
	switch(c)
	{
		case '{':
			// The left brace is used to start a formatting sequence.
			// Escaping it will print a normal brace.
			return '{';
		case 'a':
			return '\a';
		case 'b':
			return '\b';
		case 'f':
			return '\f';
		case 'n':
			return '\n';
		case 'r':
			return '\r';
		case 't':
			return '\t';
		case 'v':
			return '\v';
		case '\\':
			return '\\';
		case '\'':
			return '\'';
		case '\"':
			return '\"';
		case '0':
			return 0;
		case 'x':
			//Hexadecimal literal.
			if(!ParseHexit(*mP)) {
				mDiag.Error("\\x used with no following hex digits");
				return ' ';
			}
			return ParseIntSequence<16>(mP, 8, 0xffffffff, &mDiag);
		case 'o':
			//Octal literal.
			if(!ParseOctit(*mP)) {
				mDiag.Error("\\o used with no following octal digits");
				return ' ';
			}
			return ParseIntSequence<8>(mP, 16, 0xffffffff, &mDiag);
		default:
			if(IsDigit(c)) {
				return ParseIntSequence<10>(mP, 10, 0xffffffff, &mDiag);
			} else {
				mDiag.Warning("Unknown escape sequence '%@'", c);
				return ' ';
			}
	}
}

void Lexer::ParseNumericLiteral()
{
	auto& p = mP;
	auto& tok = *mToken;
	tok.type = Token::Integer;
	tok.kind = Token::Literal;

	//Parse the type of this literal.
	//HcS mode also supports binary literals.
	if(p[1] == 'b' || p[1] == 'B') {
		if(IsBit(p[2])) {
			//This is a binary literal.
			p += 2;
			tok.data.integer = ParseIntLiteral<2>(p);
		} else goto parseInt;
	} else if(p[1] == 'o' || p[1] == 'O') {
		if(IsOctit(p[2])) {
			//This is an octal literal.
			p += 2;
			tok.data.integer = ParseIntLiteral<8>(p);
		} else goto parseInt;
	} else if(p[1] == 'x' || p[1] == 'X') {
		if(IsHexit(p[2])) {
			//This is a hexadecimal literal.
			p += 2;
			tok.data.integer = ParseIntLiteral<16>(p);
		} else goto parseInt;
	} else {
		//Check for a dot or exponent to determine if this is a float.
		auto d = p + 1;
		while(1) {
			if(*d == '.') {
				//The first char after the dot must be numeric, as well.
				if(IsDigit(d[1])) break;
			} else if(*d == 'e' || *d == 'E') {
				//This is an exponent. If it is valid, the next char needs to be a numeric,
				//with an optional sign in-between.
				if(d[1] == '+' || d[1] == '-') d++;
				if(IsDigit(d[1])) break;
			} else if(!IsDigit(*d)) {
				//This wasn't a valid float.
				goto parseInt;
			}

			d++;
		}

		//Parse a float literal.
		tok.type = Token::Float;
		tok.data.floating = ParseFloatLiteral(p);
	}

	return;

parseInt:

	//Parse a normal integer.
	tok.data.integer = ParseIntLiteral<10>(p);
}

void Lexer::ParseSymbol()
{
	auto& tok = *mToken;
	auto& p = mP;

	bool sym1 = IsSymbol(p[1]);
	bool sym2 = sym1 && IsSymbol(p[2]);

	//Instead of setting this in many different cases, we make it the default and override it later.
	tok.kind = Token::Keyword;

	if(!sym1) {
		//Check for various reserved operators of length 1.
		if(*p == ':') {
			//Single colon.
			tok.type = Token::opColon;
		} else if(*p == '.') {
			// Single dot.
			tok.type = Token::opDot;
		} else if(*p == '=') {
			//This is the reserved Equals operator.
			tok.type = Token::opEquals;
		} else if(*p == '\\') {
			//This is the reserved backslash operator.
			tok.type = Token::opBackSlash;
		} else if(*p == '|') {
			//This is the reserved bar operator.
			tok.type = Token::opBar;
		} else if(*p == '$') {
			// This is the reserved dollar operator.
			tok.type = Token::opDollar;
		} else if(*p == '@') {
			//This is the reserved at operator.
			tok.type = Token::opAt;
		} else if(*p == '~') {
			//This is the reserved tilde operator.
			tok.type = Token::opTilde;
		} else {
			//This is a variable operator.
			tok.kind = Token::Identifier;
		}
	} else if(!sym2) {
		//Check for various reserved operators of length 2.
		if(*p == ':' && p[1] == ':') {
			//This is the reserved ColonColon operator.
			tok.type = Token::opColonColon;
		} else if(*p == '=' && p[1] == '>') {
			//This is the reserved double-arrow operator.
			tok.type = Token::opArrowD;
		} else if(*p == '.' && p[1] == '.') {
			//This is the reserved DotDot operator.
			tok.type = Token::opDotDot;
		}  else if(*p == '<' && p[1] == '-') {
			//This is the reserved arrow-left operator.
			tok.type = Token::opArrowL;
		} else if(*p == '-' && p[1] == '>') {
			//This is the reserved arrow-right operator.
			tok.type = Token::opArrowR;
		} else {
			//This is a variable operator.
			tok.kind = Token::Identifier;
		}
	} else {
		//This is a variable operator.
		tok.kind = Token::Identifier;
	}


	if(tok.kind == Token::Identifier) {
		//Check if this is a constructor.
		if(*p == ':') {
			tok.type = Token::ConSym;
		} else {
			tok.type = Token::VarSym;
		}

		//Parse a symbol sequence.
		//Get the length of the sequence, we already know that the first one is a symbol.
		Size count = 1;
		auto start = p;
		while(IsSymbol(*(++p))) count++;

		//Check for a single minus operator - used for parser optimization.
		if(count == 1 && *start == '-') {
            tok.singleMinus = true;
        } else {
            tok.singleMinus = false;
        }

		//Convert to UTF-32 and save in the current qualified name..
		mQualifier.name = String{start, count};
	} else {
		//Skip to the next token.
		if(sym1) p += 2;
		else p++;
	}
}

bool Lexer::ParseUniSymbol()
{
	auto& tok = *mToken;
	auto p = mP;
    WChar32 ch;
	if(!Tritium::Unicode::convertNextPoint(p, &ch)) {
		mDiag.Warning("Source code must be valid UTF-8.");
		return false;
	}

	if(ch > 255) return false;

	bool handled = false;

	if(ch == U'→') {
		tok.type = Token::opArrowR;
		tok.kind = Token::Keyword;
		handled = true;
	} else if(ch == U'←') {
		tok.type = Token::opArrowL;
		tok.kind = Token::Keyword;
		handled = true;
	} else if(ch == U'λ') {
		tok.type = Token::opBackSlash;
		tok.kind = Token::Keyword;
		handled = true;
	} else if(ch == U'≤') {
		tok.type = Token::VarSym;
		tok.kind = Token::Identifier;
		mQualifier.name = String{"<="};
		handled = true;
	} else if(ch == U'≥') {
		tok.type = Token::VarSym;
		tok.kind = Token::Identifier;
		mQualifier.name = String{">="};
		handled = true;
	} else if(ch == U'≠') {
		tok.type = Token::VarSym;
		tok.kind = Token::Identifier;
		mQualifier.name = String{"!="};
		handled = true;
	}

	if(handled) {
		mP = p;
	}

	return handled;
}

void Lexer::ParseSpecial()
{
	auto& tok = *mToken;
	tok.kind = Token::Special;
	tok.type = (Token::Type)*mP++;
}

void Lexer::ParseQualifier()
{
	auto& p = mP;
	auto& tok = *mToken;

	auto start = p;
	Size length = 1;
	tok.kind = Token::Identifier;
	tok.type = Token::ConID;

	auto q = &mQualifier.qualifier;

parseQ:
	while(IsIdentifier(*(++p))) length++;

	String str = String(start, length);
	if(*p == '.')
	{
		bool u = IsUpperCase(p[1]);
		bool l = IsLowerCase(p[1]) || p[1] == '_';
		bool s = IsSymbol(p[1]);

		//If the next character is a valid identifier or symbol,
		//we add this qualifier to the list and parse the remaining characters.
		//Otherwise, we parse as a ConID.
		if(u || l || s)
		{
			*q = New<Qualified>();
			(*q)->name = str;
			q = &(*q)->qualifier;

			p++;
			start = p;
			length = 0;
		}
		else
		{
			goto makeCon;
		}

		//If the next character is upper case, we either have a ConID or another qualifier.
		if(u)
		{
			goto parseQ;
		}

		//If the next character is lowercase, we either have a VarID or keyword.
		else if(l)
		{
			ParseVariable();

			//If this was a keyword, we parse as a constructor and dot operator instead.
			if(tok.kind == Token::Keyword)
			{
				p = start;
				goto makeCon;
			}
		}

		//If the next character is a symbol, we have a VarSym or ConSym.
		else if(s)
		{
			//We have a VarSym or ConSym.
			ParseSymbol();
		}
	}
	else
	{
	makeCon:
		//We have a ConID.
		mQualifier.name = str;
	}
};

void Lexer::ParseVariable()
{
	auto& p = mP;
	auto& tok = *mToken;
	tok.type = Token::VarID;
	tok.kind = Token::Identifier;

	//First, check if we have a reserved keyword.
	auto c = p + 1;
	switch(*p)
	{
		case '_':
			tok.type = Token::kw_;
			break;
		case 'c':
			if(CompareConstString(c, "ase")) tok.type = Token::kwCase;
			else if(CompareConstString(c, "lass")) tok.type = Token::kwClass;
			break;
		case 'd':
			if(CompareConstString(c, "ata")) tok.type = Token::kwData;
			else if(CompareConstString(c, "efault")) tok.type = Token::kwDefault;
			else if(CompareConstString(c, "eriving")) tok.type = Token::kwDeriving;
			else if(*c == 'o') {c++; tok.type = Token::kwDo;}
			break;
		case 'e':
			if(CompareConstString(c, "lse")) tok.type = Token::kwElse;
			break;
		case 'f':
			if(CompareConstString(c, "oreign")) tok.type = Token::kwForeign;
			else if(*c == 'o' && c[1] == 'r') {c += 2; tok.type = Token::kwFor;}
			break;
		case 'i':
			if(*c == 'f') {c++; tok.type = Token::kwIf;}
			else if(CompareConstString(c, "mport")) tok.type = Token::kwImport;
			else if(*c == 'n' && !IsIdentifier(c[1])) {c++; tok.type = Token::kwIn;}
			else if(CompareConstString(c, "nfix")) {
				if(*c == 'l') {c++; tok.type = Token::kwInfixL;}
				else if(*c == 'r') {c++; tok.type = Token::kwInfixR;}
				else tok.type = Token::kwInfix;
			} else if(CompareConstString(c, "nstance")) tok.type = Token::kwInstance;
			break;
		case 'l':
			if(*c == 'e' && c[1] == 't') {c += 2; tok.type = Token::kwLet;}
			break;
		case 'm':
			if(CompareConstString(c, "odule")) tok.type = Token::kwModule;
			break;
		case 'n':
			if(CompareConstString(c, "ewtype")) tok.type = Token::kwNewType;
			break;
		case 'o':
			if(*c == 'f') {c++; tok.type = Token::kwOf;}
			break;
		case 'p':
			if(CompareConstString(c, "refix")) tok.type = Token::kwPrefix;
			break;
        case 'r':
            if(mContext.settings.mode == CompileShader && CompareConstString(c, "esource")) {
                tok.type = Token::kwResource;
            }
            break;
		case 't':
			if(CompareConstString(c, "hen")) tok.type = Token::kwThen;
			else if(CompareConstString(c, "ype")) tok.type = Token::kwType;
			break;
		case 'v':
			if(*c == 'a' && c[1] == 'r') {c += 2; tok.type = Token::kwVar;}
		case 'w':
			if(CompareConstString(c, "here")) tok.type = Token::kwWhere;
			else if(CompareConstString(c, "hile")) tok.type = Token::kwWhile;
			break;
	}

	//We have to read the longest possible lexeme.
	//If a reserved keyword was found, we check if a longer lexeme is possible.
	if(tok.type != Token::VarID)
	{
		if(IsIdentifier(*c)) {
			tok.type = Token::VarID;
		} else {
			p = c;
			tok.kind = Token::Keyword;
			return;
		}
	}

	//Read the identifier name.
	U32 length = 1;
	auto start = p;
	while(IsIdentifier(*(++p))) length++;

	mQualifier.name = {start, length};
};

void Lexer::ParseToken()
{
	auto& tok = *mToken;
	auto& p = mP;
	auto b = p;

parseT:
	//This needs to be reset manually.
	mQualifier.qualifier = nullptr;

	// Check if we are inside a string literal.
	if(mFormatting == 3) {
		tok.sourceColumn = (U32)(p - mL) + mTabs * (kTabWidth - 1);
		tok.sourceLine = mLine;
		mFormatting = 0;
		goto stringLit;
	} else {
		//Skip any whitespace and comments.
		SkipWhitespace();

		tok.sourceColumn = (U32)(p - mL) + mTabs * (kTabWidth - 1);
		tok.sourceLine = mLine;
	}

	//Check for the end of the file.
	if(!*p)
	{
		tok.kind = Token::Special;
		if(mBlockCount) tok.type = Token::EndOfBlock;
		else tok.type = Token::EndOfFile;
	}

	//Check if we need to insert a layout token.
	else if(tok.sourceColumn == mIdent && !mNewItem)
	{
		tok.type = Token::EndOfStmt;
		tok.kind = Token::Special;
		mNewItem = true;
		goto newItem;
	}

	//Check if we need to end a layout block.
	else if(tok.sourceColumn < mIdent)
	{
		tok.type = Token::EndOfBlock;
		tok.kind = Token::Special;
	}

	// Check for start of string formatting.
	else if(mFormatting == 1)
	{
		tok.kind = Token::Special;
		tok.type = Token::StartOfFormat;
		mFormatting = 2;
	}

		// Check for end of string formatting.
	else if(mFormatting == 2 && *p == kFormatEnd)
	{
		// Issue a format end and make sure the next token is parsed as a string literal.
		// Don't skip the character - ParseStringLiteral skips one at the beginning.
		tok.kind = Token::Special;
		tok.type = Token::EndOfFormat;
		mFormatting = 3;
	}

	//Check for integral literals.
	else if(IsDigit(*p))
	{
		ParseNumericLiteral();
	}

	//Check for character literals.
	else if(*p == '\'')
	{
		tok.data.character = ParseCharLiteral();
		tok.kind = Token::Literal;
		tok.type = Token::Char;
	}

	//Check for string literals.
	else if(*p == '\"')
	{
stringLit:
		//Since string literals can span multiple lines, this may update mLocation.line.
		tok.type = Token::String;
		tok.kind = Token::Literal;
		tok.data.id = mContext.AddUnqualifiedName(ParseStringLiteral());
	}

	//Check for special operators.
	else if(IsSpecial(*p))
	{
		ParseSpecial();
	}

	//Parse symbols.
	else if(IsSymbol(*p))
	{
		ParseSymbol();
		tok.data.id = mContext.AddUnqualifiedName(mQualifier.name);
	}

	// Parse special unicode symbols.
	else if(ParseUniSymbol())
	{
		if(tok.kind == Token::Identifier)
			tok.data.id = mContext.AddUnqualifiedName(mQualifier.name);
	}

	//Parse ConIDs
	else if(IsUpperCase(*p))
	{
		ParseQualifier();
		tok.data.id = mContext.AddName(&mQualifier);
	}

	//Parse variables and reserved ids.
	else if(IsLowerCase(*p) || *p == '_')
	{
		ParseVariable();
		tok.data.id = mContext.AddUnqualifiedName(mQualifier.name);
	}

	//Unknown token - issue an error and skip it.
	else
	{
		mDiag.Error("Unknown token: '%@'", *p);
		p++;
		goto parseT;
	}

	mNewItem = false;
newItem:
	tok.length = (U32)(p - b);
}

}} //namespace athena::ast
