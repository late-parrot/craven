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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "compiler.h"
#include "memory.h"
#include "scanner.h"

#ifdef DEBUG_PRINT_CODE
#include "debug.h"
#endif

#define EMIT_BYTE(byte) emitByte(vm, byte)
#define EMIT_BYTES(byte1, byte2) emitBytes(vm, byte1, byte2)
#define EMIT_JUMP(instruction) emitJump(vm, instruction)
#define EMIT_LOOP(loopStart) emitLoop(vm, loopStart)
#define EMIT_RETURN() emitReturn(vm)
#define EMIT_CONSTANT(value) emitConstant(vm, value)

typedef struct {
    Token current;
    Token previous;
    bool hadError;
    bool panicMode;
} Parser;

typedef enum {
    PREC_NONE,
    PREC_ASSIGNMENT,  // =
    PREC_OR,          // or
    PREC_AND,         // and
    PREC_EQUALITY,    // == !=
    PREC_COMPARISON,  // < > <= >=
    PREC_TERM,        // + -
    PREC_FACTOR,      // * /
    PREC_UNARY,       // ! -
    PREC_CALL,        // . ()
    PREC_PRIMARY
} Precedence;

typedef void (*ParseFn)(VM* vm, bool canAssign);

typedef struct {
    ParseFn prefix;
    ParseFn infix;
    Precedence precedence;
} ParseRule;

typedef struct {
    Token name;
    int depth;
    bool isCaptured;
} Local;

typedef struct {
    uint8_t index;
    bool isLocal;
} Upvalue;

typedef enum {
    TYPE_FUNCTION,
    TYPE_INITIALIZER,
    TYPE_METHOD,
    TYPE_SCRIPT
} FunctionType;

typedef struct Compiler {
    struct Compiler* enclosing;
    ObjFunction* function;
    FunctionType type;

    Local locals[UINT8_COUNT];
    int localCount;
    Upvalue upvalues[UINT8_COUNT];
    int scopeDepth;
} Compiler;

typedef struct ClassCompiler {
    struct ClassCompiler* enclosing;
    bool hasSuperclass;
} ClassCompiler;

Parser parser;
Compiler* current = NULL;
ClassCompiler* currentClass = NULL;

static Chunk* currentChunk() {
    return &current->function->chunk;
}

static void errorAt(Token* token, const char* message) {
    if (parser.panicMode) return;
    parser.panicMode = true;
    fprintf(stderr, "[line %d] Error", token->line);

    if (token->type == TOKEN_EOF) {
        fprintf(stderr, " at end");
    } else if (token->type == TOKEN_ERROR) {
        // Nothing.
    } else {
        fprintf(stderr, " at '%.*s'", token->length, token->start);
    }

    fprintf(stderr, ": %s\n", message);
    parser.hadError = true;
}

static void error(const char* message) {
    errorAt(&parser.previous, message);
}

static void errorAtCurrent(const char* message) {
    errorAt(&parser.current, message);
}

static void advance() {
    parser.previous = parser.current;
    for (;;) {
        parser.current = scanToken();
        if (parser.current.type != TOKEN_ERROR) break;
        errorAtCurrent(parser.current.start);
    }
}

static void consume(TokenType type, const char* message) {
    if (parser.current.type == type) {
        advance();
        return;
    }

    errorAtCurrent(message);
}

static bool check(TokenType type) {
    return parser.current.type == type;
}

static bool match(TokenType type) {
    if (!check(type)) return false;
    advance();
    return true;
}

static bool softKeyword(Token* token, const char* keyword) {
    return memcmp(token->start, keyword, token->length) == 0;
}

static void emitByte(VM* vm, uint8_t byte) {
    writeChunk(vm, currentChunk(), byte, parser.previous.line);
}

static void emitBytes(VM* vm, uint8_t byte1, uint8_t byte2) {
    EMIT_BYTE(byte1);
    EMIT_BYTE(byte2);
}

static void emitLoop(VM* vm, int loopStart) {
    EMIT_BYTE(OP_LOOP);

    int offset = currentChunk()->count - loopStart + 2;
    if (offset > UINT16_MAX) error("Loop body too large.");

    EMIT_BYTE((offset >> 8) & 0xff);
    EMIT_BYTE(offset & 0xff);
}

