/*
Portions copyright 2025 Eric Schuette

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.

See LICENSE (https://github.com/late-parrot/craven/blob/main/LICENSE)
for more information.
*/

#ifndef craven_scanner_h
#define craven_scanner_h

/**
 * Represents the type of a token. Scanning errors are passed on with
 * :c:member:`TokenType.TOKEN_ERROR` tokens, which contain the message inside of
 * the :c:member:`Token.lexeme` field.
 */
typedef enum {
    // Single-character tokens.
    TOKEN_LEFT_PAREN, TOKEN_RIGHT_PAREN,
    TOKEN_LEFT_BRACE, TOKEN_RIGHT_BRACE,
    TOKEN_LEFT_SQUARE, TOKEN_RIGHT_SQUARE,
    TOKEN_COMMA, TOKEN_DOT, TOKEN_MINUS, TOKEN_PLUS,
    TOKEN_SEMICOLON, TOKEN_SLASH, TOKEN_STAR,
    // One or two character tokens.
    TOKEN_BANG_EQUAL, TOKEN_EQUAL, TOKEN_EQUAL_EQUAL,
    TOKEN_GREATER, TOKEN_GREATER_EQUAL,
    TOKEN_LESS, TOKEN_LESS_EQUAL, TOKEN_FAT_ARROW,
    // Literals.
    TOKEN_IDENTIFIER, TOKEN_STRING, TOKEN_NUMBER,
    // Keywords.
    TOKEN_AND, TOKEN_CLASS, TOKEN_ELSE, TOKEN_FALSE,
    TOKEN_FOR, TOKEN_FUNC, TOKEN_IF, TOKEN_IN, TOKEN_NONE,
    TOKEN_NOT, TOKEN_OR, TOKEN_PRINT, TOKEN_RETURN,
    TOKEN_SOME, TOKEN_SUPER, TOKEN_THIS, TOKEN_TRUE,
    TOKEN_VAR, TOKEN_WHILE,

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