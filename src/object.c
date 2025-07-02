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
#include <string.h>

#include "memory.h"
#include "object.h"

#define PUSH(value) \
    do { \
        if (!push(vm, value)) fatalError(vm, "Cannot push value."); \
    } while (false)

#define POP() pop(vm)

#define PEEK(amount) peek(vm, amount)

#define ALLOCATE_OBJ(type, objectType) \
    (type*)allocateObject(vm, sizeof(type), objectType)

static Obj* allocateObject(VM* vm, size_t size, ObjType type) {
    Obj* object = (Obj*)reallocate(vm, NULL, 0, size);
    object->type = type;
    object->isMarked = false;

    object->next = vm->objects;
    vm->objects = object;

#ifdef DEBUG_LOG_GC
    printf("%p allocate %zu for %d\n", (void*)object, size, type);
#endif

    return object;
}

ObjBoundMethod* newBoundMethod(VM* vm, Value receiver, ObjClosure* method) {
    ObjBoundMethod* bound = ALLOCATE_OBJ(ObjBoundMethod, OBJ_BOUND_METHOD);
    bound->receiver = receiver;
    bound->method = method;
    return bound;
}

ObjBoundNative* newBoundNative(VM* vm, Value receiver, NativeFn method) {
    ObjBoundNative* bound = ALLOCATE_OBJ(ObjBoundNative, OBJ_BOUND_NATIVE);
    bound->receiver = receiver;
    bound->method = method;
    return bound;
}

ObjClass* newClass(VM* vm, ObjString* name) {
    ObjClass* klass = ALLOCATE_OBJ(ObjClass, OBJ_CLASS);
    klass->name = name;
    initTable(&klass->methods);
    return klass;
}

ObjClosure* newClosure(VM* vm, ObjFunction* function) {
    ObjUpvalue** upvalues = ALLOCATE(ObjUpvalue*, function->upvalueCount);
    for (int i = 0; i < function->upvalueCount; i++) {
        upvalues[i] = NULL;
    }
    ObjClosure* closure = ALLOCATE_OBJ(ObjClosure, OBJ_CLOSURE);
    closure->function = function;
    closure->upvalues = upvalues;
    closure->upvalueCount = function->upvalueCount;
    return closure;
}

ObjDict* newDict(VM* vm) {
    ObjDict* dict = ALLOCATE_OBJ(ObjDict, OBJ_DICT);
    initTable(&dict->values);
    return dict;
}

ObjFunction* newFunction(VM* vm) {
    ObjFunction* function = ALLOCATE_OBJ(ObjFunction, OBJ_FUNCTION);
    function->arity = 0;
    function->upvalueCount = 0;
    function->name = NULL;
    initChunk(&function->chunk);
    return function;
}

ObjInstance* newInstance(VM* vm, ObjClass* klass) {
    ObjInstance* instance = ALLOCATE_OBJ(ObjInstance, OBJ_INSTANCE);
    instance->klass = klass;
    initTable(&instance->fields);
    return instance;
}

ObjList* newList(VM* vm) {
    ObjList* list = ALLOCATE_OBJ(ObjList, OBJ_LIST);
    initValueArray(&list->values);
    return list;
}

ObjNative* newNative(VM* vm, NativeFn function) {
    ObjNative* native = ALLOCATE_OBJ(ObjNative, OBJ_NATIVE);
    native->function = function;
    return native;
}

static ObjString* allocateString(VM* vm, char* chars, int length, uint32_t hash) {
    ObjString* string = ALLOCATE_OBJ(ObjString, OBJ_STRING);
    string->length = length;
    string->chars = chars;
    string->hash = hash;

    PUSH(OBJ_VAL(string));
    tableSet(vm, &vm->strings, OBJ_VAL(string), NIL_VAL);
    POP();

    return string;
}

static uint32_t hashString(const char* key, int length) {
    // FNV-1a algorithm
    uint32_t hash = 2166136261u;
    for (int i = 0; i < length; i++) {
        hash ^= (uint8_t)key[i];
        hash *= 16777619;
    }
    return hash;
}

ObjString* takeString(VM* vm, char* chars, int length) {
    uint32_t hash = hashString(chars, length);
    ObjString* interned = tableFindString(&vm->strings, chars, length, hash);
    if (interned != NULL) {
        FREE_ARRAY(char, chars, length + 1);
        return interned;
    }

    return allocateString(vm, chars, length, hash);
}

ObjString* copyString(VM* vm, const char* chars, int length) {
    uint32_t hash = hashString(chars, length);
    ObjString* interned = tableFindString(&vm->strings, chars, length, hash);
    if (interned != NULL) return interned;

    char* heapChars = ALLOCATE(char, length + 1);
    memcpy(heapChars, chars, length);
    heapChars[length] = '\0';
    ObjString* debug = allocateString(vm, heapChars, length, hash);
    return debug; //allocateString(heapChars, length, hash);
}

ObjUpvalue* newUpvalue(VM* vm, Value* slot) {
    ObjUpvalue* upvalue = ALLOCATE_OBJ(ObjUpvalue, OBJ_UPVALUE);
    upvalue->closed = NIL_VAL;
    upvalue->location = slot;
    upvalue->next = NULL;
    return upvalue;
}

static void printFunction(ObjFunction* function) {
    if (function->name == NULL) {
        printf("<script>");
        return;
    }
    printf("<func %s>", function->name->chars);
}

void printObject(Value value) {
    switch (OBJ_TYPE(value)) {
        case OBJ_BOUND_METHOD:
            printFunction(AS_BOUND_METHOD(value)->method->function);
            break;
        case OBJ_BOUND_NATIVE:
            printf("<native fn>");
            break;
        case OBJ_CLASS:
            printf("%s", AS_CLASS(value)->name->chars);
            break;
        case OBJ_CLOSURE:
            printFunction(AS_CLOSURE(value)->function);
            break;
        case OBJ_FUNCTION:
            printFunction(AS_FUNCTION(value));
            break;
        case OBJ_INSTANCE:
            printf("%s instance", AS_INSTANCE(value)->klass->name->chars);
            break;
        case OBJ_LIST: {
            ObjList* list = AS_LIST(value);
            printf("[");
            for (Value* v = list->values.values;
                v - list->values.values < list->values.count; v++) {
                printValue(*v);
                if (v + 1 - list->values.values < list->values.count)
                    printf(", ");
            }
            printf("]");
            break;
        }
        case OBJ_DICT: {
            ObjDict* dict = AS_DICT(value);
            Entry previous = {EMPTY_VAL, NIL_VAL};
            printf("dict {");
            for (Entry* e = dict->values.entries;
                e - dict->values.entries < dict->values.capacity; e++) {
                if (IS_EMPTY(e->key)) continue;
                if (!IS_EMPTY(previous.key)) {
                    printValue(previous.key);
                    printf(" => ");
                    printValue(previous.value);
                    printf(", ");
                }
                previous = *e;
            }
            printValue(previous.key);
            printf(" => ");
            printValue(previous.value);
            printf("}");
            break;
        }
        case OBJ_NATIVE:
            printf("<native fn>");
            break;
        case OBJ_STRING:
            printf("%s", AS_CSTRING(value));
            break;
        case OBJ_UPVALUE:
            printf("upvalue");
            break;
    }
}