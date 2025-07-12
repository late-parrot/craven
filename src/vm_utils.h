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

#ifndef craven_vm_utils_h
#define craven_vm_utils_h

#include "common.h"
#include "builtins.h"
#include "table.h"

typedef uint64_t Value;
typedef struct Obj Obj;
typedef struct ObjClass ObjClass;
typedef struct ObjClosure ObjClosure;
typedef struct ObjModule ObjModule;
typedef struct ObjString ObjString;
typedef struct ObjUpvalue ObjUpvalue;
typedef bool (*NativeFn)(VM* vm, int argCount, Value* args);

#define FRAMES_MAX 64
#define STACK_MAX (FRAMES_MAX * UINT8_COUNT)

typedef struct {
    ObjClosure* closure;
    uint8_t* ip;
    Value* slots;
} CallFrame;

typedef struct VM {
    CallFrame frames[FRAMES_MAX];
    int frameCount;
    
    Value stack[STACK_MAX];
    Value* stackTop;
    Table globals;
    Table strings;
    Builtins builtins;
    ObjString* initString;
    ObjUpvalue* openUpvalues;
    Value reserve;

    size_t bytesAllocated;
    size_t nextGC;
    Obj* objects;
    int grayCount;
    int grayCapacity;
    Obj** grayStack;

    bool kill;
} VM;

typedef enum {
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR
} InterpretResult;

void resetStack(VM* vm);
void runtimeError(VM* vm, const char* format, ...);
void fatalError(VM* vm, const char* format, ...);
bool push(VM* vm, Value value);
Value pop(VM* vm);
Value peek(VM* vm, int distance);
bool defineNative(VM* vm, const char* name, NativeFn function);
bool callClosure(VM* vm, ObjClosure* closure, int argCount);
bool callValue(VM* vm, Value callee, int argCount);
bool getIndex(VM* vm, Value object, Value index);
bool rawGetIndex(VM* vm, Value object, int index);
bool setIndex(VM* vm, Value object, Value index, Value value);
bool invokeFromClass(VM* vm, ObjClass* klass, ObjString* name, int argCount);
bool invoke(VM* vm, ObjString* name, int argCount);
bool bindMethod(VM* vm, ObjClass* klass, ObjString* name);
bool getProperty(VM* vm, Value obj, ObjString* name);
bool setProperty(VM* vm, Value obj, ObjString* name, Value value);
ObjUpvalue* captureUpvalue(VM* vm, Value* local);
void closeUpvalues(VM* vm, Value* last);
void defineMethod(VM* vm, ObjString* name);
bool isFalsey(Value value);
bool concatenate(VM* vm);
InterpretResult importFile(const char* path, ObjModule* module);

#endif