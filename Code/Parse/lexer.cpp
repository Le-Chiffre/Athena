
#include <cmath>
#include "lexer.h"

namespace athena {
namespace ast {

/**
 * Compares source code to a string constant.
 * @param source The source code to compare. Must point to the first character of the string.
 * If the strings are equal, source is set to the first character after the part that is equal.
 * @param constant The constant string to compare to.
 */
bool compareConstString(const char*& source, const char* constant) {
	auto src = source;
	while(*constant == *source) {
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
Maybe<U32> parseHexit(WChar32 c) {
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
Maybe<U32> parseOctit(WChar32 c) {
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
Maybe<U32> parseDigit(WChar32 c) {
	U32 ch = c;
	U32 index = ch - '0';
	if(index > 9) return Nothing();
	else return Just(index);
}

/**
 * Parses the provided character as a bit, to an integer in the range 0..1.
 * @return The parsed number. Returns Nothing if the character is not a valid number.
 */
Maybe<U32> parseBit(WChar32 c) {
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
Maybe<U32> parseNumericAtom(WChar32);

template<>
Maybe<U32> parseNumericAtom<16>(WChar32 p) {return parseHexit(p);}

template<>
Maybe<U32> parseNumericAtom<10>(WChar32 p) {return parseDigit(p);}

template<>
Maybe<U32> parseNumericAtom<8>(WChar32 p) {return parseOctit(p);}

template<>
Maybe<U32> parseNumericAtom<2>(WChar32 p) {return parseBit(p);}

/**
 * Returns the name of the provided numeric base.
 * Supported bases are 2, 8, 10, 16.
 */
template<U32 Base>
const char* getBaseName();

template<>
const char* getBaseName<16>() {return "hexadecimal";}

template<>
const char* getBaseName<10>() {return "decimal";}

template<>
const char* getBaseName<8>() {return "octal";}

template<>
const char* getBaseName<2>() {return "binary";}

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
U32 parseIntSequence(const char*& p, U32 numChars, U32 max, Diagnostics* diag) {
    U32 res = 0;
	for(U32 i=0; i<numChars; i++) {
		char c = *p;
		if(auto num = parseNumericAtom<Base>(c)) {
			res *= Base;
			res += num.force();
			p++;
		} else {
			break;
		}
	}

	if(res > max) diag->warning("%@ escape sequence out of range", getBaseName<Base>());
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
U32 parseIntLiteral(const char*& p) {
	U32 res = 0;
	while(auto c = parseNumericAtom<Base>(*p)) {
		res *= Base;
		res += c.force();
		p++;
	}
	return res;
}


template<typename T> inline bool isNumeric(T x) {
    return (x >= '0') && (x <= '9');
}

template<typename T> inline int readDigit(T x) {
    return (int)(x-'0');
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
double parseFloatLiteral(const char*& p) {
    double out = (double)0.0;

    // Check sign.
    bool neg = false;
    if(*p == '+') {
        p++;
    } else if(*p == '-') {
        p++;
        neg = true;
    }

    // Create part before decimal point.
    while(isNumeric(*p)) {
        double n = (double)readDigit(*p);
        out *= 10.0;
        out += n;
        p++;
    }

    // Check if there is a fractional part.
    if(*p == '.') {
        p++;
        double dec = 0.0;
        U32 dpl = 0;

        while(isNumeric(*p)) {
            double n = (double)readDigit(*p);
            dec *= 10.0;
            dec += n;

            dpl++;
            p++;
        }

        // We need to use a floating point power here in order to support more than 9 decimals.
        double power = pow(10.0, dpl);
        dec /= power;
        out += dec;
    }

    // Check if there is an exponent.
    if(*p == 'E') {
        p++;

        // Check sign.
        bool signNegative = false;
        if(*p == '+') {
            p++;
        } else if(*p == '-') {
            p++;
            signNegative = true;
        }

        // Has exp. part;
        double exp = 0.0;

        while(isNumeric(*p)) {
            double n = (double)readDigit(*p);
            exp *= 10.0;
            exp += n;
            p++;
        }

        if(signNegative) exp = -exp;

        double power = pow(10.0, exp);
        out *= power;
    }

    if(neg) out = -out;
    return out;
}

/**
 * Returns true if this is an uppercase character.
 * TODO: Currently, only characters in the ASCII range are considered.
 */
bool isUpperCase(WChar32 c) {
	U32 ch = c;
	U32 index = ch - 'A';
	return index <= ('Z' - 'A');
}

/**
 * Returns true if this is a lowercase character.
 * TODO: Currently, only characters in the ASCII range are considered.
 */
bool isLowerCase(WChar32 c) {
	U32 ch = c;
	U32 index = ch - 'a';
	return index <= ('z' - 'a');
}

/**
 * Returns true if this is a lowercase or uppercase character.
 */
bool isAlpha(WChar32 c) {
	return isUpperCase(c) || isLowerCase(c);
}

/**
 * Returns true if this is a bit.
 */
bool isBit(WChar32 c) {
	U32 ch = c;
	U32 index = ch - '0';
	return index <= 1;
}

/**
 * Returns true if this is a digit.
 */
bool isDigit(WChar32 c) {
	U32 ch = c;
	U32 index = ch - '0';
	return index <= 9;
}

/**
 * Returns true if this is an octit.
 */
bool isOctit(WChar32 c) {
	U32 ch = c;
	U32 index = ch - '0';
	return index <= 7;
}

/**
 * Returns true if this is a hexit.
 */
bool isHexit(WChar32 c) {
	// We use a small lookup table for this,
	// since the number of branches would be ridiculous otherwise.
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

	// Anything lower than '0' will underflow, giving some large number above 54.
	U32 ch = c;
	U32 index = ch - '0';

	if(index > 54) return false;
	else return table[index];
}

/**
 * Returns true if the provided character is alpha-numeric.
 */
bool isAlphaNumeric(WChar32 c) {
	return isAlpha(c) || isDigit(c);
}

/**
 * Returns true if the provided character is valid as part of an identifier (VarID or ConID).
 */
bool isIdentifier(WChar32 c) {
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

	// Anything lower than ' will underflow, giving some large number above 83.
	U32 ch = c;
	U32 index = ch - '\'';

	if(index > 83) return false;
	else return table[index];
}

/**
 * Checks if the provided character is a symbol, as specified in section 2.2 of the Haskell spec.
 * TODO: Currently, only characters in the ASCII range are considered valid.
 */
bool isSymbol(WChar32 c) {
	// We use a small lookup table for this,
	// since the number of branches would be ridiculous otherwise.
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
bool isSpecial(WChar32 c) {
	// We use a small lookup table for this,
	// since the number of branches would be ridiculous otherwise.
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
bool isWhiteChar(WChar32 c) {
	// Spaces are handled separately.
	// All other white characters are in the same range.
	// Anything lower than TAB will underflow, giving some large number above 4.
	U32 ch = c;
	U32 index = ch - 9;
	return index <= 4 || c == ' ';
}

/**
 * Checks if the provided character is a graphic, as specified in section 2.2 of the Haskell spec.
 * TODO: Currently, only characters in the ASCII range are considered valid.
 */
bool isGraphic(WChar32 c) {
	U32 ch = c;
	U32 index = ch - '!';
	return index <= 93;
}

// UTF-8 --> UTF-32 conversion (single code point).
U32* decodeUtf8(const char*& string, U32* dest) {
    // Get the bytes from the first Char.
    U32 p;
    Byte* s = (Byte*)string;
    U32 c = s[0];
    string++;

    if(c < 0x80) {
        // 1-byte sequence, 00->7F
        *dest = (U32)c;
        return dest + 1;
    }

    if(c < 0xC2) {
        // Invalid sequence, 80->C1
        goto fail;
    }

    if(c < 0xE0) {
        // 2-byte sequence, C0->DF
        p = (c & 0x1F);
        goto _2bytes;
    }

    if(c < 0xF0) {
        // 3-byte sequence, E0->EF
        p = (c & 0xF);
        goto _3bytes;
    }

    if(c < 0xF8) {
        // 4-byte sequence, F0->F7
        p = (c & 0x7);

        // Valid Unicode cannot be higher than 0x10FFFF.
        if(p > 8) goto fail;
        else goto _4bytes;
    }

    fail:
    string = 0;
    return 0;

    _4bytes:
    {
        string++;
        s++;
        U32 fourth = *s;

        //Fourth Char must be 10xxxxxx
        if((fourth >> 6) != 2) goto fail;

        p <<= 6;
        p |= (fourth & 0x3F);
    }
    _3bytes:
    {
        string++;
        s++;
        U32 third = *s;

        // Third Char must be 10xxxxxx
        if((third >> 6) != 2) goto fail;

        p <<= 6;
        p |= (third & 0x3F);
    }
    _2bytes:
    string++;
    s++;
    U32 second = *s;

    // Second Char must be 10xxxxxx
    if((second >> 6) != 2) goto fail;

    p <<= 6;
    p |= (second & 0x3F);
    *dest = p;
    return dest + 1;
}

// UTF-32 --> UTF-8 conversion (single code point).
Byte* encodeUtf8(const U32*& string, Byte* buffer) {
    U32 codePoint = *string;
    string++;

    if(codePoint <= 0x7F) {
        // One character.
        buffer[0] = (Byte)codePoint;
        return buffer+1;
    } else if(codePoint <= 0x07FF) {
        // Two characters.
        buffer[0] = Byte(0xC0 | (codePoint >> 6));
        buffer[1] = Byte(0x80 | (codePoint & 0x3F));
        return buffer+2;
    } else if(codePoint <= 0xFFFF) {
        // Three characters.
        buffer[0] = Byte( 0xE0 | (codePoint >> 12));
        buffer[1] = Byte(0x80 | ((codePoint >> 6) & 0x3F));
        buffer[2] = Byte(0x80 | (codePoint & 0x3F));
        return buffer+3;
    } else if(codePoint <= 0x10FFFF) {
        // Four characters.
        buffer[0] = Byte(0xF0 | (codePoint >> 18));
        buffer[1] = Byte(0x80 | ((codePoint >> 12) & 0x3F));
        buffer[2] = Byte(0x80 | ((codePoint >> 6) & 0x3F));
        buffer[3] = Byte(0x80 | (codePoint & 0x3F));
        return buffer+4;
    } else {
        // Invalid code point.
        string = 0;
        return 0;
    }
}

//------------------------------------------------------------------------------

Lexer::Lexer(CompileContext& context, Diagnostics& diag, const char* text, Token* tok) :
	token(tok), text(text), p(text), l(text), context(context), diag(diag) {}

Token* Lexer::next() {
	parseToken();
	return token;
}

U32 Lexer::nextCodePoint() {
    U32 c;
	if(decodeUtf8(p, &c)) {
		return c;
	} else {
		diag.warning("Invalid UTF-8 sequence");
		return ' ';
	}
}

void Lexer::nextLine() {
	l = p + 1;
	line++;
	tabs = 0;
}

bool Lexer::whiteChar_UpdateLine() {
	if(*p == '\n') {
		nextLine();
		return true;
	}

	if(*p == '\t') {
		tabs++;
		return true;
	}

	return isWhiteChar(*p);
}

void Lexer::skipWhitespace() {
	auto& p = this->p;
	while(*p) {
		// Skip whitespace.
		if(!whiteChar_UpdateLine()) {
			// Check for single-line comments.
			if(*p == '-' && p[1] == '-' && !isSymbol(p[2])) {
				// Skip the current line.
				p += 2;
				while(*p && *p != '\n') p++;

				// If this is a newline, we update the location.
				// If it is the file end, the caller will take care of it.
				if(*p == '\n') {
					nextLine();
					p++;
				}
			}

			// Check for multi-line comments.
			else if(*p == '{' && p[1] == '-') {
				// The current nested comment depth.
				U32 level = 1;

				// Skip until the comment end.
				p += 2;
				while(*p) {
					// Update the source location if needed.
					if(*p == '\n') nextLine();

					// Check for nested comments.
					if(*p == '{' && p[1] == '-') level++;

					// Check for comment end.
					if(*p == '-' && p[1] == '}') {
						level--;
						if(level == 0) {
							p += 2;
							break;
						}
					}
				}

				// mP now points to the first character after the comment, or the file end.
				// Check if the comments were nested correctly.
				if(level)
					diag.warning("Incorrectly nested comment: missing %@ comment terminator(s).", level);
			}

			// No comment or whitespace - we are done.
			break;
		}

		// Check the next character.
		p++;
	}
}

std::string Lexer::parseStringLiteral() {
	std::string chars;

	p++;
	while(1) {
		if(*p == '\\') {
			// This is an escape sequence or gap.
			p++;
			if(whiteChar_UpdateLine()) {
				// This is a gap - we skip characters until the next '\'.
				// Update the current source line if needed.
				p++;
				while(whiteChar_UpdateLine()) p++;

				if(*p != '\\') {
					// The first character after a gap must be '\'.
					diag.warning("Missing gap end in string literal");
				}

				// Continue parsing the string.
				p++;
			} else {
                const auto ch = parseEscapedLiteral();
                auto pch = &ch;
                Byte buffer[5];
                auto length = encodeUtf8(pch, buffer) - buffer;
                chars.append((const char*)buffer, length);
			}
		} else if(*p == kFormatStart) {
			// Start a string format sequence.
			formatting = 1;
			p++;
			break;
		} else {
			if(*p == '\"') {
				// Terminate the string.
				p++;
				break;
			} else if(!*p || *p == '\n') {
				// If the line ends without terminating the string, we issue a warning.
				diag.warning("Missing terminating quote in string literal");
				break;
			} else {
				// Add characters to the string in the way they appear in source.
                chars.push_back(*p);
                p++;
			}
		}
	}

	// Create a new buffer for this string.
	return move(chars);
}

U32 Lexer::parseCharLiteral() {
	p++;
    U32 c;

	if(*p == '\\') {
		// This is an escape sequence.
		p++;
		c = parseEscapedLiteral();
	} else {
		// This is a char literal.
		c = nextCodePoint();
	}

	// Ignore any remaining characters in the literal.
	// It needs to end on this line.
	if(*p++ != '\'') {
		diag.warning("Multi-character character constant");
		while(*p != '\'') {
			if(*p == '\n' || *p == 0) {
				diag.warning("Missing terminating ' character in char literal");
				break;
			}
			p++;
		}
	}
	return c;
}

U32 Lexer::parseEscapedLiteral() {
    char c = *p++;
    switch(c) {
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
            if(!parseHexit(*p)) {
                diag.error("\\x used with no following hex digits");
                return ' ';
            }
            return parseIntSequence<16>(p, 8, 0xffffffff, &diag);
        case 'o':
            //Octal literal.
            if(!parseOctit(*p)) {
                diag.error("\\o used with no following octal digits");
                return ' ';
            }
            return parseIntSequence<8>(p, 16, 0xffffffff, &diag);
        default:
            if(isDigit(c)) {
                return parseIntSequence<10>(p, 10, 0xffffffff, &diag);
            } else {
                diag.warning("Unknown escape sequence '%@'", c);
                return ' ';
            }
    }
}

void Lexer::parseNumericLiteral() {
	auto& p = this->p;
	auto& tok = *token;
	tok.type = Token::Integer;
	tok.kind = Token::Literal;

	// Parse the type of this literal.
	// HcS mode also supports binary literals.
	if(p[1] == 'b' || p[1] == 'B') {
		if(isBit(p[2])) {
			// This is a binary literal.
			p += 2;
			tok.data.integer = parseIntLiteral<2>(p);
		} else goto parseInt;
	} else if(p[1] == 'o' || p[1] == 'O') {
		if(isOctit(p[2])) {
			// This is an octal literal.
			p += 2;
			tok.data.integer = parseIntLiteral<8>(p);
		} else goto parseInt;
	} else if(p[1] == 'x' || p[1] == 'X') {
		if(isHexit(p[2])) {
			// This is a hexadecimal literal.
			p += 2;
			tok.data.integer = parseIntLiteral<16>(p);
		} else goto parseInt;
	} else {
		// Check for a dot or exponent to determine if this is a float.
		auto d = p + 1;
		while(1) {
			if(*d == '.') {
				// The first char after the dot must be numeric, as well.
				if(isDigit(d[1])) break;
			} else if(*d == 'e' || *d == 'E') {
				// This is an exponent. If it is valid, the next char needs to be a numeric,
				// with an optional sign in-between.
				if(d[1] == '+' || d[1] == '-') d++;
				if(isDigit(d[1])) break;
			} else if(!isDigit(*d)) {
				// This wasn't a valid float.
				goto parseInt;
			}

			d++;
		}

		// Parse a float literal.
		tok.type = Token::Float;
		tok.data.floating = parseFloatLiteral(p);
	}

	return;

parseInt:

	// Parse a normal integer.
	tok.data.integer = parseIntLiteral<10>(p);
}

void Lexer::parseSymbol() {
	auto& tok = *token;
	auto& p = this->p;

	bool sym1 = isSymbol(p[1]);
	bool sym2 = sym1 && isSymbol(p[2]);

	// Instead of setting this in many different cases, we make it the default and override it later.
	tok.kind = Token::Keyword;

	if(!sym1) {
		// Check for various reserved operators of length 1.
		if(*p == ':') {
			// Single colon.
			tok.type = Token::opColon;
		} else if(*p == '.') {
			// Single dot.
			tok.type = Token::opDot;
		} else if(*p == '=') {
			// This is the reserved Equals operator.
			tok.type = Token::opEquals;
		} else if(*p == '\\') {
			// This is the reserved backslash operator.
			tok.type = Token::opBackSlash;
		} else if(*p == '|') {
			// This is the reserved bar operator.
			tok.type = Token::opBar;
		} else if(*p == '$') {
			// This is the reserved dollar operator.
			tok.type = Token::opDollar;
		} else if(*p == '@') {
			// This is the reserved at operator.
			tok.type = Token::opAt;
		} else if(*p == '~') {
			// This is the reserved tilde operator.
			tok.type = Token::opTilde;
		} else {
			// This is a variable operator.
			tok.kind = Token::Identifier;
		}
	} else if(!sym2) {
		// Check for various reserved operators of length 2.
		if(*p == ':' && p[1] == ':') {
			// This is the reserved ColonColon operator.
			tok.type = Token::opColonColon;
		} else if(*p == '=' && p[1] == '>') {
			// This is the reserved double-arrow operator.
			tok.type = Token::opArrowD;
		} else if(*p == '.' && p[1] == '.') {
			// This is the reserved DotDot operator.
			tok.type = Token::opDotDot;
		}  else if(*p == '<' && p[1] == '-') {
			// This is the reserved arrow-left operator.
			tok.type = Token::opArrowL;
		} else if(*p == '-' && p[1] == '>') {
			// This is the reserved arrow-right operator.
			tok.type = Token::opArrowR;
		} else {
			// This is a variable operator.
			tok.kind = Token::Identifier;
		}
	} else {
		// This is a variable operator.
		tok.kind = Token::Identifier;
	}


	if(tok.kind == Token::Identifier) {
		// Check if this is a constructor.
		if(*p == ':') {
			tok.type = Token::ConSym;
		} else {
			tok.type = Token::VarSym;
		}

		// Parse a symbol sequence.
		// Get the length of the sequence, we already know that the first one is a symbol.
		Size count = 1;
		auto start = p;
		while(isSymbol(*(++p))) count++;

		// Check for a single minus operator - used for parser optimization.
		if(count == 1 && *start == '-') {
            tok.singleMinus = true;
        } else {
            tok.singleMinus = false;
        }

		// Convert to UTF-32 and save in the current qualified name..
		qualifier.name = std::string{start, count};
	} else {
		// Skip to the next token.
		if(sym1) p += 2;
		else p++;
	}
}

void Lexer::parseSpecial() {
	auto& tok = *token;
	tok.kind = Token::Special;
	tok.type = (Token::Type)*p++;
}

void Lexer::parseQualifier() {
	auto& p = this->p;
	auto& tok = *token;

	auto start = p;
	Size length = 1;
	tok.kind = Token::Identifier;
	tok.type = Token::ConID;

	auto q = &qualifier.qualifier;

parseQ:
	while(isIdentifier(*(++p))) length++;

	auto str = std::string(start, length);
	if(*p == '.') {
		bool u = isUpperCase(p[1]);
		bool l = isLowerCase(p[1]) || p[1] == '_';
		bool s = isSymbol(p[1]);

		// If the next character is a valid identifier or symbol,
		// we add this qualifier to the list and parse the remaining characters.
		// Otherwise, we parse as a ConID.
		if(u || l || s) {
			*q = build<Qualified>();
			(*q)->name = str;
			q = &(*q)->qualifier;

			p++;
			start = p;
			length = 0;
		} else {
			goto makeCon;
		}

		// If the next character is upper case, we either have a ConID or another qualifier.
		if(u) {
			goto parseQ;
		}

		// If the next character is lowercase, we either have a VarID or keyword.
		else if(l) {
			parseVariable();

			// If this was a keyword, we parse as a constructor and dot operator instead.
			if(tok.kind == Token::Keyword) {
				p = start;
				goto makeCon;
			}
		}

		// If the next character is a symbol, we have a VarSym or ConSym.
		else if(s) {
			// We have a VarSym or ConSym.
			parseSymbol();
		}
	} else {
	makeCon:
		// We have a ConID.
		qualifier.name = str;
	}
};

void Lexer::parseVariable() {
	auto& p = this->p;
	auto& tok = *token;
	tok.type = Token::VarID;
	tok.kind = Token::Identifier;

	// First, check if we have a reserved keyword.
	auto c = p + 1;
	switch(*p) {
		case '_':
			tok.type = Token::kw_;
			break;
		case 'c':
			if(compareConstString(c, "ase")) tok.type = Token::kwCase;
			else if(compareConstString(c, "lass")) tok.type = Token::kwClass;
			break;
		case 'd':
			if(compareConstString(c, "ata")) tok.type = Token::kwData;
			else if(compareConstString(c, "efault")) tok.type = Token::kwDefault;
			else if(compareConstString(c, "eriving")) tok.type = Token::kwDeriving;
			else if(*c == 'o') {c++; tok.type = Token::kwDo;}
			break;
		case 'e':
			if(compareConstString(c, "lse")) tok.type = Token::kwElse;
			break;
		case 'f':
			if(compareConstString(c, "oreign")) tok.type = Token::kwForeign;
			else if(*c == 'o' && c[1] == 'r') {c += 2; tok.type = Token::kwFor;}
			break;
		case 'i':
			if(*c == 'f') {c++; tok.type = Token::kwIf;}
			else if(compareConstString(c, "mport")) tok.type = Token::kwImport;
			else if(*c == 'n' && !isIdentifier(c[1])) {c++; tok.type = Token::kwIn;}
			else if(compareConstString(c, "nfix")) {
				if(*c == 'l') {c++; tok.type = Token::kwInfixL;}
				else if(*c == 'r') {c++; tok.type = Token::kwInfixR;}
				else tok.type = Token::kwInfix;
			} else if(compareConstString(c, "nstance")) tok.type = Token::kwInstance;
			break;
		case 'l':
			if(*c == 'e' && c[1] == 't') {c += 2; tok.type = Token::kwLet;}
			break;
		case 'm':
			if(compareConstString(c, "odule")) tok.type = Token::kwModule;
			break;
		case 'n':
			if(compareConstString(c, "ewtype")) tok.type = Token::kwNewType;
			break;
		case 'o':
			if(*c == 'f') {c++; tok.type = Token::kwOf;}
			break;
		case 'p':
			if(compareConstString(c, "refix")) tok.type = Token::kwPrefix;
			break;
		case 't':
			if(compareConstString(c, "hen")) tok.type = Token::kwThen;
			else if(compareConstString(c, "ype")) tok.type = Token::kwType;
			break;
		case 'v':
			if(*c == 'a' && c[1] == 'r') {c += 2; tok.type = Token::kwVar;}
		case 'w':
			if(compareConstString(c, "here")) tok.type = Token::kwWhere;
			else if(compareConstString(c, "hile")) tok.type = Token::kwWhile;
			break;
	}

	// We have to read the longest possible lexeme.
	// If a reserved keyword was found, we check if a longer lexeme is possible.
	if(tok.type != Token::VarID) {
		if(isIdentifier(*c)) {
			tok.type = Token::VarID;
		} else {
			p = c;
			tok.kind = Token::Keyword;
			return;
		}
	}

	// Read the identifier name.
	U32 length = 1;
	auto start = p;
	while(isIdentifier(*(++p))) length++;

	qualifier.name = {start, length};
};

void Lexer::parseToken() {
	auto& tok = *token;
	auto& p = this->p;
	auto b = p;

parseT:
	// This needs to be reset manually.
	qualifier.qualifier = nullptr;

	// Check if we are inside a string literal.
	if(formatting == 3) {
		tok.sourceColumn = (U32)(p - l) + tabs * (kTabWidth - 1);
		tok.sourceLine = line;
		formatting = 0;
		goto stringLit;
	} else {
		// Skip any whitespace and comments.
		skipWhitespace();

		tok.sourceColumn = (U32)(p - l) + tabs * (kTabWidth - 1);
		tok.sourceLine = line;
	}

	// Check for the end of the file.
	if(!*p) {
		tok.kind = Token::Special;
		if(blockCount) tok.type = Token::EndOfBlock;
		else tok.type = Token::EndOfFile;
	}

	// Check if we need to insert a layout token.
	else if(tok.sourceColumn == ident && !newItem) {
		tok.type = Token::EndOfStmt;
		tok.kind = Token::Special;
		newItem = true;
		goto newItem;
	}

	// Check if we need to end a layout block.
	else if(tok.sourceColumn < ident) {
		tok.type = Token::EndOfBlock;
		tok.kind = Token::Special;
	}

	// Check for start of string formatting.
	else if(formatting == 1) {
		tok.kind = Token::Special;
		tok.type = Token::StartOfFormat;
		formatting = 2;
	}

    // Check for end of string formatting.
	else if(formatting == 2 && *p == kFormatEnd) {
		// Issue a format end and make sure the next token is parsed as a string literal.
		// Don't skip the character - ParseStringLiteral skips one at the beginning.
		tok.kind = Token::Special;
		tok.type = Token::EndOfFormat;
		formatting = 3;
	}

	//Check for integral literals.
	else if(isDigit(*p)) {
		parseNumericLiteral();
	}

	// Check for character literals.
	else if(*p == '\'') {
		tok.data.character = parseCharLiteral();
		tok.kind = Token::Literal;
		tok.type = Token::Char;
	}

	// Check for string literals.
	else if(*p == '\"') {
stringLit:
		// Since string literals can span multiple lines, this may update mLocation.line.
		tok.type = Token::String;
		tok.kind = Token::Literal;
		tok.data.id = context.addUnqualifiedName(parseStringLiteral());
	}

	//Check for special operators.
	else if(isSpecial(*p)) {
		parseSpecial();
	}

	//Parse symbols.
	else if(isSymbol(*p)) {
		parseSymbol();
		tok.data.id = context.addUnqualifiedName(qualifier.name);
	}

	//Parse ConIDs
	else if(isUpperCase(*p)) {
		parseQualifier();
		tok.data.id = context.addName(&qualifier);
	}

	//Parse variables and reserved ids.
	else if(isLowerCase(*p) || *p == '_') {
		parseVariable();
		tok.data.id = context.addUnqualifiedName(qualifier.name);
	}

	//Unknown token - issue an error and skip it.
	else {
		diag.error("Unknown token: '%@'", *p);
		p++;
		goto parseT;
	}

	newItem = false;
newItem:
	tok.length = (U32)(p - b);
}

}} //namespace athena::ast
