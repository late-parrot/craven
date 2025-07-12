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

#include <time.h>

#include "builtins.h"
#include "object.h"
#include "value.h"
#include "vm_utils.h"

#define PUSH(value) \
    do { \
        if (!push(vm, value)) return false; \
    } while (false)

#define POP() pop(vm)

#define PEEK(amount) peek(vm, amount)

#define CHECK_ARGCOUNT(expected) \
    do { \
        if (argCount != expected) { \
            runtimeError(vm, "%d args expected but got %d.", expected, argCount); \
            return false; \
        } \
    } while (false)

#define CHECK_TYPE(value, isMacro, typename) \
    do { \
        if (!isMacro(value)) { \
            runtimeError(vm, "Expected value of type %s.", typename); \
            return false; \
        } \
    } while (false)

static bool clockNative(VM* vm, int argCount, Value* args) {
    CHECK_ARGCOUNT(0);
    PUSH(NUMBER_VAL((double)clock() / CLOCKS_PER_SEC));
    return true;
}

static bool importNative(VM* vm, int argCount, Value* args) {
    CHECK_ARGCOUNT(1);
    CHECK_TYPE(args[0], IS_STRING, "string");
    ObjModule* module = newModule(vm, AS_STRING(args[0]));
    if (importFile(AS_CSTRING(args[0]), module) != INTERPRET_OK) {
        runtimeError(vm, "Above error while importing module from %s", AS_CSTRING(args[0]));
        return false;
    }
    PUSH(OBJ_VAL(module));
    return true;
}

static bool stringLengthNative(VM* vm, int argCount, Value* args) {
    CHECK_ARGCOUNT(0);
    PUSH(NUMBER_VAL(AS_STRING(args[-1])->length));
    return true;
}

static bool listAppendNative(VM* vm, int argCount, Value* args) {
    CHECK_ARGCOUNT(1);
    writeValueArray(vm, &AS_LIST(args[-1])->values, args[0]);
    PUSH(args[0]);
    return true;
}

static bool listLengthNative(VM* vm, int argCount, Value* args) {
    CHECK_ARGCOUNT(0);
    PUSH(NUMBER_VAL(AS_LIST(args[-1])->values.count));
    return true;
}

static bool optionUnwrapNative(VM* vm, int argCount, Value* args) {
    CHECK_ARGCOUNT(0);
    ObjOption* option = AS_OPTION(args[-1]);
    if (option->isNone) {
        runtimeError(vm, "Attempted to unwrap `none`.");
        return false;
    }
    PUSH(option->value);
    return true;
}

void initBuiltins(Builtins* builtins) {
    initTable(&builtins->stringMembers);
    initTable(&builtins->listMembers);
    initTable(&builtins->dictMembers);
    initTable(&builtins->optionMembers);
}

static void addBuiltin(VM* vm, Table* table, const char* name, NativeFn native) {
    Value key = OBJ_VAL(copyString(vm, name, strlen(name)));
    push(vm, key);
    Value val = OBJ_VAL(newNative(vm, native));
    push(vm, val);
    tableSet(vm, table, key, val);
    pop(vm);
    pop(vm);
}

void createBuiltins(VM* vm, Builtins* builtins) {
    defineNative(vm, "clock", clockNative);
    defineNative(vm, "import", importNative);

    addBuiltin(vm, &builtins->stringMembers, "length", stringLengthNative);

    addBuiltin(vm, &builtins->listMembers, "append", listAppendNative);
    addBuiltin(vm, &builtins->listMembers, "length", listLengthNative);

    addBuiltin(vm, &builtins->optionMembers, "unwrap", optionUnwrapNative);
}

void markBuiltins(VM* vm, Builtins* builtins) {
    markTable(vm, &builtins->stringMembers);
    markTable(vm, &builtins->listMembers);
    markTable(vm, &builtins->dictMembers);
    markTable(vm, &builtins->optionMembers);
}

void freeBuiltins(VM* vm, Builtins* builtins) {
    freeTable(vm, &builtins->stringMembers);
    freeTable(vm, &builtins->listMembers);
    freeTable(vm, &builtins->dictMembers);
    freeTable(vm, &builtins->optionMembers);
}