static int emitJump(VM* vm, uint8_t instruction) {
    EMIT_BYTE(instruction);
    EMIT_BYTE(0xff);
    EMIT_BYTE(0xff);
    return currentChunk()->count - 2;
}

static void emitReturn(VM* vm) {
    if (current->type == TYPE_INITIALIZER) {
        EMIT_BYTES(OP_GET_LOCAL, 0);
    }
    EMIT_BYTE(OP_RETURN);
}

static uint8_t makeConstant(VM* vm, Value value) {
    int constant = addConstant(vm, currentChunk(), value);
    if (constant > UINT8_MAX) {
        error("Too many constants in one chunk.");
        return 0;
    }

    return (uint8_t)constant;
}

static void emitConstant(VM* vm, Value value) {
    EMIT_BYTES(OP_CONSTANT, makeConstant(vm, value));
}

static void patchJump(int offset) {
    // -2 to adjust for the bytecode for the jump offset itself.
    int jump = currentChunk()->count - offset - 2;

    if (jump > UINT16_MAX) {
        error("Too much code to jump over.");
    }

    currentChunk()->code[offset] = (jump >> 8) & 0xff;
    currentChunk()->code[offset + 1] = jump & 0xff;
}

static void initCompiler(VM* vm, Compiler* compiler, FunctionType type) {
    compiler->enclosing = current;
    compiler->function = NULL;
    compiler->type = type;
    compiler->localCount = 0;
    compiler->scopeDepth = 0;
    compiler->function = newFunction(vm);
    current = compiler;
    if (type != TYPE_SCRIPT) {
        if (parser.previous.type == TOKEN_FUNC) {
            current->function->name = copyString(vm, "anonymous", 9);
        } else {
            current->function->name = copyString(vm,
                parser.previous.start,
                parser.previous.length
            );
        }
    }

    Local* local = &current->locals[current->localCount++];
    local->depth = 0;
    local->isCaptured = false;
    if (type != TYPE_FUNCTION) {
        local->name.start = "this";
        local->name.length = 4;
    } else {
        local->name.start = "";
        local->name.length = 0;
    }
}

static ObjFunction* endCompiler(VM* vm) {
    EMIT_RETURN();
    ObjFunction* function = current->function;

#ifdef DEBUG_PRINT_CODE
    if (!parser.hadError) {
        disassembleChunk(currentChunk(),
            function->name != NULL ? function->name->chars : "<script>"
        );
    }
#endif

    current = current->enclosing;
    return function;
}

static void beginScope() {
    current->scopeDepth++;
}

static void endScope(VM* vm) {
    current->scopeDepth--;

    while (current->localCount > 0 &&
        current->locals[current->localCount - 1].depth > current->scopeDepth) {
        if (current->locals[current->localCount - 1].isCaptured) {
            EMIT_BYTE(OP_CLOSE_UPVALUE);
        } else {
            EMIT_BYTE(OP_POP);
        }
        current->localCount--;
    }
}

static void block(VM* vm);
static void blockExpr(VM* vm, bool canAssign);
static void classDeclaration(VM* vm, bool canAssign);
static void expression(VM* vm);
static void forStatement(VM* vm, bool canAssign);
static void funcDeclaration(VM* vm, bool canAssign);
static void ifStatement(VM* vm, bool canAssign);
static void printStatement(VM* vm, bool canAssign);
static void returnStatement(VM* vm, bool canAssign);
static void varDeclaration(VM* vm, bool canAssign);
static void whileStatement(VM* vm, bool canAssign);
static ParseRule* getRule(TokenType type);
static void parsePrecedence(VM* vm, Precedence precedence);

static uint8_t identifierConstant(VM* vm, Token* name) {
    return makeConstant(vm, OBJ_VAL(copyString(vm, name->start, name->length)));
}

static bool identifiersEqual(Token* a, Token* b) {
    if (a->length != b->length) return false;
    return memcmp(a->start, b->start, a->length) == 0;
}

static int resolveLocal(Compiler* compiler, Token* name) {
    for (int i = compiler->localCount - 1; i >= 0; i--) {
        Local* local = &compiler->locals[i];
        if (identifiersEqual(name, &local->name)) {
            if (local->depth == -1) {
                error("Can't read local variable in its own initializer.");
            }
            return i;
        }
    }
    return -1;
}

