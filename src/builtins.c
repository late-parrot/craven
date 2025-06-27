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

#include "builtins.h"
#include "object.h"
#include "vm.h"

#define PUSH(value) \
    do { \
        if (!push(value)) return false; \
    } while (false)

#define CHECK_ARGCOUNT(expected) \
    do { \
        if (argCount != expected) { \
            runtimeError("%d args expected but got %d.", expected, argCount); \
            return false; \
        } \
    } while (false)

static bool stringLengthNative(int argCount, Value* args) {
    CHECK_ARGCOUNT(0);
    PUSH(NUMBER_VAL(AS_STRING(args[-1])->length));
    return true;
}

static bool listAppendNative(int argCount, Value* args) {
    CHECK_ARGCOUNT(1);
    writeValueArray(&AS_LIST(args[-1])->values, args[0]);
    PUSH(args[0]);
    return true;
}

static bool listLengthNative(int argCount, Value* args) {
    CHECK_ARGCOUNT(0);
    PUSH(NUMBER_VAL(AS_LIST(args[-1])->values.count));
    return true;
}

void initBuiltins(Builtins* builtins) {
    initTable(&builtins->stringMembers);
    initTable(&builtins->listMembers);
    initTable(&builtins->dictMembers);
}

static void addBuiltin(Table* table, const char* name, NativeFn native) {
    tableSet(
        table,
        OBJ_VAL(copyString(name, strlen(name))),
        OBJ_VAL(newNative(native))
    );
}

void createBuiltins(Builtins* builtins) {
    addBuiltin(&builtins->stringMembers, "length", stringLengthNative);

    addBuiltin(&builtins->listMembers, "append", listAppendNative);
    addBuiltin(&builtins->listMembers, "length", listLengthNative);
}

void markBuiltins(Builtins* builtins) {
    markTable(&builtins->stringMembers);
    markTable(&builtins->listMembers);
    markTable(&builtins->dictMembers);
}

void freeBuiltins(Builtins* builtins) {
    freeTable(&builtins->stringMembers);
    freeTable(&builtins->listMembers);
    freeTable(&builtins->dictMembers);
}