#ifndef clox_scanner_h
#define clox_scanner_h

/**
 * Represents the type of a token. Scanning errors are passed on with
 * :c:member:`TokenType.TOKEN_ERROR` tokens, which contain the message inside of
 * the :c:member:`Token.lexeme` field.
 */
typedef enum {
    // Single-character tokens.
    TOKEN_LEFT_PAREN, TOKEN_RIGHT_PAREN,
    TOKEN_LEFT_BRACE, TOKEN_RIGHT_BRACE,
    TOKEN_COMMA, TOKEN_DOT, TOKEN_MINUS, TOKEN_PLUS,
    TOKEN_SEMICOLON, TOKEN_SLASH, TOKEN_STAR,
    // One or two character tokens.
    TOKEN_BANG_EQUAL, TOKEN_EQUAL, TOKEN_EQUAL_EQUAL,
    TOKEN_GREATER, TOKEN_GREATER_EQUAL,
    TOKEN_LESS, TOKEN_LESS_EQUAL,
    // Literals.
    TOKEN_IDENTIFIER, TOKEN_STRING, TOKEN_NUMBER,
    // Keywords.
    TOKEN_AND, TOKEN_CLASS, TOKEN_ELSE, TOKEN_FALSE,
    TOKEN_FOR, TOKEN_FUNC, TOKEN_IF, TOKEN_NIL, TOKEN_NOT,
    TOKEN_OR, TOKEN_PRINT, TOKEN_RETURN, TOKEN_SUPER,
    TOKEN_THIS, TOKEN_TRUE, TOKEN_VAR, TOKEN_WHILE,

    TOKEN_ERROR, TOKEN_EOF
} TokenType;

/**
 * Represents the smallest unit of a source file, producing a flat stream. This
 * stream is fed to the compiler, which uses the tokens to produce bytecode.
 */
typedef struct {
    /** The type of the token, useful for quick identification. */
    TokenType type;
    /**
     * A pointer to somewhere in the original source code string, used
     * for deeper comparisons.
     */
    const char* start;
    /**
     * Since the lexeme is simply stored as a pointer to the original source
     * string, it also needs a length because it cannot be null terminated.
     */
    int length;
    /** The line that the token was scanned on, for error reporting purposes. */
    int line;
} Token;

/**
 * Initialize the scanner with the given source code, and get it ready to start
 * scanning tokens.
 * 
 * :param source: The source code to start scanning from
 */
void initScanner(const char* source);

/**
 * Scan a token and return it. Tokens are scanned as-needed to avoid a huge array
 * of them in memory.
 * 
 * :return: The next scanable token in the source, or a
 *  :c:member:`TokenType.TOKEN_EOF` token if the end of the source is reached
 */
Token scanToken();

#endif