static int addUpvalue(Compiler* compiler, uint8_t index, bool isLocal) {
    int upvalueCount = compiler->function->upvalueCount;

    for (int i = 0; i < upvalueCount; i++) {
        Upvalue* upvalue = &compiler->upvalues[i];
        if (upvalue->index == index && upvalue->isLocal == isLocal) {
            return i;
        }
    }

    if (upvalueCount == UINT8_COUNT) {
        error("Too many closure variables in function.");
        return 0;
    }

    compiler->upvalues[upvalueCount].isLocal = isLocal;
    compiler->upvalues[upvalueCount].index = index;
    return compiler->function->upvalueCount++;
}

static int resolveUpvalue(Compiler* compiler, Token* name) {
    if (compiler->enclosing == NULL) return -1;

    int local = resolveLocal(compiler->enclosing, name);
    if (local != -1) {
        compiler->enclosing->locals[local].isCaptured = true;
        return addUpvalue(compiler, (uint8_t)local, true);
    }

    int upvalue = resolveUpvalue(compiler->enclosing, name);
    if (upvalue != -1) {
        return addUpvalue(compiler, (uint8_t)upvalue, false);
    }

    return -1;
}

static void addLocal(Token name) {
    if (current->localCount == UINT8_COUNT) {
        error("Too many local variables in function.");
        return;
    }
    Local* local = &current->locals[current->localCount++];
    local->name = name;
    local->depth = -1;
    local->isCaptured = false;
}

static void declareVariable() {
    if (current->scopeDepth == 0) return;

    Token* name = &parser.previous;
    for (int i = current->localCount - 1; i >= 0; i--) {
        Local* local = &current->locals[i];
        if (local->depth != -1 && local->depth < current->scopeDepth) {
            break; 
        }

        if (identifiersEqual(name, &local->name)) {
            error("Already a variable with this name in this scope.");
        }
    }
    addLocal(*name);
}

static uint8_t parseVariable(VM* vm, const char* errorMessage) {
    consume(TOKEN_IDENTIFIER, errorMessage);

    declareVariable();
    if (current->scopeDepth > 0) return 0;

    return identifierConstant(vm, &parser.previous);
}

static void markInitialized() {
    if (current->scopeDepth == 0) return;
    current->locals[current->localCount - 1].depth = current->scopeDepth;
}

static void defineVariable(VM* vm, uint8_t global) {
    if (current->scopeDepth > 0) {
        markInitialized();
        return;
    }
    EMIT_BYTES(OP_DEFINE_GLOBAL, global);
}

static uint8_t argumentList(VM* vm) {
    uint8_t argCount = 0;
    if (!check(TOKEN_RIGHT_PAREN)) {
        do {
            expression(vm);
            if (argCount == 255) {
                error("Can't have more than 255 arguments.");
            }
            argCount++;
        } while (match(TOKEN_COMMA));
    }
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after arguments.");
    return argCount;
}

static void and_(VM* vm, bool canAssign) {
    int endJump = EMIT_JUMP(OP_JUMP_IF_FALSE);
    EMIT_BYTE(OP_POP);
    parsePrecedence(vm, PREC_AND);
    patchJump(endJump);
}

static void binary(VM* vm, bool canAssign) {
    TokenType operatorType = parser.previous.type;
    ParseRule* rule = getRule(operatorType);
    // One level higher means we have left-associativity;
    // right-associative would be the same level.
    parsePrecedence(vm, (Precedence)(rule->precedence + 1));

    switch (operatorType) {
        case TOKEN_BANG_EQUAL:    EMIT_BYTES(OP_EQUAL, OP_NOT); break;
        case TOKEN_EQUAL_EQUAL:   EMIT_BYTE(OP_EQUAL); break;
        case TOKEN_GREATER:       EMIT_BYTE(OP_GREATER); break;
        case TOKEN_GREATER_EQUAL: EMIT_BYTES(OP_LESS, OP_NOT); break;
        case TOKEN_LESS:          EMIT_BYTE(OP_LESS); break;
        case TOKEN_LESS_EQUAL:    EMIT_BYTES(OP_GREATER, OP_NOT); break;
        case TOKEN_PLUS:          EMIT_BYTE(OP_ADD); break;
        case TOKEN_MINUS:         EMIT_BYTE(OP_SUBTRACT); break;
        case TOKEN_STAR:          EMIT_BYTE(OP_MULTIPLY); break;
        case TOKEN_SLASH:         EMIT_BYTE(OP_DIVIDE); break;
        default: return; // Unreachable.
    }
}

