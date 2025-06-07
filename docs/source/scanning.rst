Scanning and Compiling
======================

Scanning and Tokens
-------------------

The following can be included from ``scanner.h`` and can be used to create and work
with tokens.

.. c:enum:: TokenType

    Represents the type of a token. Scanning errors are passed on with
    :c:member:`TokenType.TOKEN_ERROR` tokens, which contain the message inside of
    the :c:member:`Token.start` field.

    .. c:enumerator::
        TOKEN_LEFT_PAREN
        TOKEN_RIGHT_PAREN
        TOKEN_LEFT_BRACE
        TOKEN_RIGHT_BRACE
        TOKEN_COMMA
        TOKEN_DOT
        TOKEN_MINUS
        TOKEN_PLUS
        TOKEN_SEMICOLON 
        TOKEN_SLASH
        TOKEN_STAR
        TOKEN_BANG
        TOKEN_BANG_EQUAL
        TOKEN_EQUAL
        TOKEN_EQUAL_EQUAL
        TOKEN_GREATER
        TOKEN_GREATER_EQUAL
        TOKEN_LESS
        TOKEN_LESS_EQUAL
        TOKEN_IDENTIFIER
        TOKEN_STRING
        TOKEN_NUMBER
        TOKEN_AND
        TOKEN_CLASS
        TOKEN_ELSE
        TOKEN_FALSE
        TOKEN_FOR
        TOKEN_FUN
        TOKEN_IF
        TOKEN_NIL
        TOKEN_OR
        TOKEN_PRINT
        TOKEN_RETURN
        TOKEN_SUPER
        TOKEN_THIS
        TOKEN_TRUE
        TOKEN_VAR
        TOKEN_WHILE
        TOKEN_ERROR
        TOKEN_EOF

.. c:autostruct:: Token
    :file: scanner.h
    :members:

.. c:autofunction:: initScanner

.. c:autofunction:: scanToken

Compiling and the Compiler
--------------------------

The following can be included from ``compiler.h`` and can help generate bytecode.

.. c:autodoc:: compiler.h