package se.rimmer.athena.parser

/**
 * Checks if the provided character is white, as specified in section 2.2 of the Haskell spec.
 */
fun isWhiteChar(c: Char): Boolean {
    // Spaces are handled separately.
    // All other white characters are in the same range.
    // Anything lower than TAB will underflow, giving some large number above 4.
    val ch = c.toInt()
    val index = ch - 9
    return (index <= 4 && index >= 0) || c == ' '
}

/**
 * Checks if the provided character is special, as specified in section 2.2 of the Haskell spec.
 */
fun isSpecial(c: Char): Boolean {
    val ch = c
    val index = ch - '('
    if(index > 85 || index < 0) return false;
    else return isSpecialTable[index];
}

// We use a small lookup table for this,
// since the number of branches would be ridiculous otherwise.
val isSpecialTable = arrayOf(
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
)

/**
 * Returns true if the provided character is valid as part of an identifier (VarID or ConID).
 */
fun isIdentifier(c: Char): Boolean {
    // Anything lower than ' will underflow, giving some large number above 83.
    val ch = c
    val index = ch - '\''

    if(index > 83 || index < 0) return false;
    else return isIdentifierTable[index]
}

val isIdentifierTable = arrayOf(
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
)

/**
 * Checks if the provided character is a symbol, as specified in section 2.2 of the Haskell spec.
 * TODO: Currently, only characters in the ASCII range are considered valid.
 */
fun isSymbol(c: Char): Boolean {
    val ch = c
    val index = ch - '!'
    if(index < 0 || index > 93) return false;
    else return isSymbolLookup[index]
}

// We use a small lookup table for symbols,
// since the number of branches would be ridiculous otherwise.
val isSymbolLookup = arrayOf(
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
)

/**
 * Parses the provided character as a hexit, to an integer in the range 0..15.
 * @return The parsed number. Returns Nothing if the character is not a valid number.
 */
fun parseHexit(c: Char): Int? {
    val ch = c
    val index = ch - '0'

    if(index < 0 || index > 54) {
        return null
    }

    val res = parseHexitLookup[index]
    if(res > 15) {
        return null
    }

    return res
}

// We use a small lookup table for com.youpic.codegen.parser.parseHexit,
// since the number of branches would be ridiculous otherwise.
val parseHexitLookup = arrayOf(
    0,  1,  2,  3,  4,  5,  6,  7,  8,  9,	/* 0..9 */
    255,255,255,255,255,255,255,			/* :..@ */
    10, 11, 12, 13, 14, 15,					/* A..F */
    255,255,255,255,255,255,255,			/* G..` */
    255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,
    255,255,255,255,255,255,
    10, 11, 12, 13, 14, 15					/* a..f */
)

/**
 * Parses the provided character as an octit, to an integer in the range 0..7.
 * @return The parsed number. Returns Nothing if the character is not a valid number.
 */
fun parseOctit(c: Char): Int? {
    val ch = c;
    val index = ch - '0';

    if(index < 0 || index > 7) {
        return null
    } else {
        return index
    }
}

/**
 * Returns true if this is a bit.
 */
fun isBit(c: Char): Boolean {
    val ch = c
    val index = ch - '0'
    return index <= 1 && index >= 0
}

/**
 * Returns true if this is a digit.
 */
fun isDigit(c: Char): Boolean {
    val ch = c
    val index = ch - '0'
    return index <= 9 && index >= 0
}

/**
 * Returns true if this is an octit.
 */
fun isOctit(c: Char): Boolean {
    val ch = c
    val index = ch - '0'
    return index <= 7 && index >= 0
}

/**
 * Returns true if this is a hexit.
 */
fun isHexit(c: Char): Boolean {
    // Anything lower than '0' will underflow, giving some large number above 54.
    val ch = c
    val index = ch - '0'

    if(index > 54 || index < 0) return false;
    else return isHexitLookup[index]
}

// We use a small lookup table for the hexit lookup,
// since the number of branches would be ridiculous otherwise.
val isHexitLookup = arrayOf(
    true, true, true, true, true, true, true, true, true, true,	/* 0..9 */
    false,false,false,false,false,false,false,					/* :..@ */
    true, true, true, true, true, true,							/* A..F */
    false,false,false,false,false,false,false,					/* G..` */
    false,false,false,false,false,false,false,
    false,false,false,false,false,false,false,
    false,false,false,false,false,false,
    true, true, true, true, true, true							/* a..f */
)