static void call(VM* vm, bool canAssign) {
    uint8_t argCount = argumentList(vm);
    EMIT_BYTES(OP_CALL, argCount);
}

static void dict(VM* vm) {
    int entryCount = 0;
    consume(TOKEN_LEFT_BRACE, "Expect '{' after 'dict'.");
    if (!check(TOKEN_RIGHT_BRACE)) {
        do {
            if (entryCount == 255) {
                error("Can't have more than 255 elements.");
            }
            entryCount++;
            expression(vm);
            consume(TOKEN_FAT_ARROW, "Expect '=>' after dict key.");
            expression(vm);
        } while (match(TOKEN_COMMA));
    }
    consume(TOKEN_RIGHT_BRACE, "Expect '}' after dict elements.");
    EMIT_BYTES(OP_DICT, entryCount);
}

static void dot(VM* vm, bool canAssign) {
    consume(TOKEN_IDENTIFIER, "Expect property name after '.'.");
    uint8_t name = identifierConstant(vm, &parser.previous);
    if (canAssign && match(TOKEN_EQUAL)) {
        expression(vm);
        EMIT_BYTES(OP_SET_PROPERTY, name);
    } else if (match(TOKEN_LEFT_PAREN)) {
        uint8_t argCount = argumentList(vm);
        EMIT_BYTES(OP_INVOKE, name);
        EMIT_BYTE(argCount);
    } else {
        EMIT_BYTES(OP_GET_PROPERTY, name);
    }
}

static void grouping(VM* vm, bool canAssign) {
    expression(vm);
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}

static void index_(VM* vm, bool canAssign) {
    expression(vm);
    consume(TOKEN_RIGHT_SQUARE, "Expect ']' after index.");
    if (canAssign && match(TOKEN_EQUAL)) {
        expression(vm);
        EMIT_BYTE(OP_SET_INDEX);
    } else {
        EMIT_BYTE(OP_GET_INDEX);
    }
}

static void list(VM* vm, bool canAssign) {
    uint8_t elemCount = 0;
    if (!check(TOKEN_RIGHT_SQUARE)) {
        do {
            expression(vm);
            if (elemCount == 255) {
                error("Can't have more than 255 elements.");
            }
            elemCount++;
        } while (match(TOKEN_COMMA));
    }
    consume(TOKEN_RIGHT_SQUARE, "Expect ']' after list elements.");
    EMIT_BYTES(OP_LIST, elemCount);
}

static void literal(VM* vm, bool canAssign) {
    switch (parser.previous.type) {
        case TOKEN_FALSE: EMIT_BYTE(OP_FALSE); break;
        case TOKEN_NIL: EMIT_BYTE(OP_NIL); break;
        case TOKEN_TRUE: EMIT_BYTE(OP_TRUE); break;
        default: return; // Unreachable.
    }
}

static void number(VM* vm, bool canAssign) {
    double value = strtod(parser.previous.start, NULL);
    EMIT_CONSTANT(NUMBER_VAL(value));
}

static void or_(VM* vm, bool canAssign) {
    int elseJump = EMIT_JUMP(OP_JUMP_IF_FALSE);
    int endJump = EMIT_JUMP(OP_JUMP);
    patchJump(elseJump);
    EMIT_BYTE(OP_POP);
    parsePrecedence(vm, PREC_OR);
    patchJump(endJump);
}

static void string(VM* vm, bool canAssign) {
    // TODO: Add support for escape sequences here
    EMIT_CONSTANT(OBJ_VAL(copyString(vm,
        parser.previous.start + 1,
        parser.previous.length - 2
    )));
}

static void namedVariable(VM* vm, Token name, bool canAssign) {
    uint8_t getOp, setOp;
    int arg = resolveLocal(current, &name);
    if (arg != -1) {
        getOp = OP_GET_LOCAL;
        setOp = OP_SET_LOCAL;
    } else if ((arg = resolveUpvalue(current, &name)) != -1) {
        getOp = OP_GET_UPVALUE;
        setOp = OP_SET_UPVALUE;
    } else {
        arg = identifierConstant(vm, &name);
        getOp = OP_GET_GLOBAL;
        setOp = OP_SET_GLOBAL;
    }

    if (canAssign && match(TOKEN_EQUAL)) {
        expression(vm);
        EMIT_BYTES(setOp, (uint8_t)arg);
    } else {
        EMIT_BYTES(getOp, (uint8_t)arg);
    }
}

static void variable(VM* vm, bool canAssign) {
    if (softKeyword(&parser.previous, "dict") && check(TOKEN_LEFT_BRACE)) {
        dict(vm);
    } else {
        namedVariable(vm, parser.previous, canAssign);
    }
}

static Token syntheticToken(const char* text) {
    Token token;
    token.start = text;
    token.length = (int)strlen(text);
    return token;
}

static void super_(VM* vm, bool canAssign) {
    if (currentClass == NULL) {
        error("Can't use 'super' outside of a class.");
    } else if (!currentClass->hasSuperclass) {
        error("Can't use 'super' in a class with no superclass.");
    }

    consume(TOKEN_DOT, "Expect '.' after 'super'.");
    consume(TOKEN_IDENTIFIER, "Expect superclass method name.");
    uint8_t name = identifierConstant(vm, &parser.previous);

    namedVariable(vm, syntheticToken("this"), false);
    if (match(TOKEN_LEFT_PAREN)) {
        uint8_t argCount = argumentList(vm);
        namedVariable(vm, syntheticToken("super"), false);
        EMIT_BYTES(OP_SUPER_INVOKE, name);
        EMIT_BYTE(argCount);
    } else {
        namedVariable(vm, syntheticToken("super"), false);
        EMIT_BYTES(OP_GET_SUPER, name);
    }
}

static void this_(VM* vm, bool canAssign) {
    if (currentClass == NULL) {
        error("Can't use 'this' outside of a class.");
        return;
    }
    variable(vm, false);
}

static void unary(VM* vm, bool canAssign) {
    TokenType operatorType = parser.previous.type;
    parsePrecedence(vm, PREC_UNARY);
    switch (operatorType) {
        case TOKEN_NOT: EMIT_BYTE(OP_NOT); break;
        case TOKEN_MINUS: EMIT_BYTE(OP_NEGATE); break;
        default: return; // Unreachable.
    }
}

ParseRule rules[] = {
    [TOKEN_LEFT_PAREN]    = {grouping,         call,   PREC_CALL},
    [TOKEN_RIGHT_PAREN]   = {NULL,             NULL,   PREC_NONE},
    [TOKEN_LEFT_BRACE]    = {blockExpr,        NULL,   PREC_NONE}, 
    [TOKEN_RIGHT_BRACE]   = {NULL,             NULL,   PREC_NONE},
    [TOKEN_LEFT_SQUARE]   = {list,             index_, PREC_CALL},
    [TOKEN_RIGHT_SQUARE]  = {NULL,             NULL,   PREC_NONE},
    [TOKEN_COMMA]         = {NULL,             NULL,   PREC_NONE},
    [TOKEN_DOT]           = {NULL,             dot,    PREC_CALL},
    [TOKEN_MINUS]         = {unary,            binary, PREC_TERM},
    [TOKEN_PLUS]          = {NULL,             binary, PREC_TERM},
    [TOKEN_SEMICOLON]     = {NULL,             NULL,   PREC_NONE},
    [TOKEN_SLASH]         = {NULL,             binary, PREC_FACTOR},
    [TOKEN_STAR]          = {NULL,             binary, PREC_FACTOR},
    [TOKEN_NOT]           = {unary,            NULL,   PREC_NONE},
    [TOKEN_BANG_EQUAL]    = {NULL,             binary, PREC_EQUALITY},
    [TOKEN_EQUAL]         = {NULL,             NULL,   PREC_NONE},
    [TOKEN_EQUAL_EQUAL]   = {NULL,             binary, PREC_EQUALITY},
    [TOKEN_GREATER]       = {NULL,             binary, PREC_COMPARISON},
    [TOKEN_GREATER_EQUAL] = {NULL,             binary, PREC_COMPARISON},
    [TOKEN_LESS]          = {NULL,             binary, PREC_COMPARISON},
    [TOKEN_LESS_EQUAL]    = {NULL,             binary, PREC_COMPARISON},
    [TOKEN_IDENTIFIER]    = {variable,         NULL,   PREC_NONE},
    [TOKEN_STRING]        = {string,           NULL,   PREC_NONE},
    [TOKEN_NUMBER]        = {number,           NULL,   PREC_NONE},
    [TOKEN_AND]           = {NULL,             and_,   PREC_AND},
    [TOKEN_CLASS]         = {classDeclaration, NULL,   PREC_NONE},
    [TOKEN_ELSE]          = {NULL,             NULL,   PREC_NONE},
    [TOKEN_FALSE]         = {literal,          NULL,   PREC_NONE},
    [TOKEN_FOR]           = {forStatement,     NULL,   PREC_NONE},
    [TOKEN_FUNC]          = {funcDeclaration,  NULL,   PREC_NONE},
    [TOKEN_IF]            = {ifStatement,      NULL,   PREC_NONE},
    [TOKEN_NIL]           = {literal,          NULL,   PREC_NONE},
    [TOKEN_OR]            = {NULL,             or_,    PREC_OR},
    [TOKEN_PRINT]         = {printStatement,   NULL,   PREC_NONE},
    [TOKEN_RETURN]        = {returnStatement,  NULL,   PREC_NONE},
    [TOKEN_SUPER]         = {super_,           NULL,   PREC_NONE},
    [TOKEN_THIS]          = {this_,            NULL,   PREC_NONE},
    [TOKEN_TRUE]          = {literal,          NULL,   PREC_NONE},
    [TOKEN_VAR]           = {varDeclaration,   NULL,   PREC_NONE},
    [TOKEN_WHILE]         = {whileStatement,   NULL,   PREC_NONE},
    [TOKEN_ERROR]         = {NULL,             NULL,   PREC_NONE},
    [TOKEN_EOF]           = {NULL,             NULL,   PREC_NONE},
};

static void parsePrecedence(VM* vm, Precedence precedence) {
    advance();
    ParseFn prefixRule = getRule(parser.previous.type)->prefix;
    if (prefixRule == NULL) {
        error("Expect expression.");
        return;
    }

    bool canAssign = precedence <= PREC_ASSIGNMENT;
    prefixRule(vm, canAssign);

    while (precedence <= getRule(parser.current.type)->precedence) {
        advance();
        ParseFn infixRule = getRule(parser.previous.type)->infix;
        infixRule(vm, canAssign);
    }

    if (canAssign && match(TOKEN_EQUAL)) {
        error("Invalid assignment target.");
    }
}

static ParseRule* getRule(TokenType type) {
    return &rules[type];
}

static void function(VM* vm, FunctionType type) {
    Compiler compiler;
    initCompiler(vm, &compiler, type);
    beginScope();

    consume(TOKEN_LEFT_PAREN, "Expect '(' after function name.");
    if (!check(TOKEN_RIGHT_PAREN)) {
        do {
            current->function->arity++;
            if (current->function->arity > 255) {
                errorAtCurrent("Can't have more than 255 parameters.");
            }
            uint8_t constant = parseVariable(vm, "Expect parameter name.");
            defineVariable(vm, constant);
        } while (match(TOKEN_COMMA));
    }
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after parameters.");
    consume(TOKEN_LEFT_BRACE, "Expect '{' before function body.");
    block(vm);

    ObjFunction* function = endCompiler(vm);
    EMIT_BYTES(OP_CLOSURE, makeConstant(vm, OBJ_VAL(function)));

    for (int i = 0; i < function->upvalueCount; i++) {
        EMIT_BYTE(compiler.upvalues[i].isLocal ? 1 : 0);
        EMIT_BYTE(compiler.upvalues[i].index);
    }
}

static void method(VM* vm) {
    consume(TOKEN_IDENTIFIER, "Expect method name.");
    uint8_t constant = identifierConstant(vm, &parser.previous);

    FunctionType type = TYPE_METHOD;
    if (parser.previous.length == 4 &&
        memcmp(parser.previous.start, "init", 4) == 0) {
        type = TYPE_INITIALIZER;
    }
    function(vm, type);
    EMIT_BYTES(OP_METHOD, constant);
}

static void classDeclaration(VM* vm, bool canAssign) {
    consume(TOKEN_IDENTIFIER, "Expect class name.");
    Token className = parser.previous;
    uint8_t nameConstant = identifierConstant(vm, &parser.previous);
    declareVariable();

    EMIT_BYTES(OP_CLASS, nameConstant);
    defineVariable(vm, nameConstant);

    ClassCompiler classCompiler;
    classCompiler.hasSuperclass = false;
    classCompiler.enclosing = currentClass;
    currentClass = &classCompiler;

    if (match(TOKEN_LESS)) {
        consume(TOKEN_IDENTIFIER, "Expect superclass name.");
        variable(vm, false);

        if (identifiersEqual(&className, &parser.previous)) {
            error("A class can't inherit from itself.");
        }

        beginScope();
        addLocal(syntheticToken("super"));
        defineVariable(vm, 0);

        namedVariable(vm, className, false);
        EMIT_BYTE(OP_INHERIT);
        classCompiler.hasSuperclass = true;
    }

    namedVariable(vm, className, false);
    consume(TOKEN_LEFT_BRACE, "Expect '{' before class body.");
    while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) {
        method(vm);
    }
    consume(TOKEN_RIGHT_BRACE, "Expect '}' after class body.");

    if (classCompiler.hasSuperclass) {
        endScope(vm);
    }

    currentClass = currentClass->enclosing;
}

static void funcDeclaration(VM* vm, bool canAssign) {
    if (check(TOKEN_IDENTIFIER)) {
        uint8_t global = parseVariable(vm, "Expect function name.");
        Token funcName = parser.previous;
        markInitialized();
        function(vm, TYPE_FUNCTION);
        defineVariable(vm, global);
        namedVariable(vm, funcName, false);
    } else { // Anonymous function
        function(vm, TYPE_FUNCTION);
    }
}

static void varDeclaration(VM* vm, bool canAssign) {
    uint8_t global = parseVariable(vm, "Expect variable name.");
    Token varName = parser.previous;
    if (match(TOKEN_EQUAL)) {
        expression(vm);
    } else {
        EMIT_BYTE(OP_NIL);
    }
    consume(TOKEN_SEMICOLON, "Expect ';' after variable declaration.");
    defineVariable(vm, global);
    namedVariable(vm, varName, false);
}

static void expressionStatement(VM* vm) {
    expression(vm);
    consume(TOKEN_SEMICOLON, "Expect ';' after expression.");
}

static void forStatement(VM* vm, bool canAssign) {
    beginScope();
    uint8_t global = parseVariable(vm, "Expect variable name after 'for'.");
    Token varName = parser.previous;
    EMIT_BYTE(OP_NIL);
    defineVariable(vm, global);
    consume(TOKEN_IN, "Expect 'in' after variable name.");

    expression(vm);
    EMIT_BYTES(OP_INT, 0);

    int loopStart = currentChunk()->count;
    int exitJump = EMIT_JUMP(OP_NEXT_JUMP);
    
    uint8_t op;
    int arg = resolveLocal(current, &varName);
    if (arg != -1) {
        op = OP_SET_LOCAL;
    } else if ((arg = resolveUpvalue(current, &varName)) != -1) {
        op = OP_SET_UPVALUE;
    } else {
        arg = identifierConstant(vm, &varName);
        op = OP_SET_GLOBAL;
    }
    EMIT_BYTES(op, (uint8_t)arg);
    EMIT_BYTE(OP_POP);

    consume(TOKEN_LEFT_BRACE, "Expected '{' for 'for' body");
    block(vm);
    EMIT_BYTE(OP_POP);

    EMIT_LOOP(loopStart);
    patchJump(exitJump);
    endScope(vm);
}

static void ifStatement(VM* vm, bool canAssign) {
    expression(vm);

    int thenJump = EMIT_JUMP(OP_JUMP_IF_FALSE);
    EMIT_BYTE(OP_POP);
    consume(TOKEN_LEFT_BRACE, "Expected '{' for 'if' body");
    block(vm);
    int elseJump = EMIT_JUMP(OP_JUMP);
    patchJump(thenJump);
    EMIT_BYTE(OP_POP);

    if (match(TOKEN_ELSE)) {
        consume(TOKEN_LEFT_BRACE, "Expected '{' for 'if' body");
        block(vm);
    }
    patchJump(elseJump);
}

static void printStatement(VM* vm, bool canAssign) {
    expression(vm);
    consume(TOKEN_SEMICOLON, "Expect ';' after value.");
    EMIT_BYTE(OP_PRINT);
}

static void returnStatement(VM* vm, bool canAssign) {
    if (current->type == TYPE_SCRIPT) {
        error("Can't return from top-level code.");
    }

    if (match(TOKEN_SEMICOLON)) {
        EMIT_RETURN();
    } else {
        if (current->type == TYPE_INITIALIZER) {
            error("Can't return a value from an initializer.");
        }

        expression(vm);
        consume(TOKEN_SEMICOLON, "Expect ';' after return value.");
        EMIT_BYTE(OP_RETURN);
    }
}

static void whileStatement(VM* vm, bool canAssign) {
    EMIT_BYTE(OP_NIL);

    int loopStart = currentChunk()->count;
    expression(vm);

    int exitJump = EMIT_JUMP(OP_JUMP_IF_FALSE);
    EMIT_BYTES(OP_POP, OP_POP);
    consume(TOKEN_LEFT_BRACE, "Expected '{' for 'while' body");
    block(vm);
    EMIT_LOOP(loopStart);

    patchJump(exitJump);
    EMIT_BYTE(OP_POP);
}

static void synchronize() {
    parser.panicMode = false;
    while (parser.current.type != TOKEN_EOF) {
        if (parser.previous.type == TOKEN_SEMICOLON) return;
        switch (parser.current.type) {
            case TOKEN_CLASS:
            case TOKEN_FUNC:
            case TOKEN_VAR:
            case TOKEN_FOR:
            case TOKEN_IF:
            case TOKEN_WHILE:
            case TOKEN_PRINT:
            case TOKEN_RETURN:
                return;
            default: ;
        }
        advance();
    }
}

static void expression(VM* vm) {
    parsePrecedence(vm, PREC_ASSIGNMENT);
}

static void block(VM* vm) {
    if (check(TOKEN_RIGHT_BRACE)) EMIT_BYTE(OP_NIL); // Empty block
    while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) {
        if (match(TOKEN_CLASS)) {
            classDeclaration(vm, false);
        } else if (match(TOKEN_FOR)) {
            forStatement(vm, false);
        } else if (match(TOKEN_FUNC)) {
            funcDeclaration(vm, false);
        } else if (match(TOKEN_IF)) {
            ifStatement(vm, false);
        } else if (match(TOKEN_PRINT)) {
            printStatement(vm, false);
        } else if (match(TOKEN_RETURN)) {
            returnStatement(vm, false);
        } else if (match(TOKEN_VAR)) {
            varDeclaration(vm, false);
        } else if (match(TOKEN_WHILE)) {
            whileStatement(vm, false);
        } else if (match(TOKEN_LEFT_BRACE)) {
            beginScope();
            block(vm);
            endScope(vm);
        } else {
            expression(vm);
            if (!match(TOKEN_SEMICOLON) && !check(TOKEN_RIGHT_BRACE))
                errorAtCurrent("Expect ';' or '}' at end of expression.");
        }
        if (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) EMIT_BYTE(OP_POP);
    }
    consume(TOKEN_RIGHT_BRACE, "Expect '}' after block.");
}

static void blockExpr(VM* vm, bool canAssign) {
    beginScope();
    block(vm);
    endScope(vm);
}

static void statement(VM* vm) {
    if (match(TOKEN_CLASS)) {
        classDeclaration(vm, false);
    } else if (match(TOKEN_FOR)) {
        forStatement(vm, false);
    } else if (match(TOKEN_FUNC)) {
        funcDeclaration(vm, false);
    } else if (match(TOKEN_IF)) {
        ifStatement(vm, false);
    } else if (match(TOKEN_PRINT)) {
        printStatement(vm, false);
    } else if (match(TOKEN_RETURN)) {
        returnStatement(vm, false);
    } else if (match(TOKEN_VAR)) {
        varDeclaration(vm, false);
    } else if (match(TOKEN_WHILE)) {
        whileStatement(vm, false);
    } else if (match(TOKEN_LEFT_BRACE)) {
        beginScope();
        block(vm);
        endScope(vm);
    } else {
        expressionStatement(vm);
    }

    EMIT_BYTE(OP_POP);

    if (parser.panicMode) synchronize();
}

ObjFunction* compile(VM* vm, const char* source) {
    initScanner(source);
    Compiler compiler;
    initCompiler(vm, &compiler, TYPE_SCRIPT);

    parser.hadError = false;
    parser.panicMode = false;

    advance();
    
    while (!match(TOKEN_EOF)) {
        statement(vm);
    }

    ObjFunction* function = endCompiler(vm);
    return parser.hadError ? NULL : function;
}

void markCompilerRoots(VM* vm) {
    Compiler* compiler = current;
    while (compiler != NULL) {
        markObject(vm, (Obj*)compiler->function);
        compiler = compiler->enclosing;
    }
}