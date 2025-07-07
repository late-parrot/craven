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

#include <stdlib.h>

#include "builtins.h"
#include "compiler.h"
#include "memory.h"

#ifdef DEBUG_LOG_GC
#include <stdio.h>
#include "debug.h"
#endif

#define GC_HEAP_GROW_FACTOR 2

void* reallocate(VM* vm, void* pointer, size_t oldSize, size_t newSize) {
    vm->bytesAllocated += newSize - oldSize;
    if (newSize > oldSize) {
#ifdef DEBUG_STRESS_GC
        collectGarbage(vm);
#endif
        if (vm->bytesAllocated > vm->nextGC) {
            collectGarbage(vm);
        }
    }
    
    if (newSize == 0) {
        free(pointer);
        return NULL;
    }

    void* result = realloc(pointer, newSize);
    if (result == NULL) exit(1);
    return result;
}

void markObject(VM* vm, Obj* object) {
    if (object == NULL) return;
    if (object->isMarked) return;
#ifdef DEBUG_LOG_GC
    printf("%p mark ", (void*)object);
    printValue(OBJ_VAL(object));
    printf("\n");
#endif

    object->isMarked = true;

    if (vm->grayCapacity < vm->grayCount + 1) {
        vm->grayCapacity = GROW_CAPACITY(vm->grayCapacity);
        vm->grayStack = (Obj**)realloc(vm->grayStack, sizeof(Obj*) * vm->grayCapacity);
        if (vm->grayStack == NULL) exit(1);
    }

    vm->grayStack[vm->grayCount++] = object;
}

void markValue(VM* vm, Value value) {
    if (IS_OBJ(value)) markObject(vm, AS_OBJ(value));
}

static void markArray(VM* vm, ValueArray* array) {
    for (int i = 0; i < array->count; i++) {
        markValue(vm, array->values[i]);
    }
}

static void blackenObject(VM* vm, Obj* object) {
#ifdef DEBUG_LOG_GC
    printf("%p blacken ", (void*)object);
    printValue(OBJ_VAL(object));
    printf("\n");
#endif
    switch (object->type) {
        case OBJ_BOUND_METHOD: {
            ObjBoundMethod* bound = (ObjBoundMethod*)object;
            markValue(vm, bound->receiver);
            markObject(vm, (Obj*)bound->method);
            break;
        }
        case OBJ_BOUND_NATIVE: {
            ObjBoundNative* bound = (ObjBoundNative*)object;
            markValue(vm, bound->receiver);
            break;
        }
        case OBJ_CLASS: {
            ObjClass* klass = (ObjClass*)object;
            markObject(vm, (Obj*)klass->name);
            markTable(vm, &klass->methods);
            break;
        }
        case OBJ_CLOSURE: {
            ObjClosure* closure = (ObjClosure*)object;
            markObject(vm, (Obj*)closure->function);
            for (int i = 0; i < closure->upvalueCount; i++) {
                markObject(vm, (Obj*)closure->upvalues[i]);
            }
            break;
        }
        case OBJ_FUNCTION: {
            ObjFunction* function = (ObjFunction*)object;
            markObject(vm, (Obj*)function->name);
            markArray(vm, &function->chunk.constants);
            break;
        }
        case OBJ_INSTANCE: {
            ObjInstance* instance = (ObjInstance*)object;
            markObject(vm, (Obj*)instance->klass);
            markTable(vm, &instance->fields);
            break;
        }
        case OBJ_LIST:
            markArray(vm, &((ObjList*)object)->values);
            break;
        case OBJ_UPVALUE:
            markValue(vm, ((ObjUpvalue*)object)->closed);
            break;
        case OBJ_NATIVE:
        case OBJ_STRING:
            break;
        
    }
}

static void freeObject(VM* vm, Obj* object) {
#ifdef DEBUG_LOG_GC
    printf("%p free type %d\n", (void*)object, object->type);
#endif

    switch (object->type) {
        case OBJ_BOUND_METHOD:
            FREE(ObjBoundMethod, object);
            break;
        case OBJ_BOUND_NATIVE:
            FREE(ObjBoundNative, object);
            break;
        case OBJ_CLASS: {
            ObjClass* klass = (ObjClass*)object;
            freeTable(vm, &klass->methods);
            FREE(ObjClass, object);
            break;
        }
        case OBJ_CLOSURE: {
            ObjClosure* closure = (ObjClosure*)object;
            FREE_ARRAY(ObjUpvalue*, closure->upvalues, closure->upvalueCount);
            FREE(ObjClosure, object);
            break;
        }
        case OBJ_DICT: {
            ObjDict* dict = (ObjDict*)object;
            freeTable(vm, &dict->values);
            FREE(ObjDict, object);
            break;
        }
        case OBJ_FUNCTION: {
            ObjFunction* function = (ObjFunction*)object;
            freeChunk(vm, &function->chunk);
            FREE(ObjFunction, object);
            break;
        }
        case OBJ_INSTANCE: {
            ObjInstance* instance = (ObjInstance*)object;
            freeTable(vm, &instance->fields);
            FREE(ObjInstance, object);
            break;
        }
        case OBJ_LIST: {
            ObjList* list = (ObjList*)object;
            freeValueArray(vm, &list->values);
            FREE(ObjList, object);
            break;
        }
        case OBJ_NATIVE:
            FREE(ObjNative, object);
            break;
        case OBJ_STRING: {
            ObjString* string = (ObjString*)object;
            FREE_ARRAY(char, string->chars, string->length + 1);
            FREE(ObjString, object);
            break;
        }
        case OBJ_UPVALUE:
            FREE(ObjUpvalue, object);
            break;
    }
}

static void markRoots(VM* vm) {
    for (Value* slot = vm->stack; slot < vm->stackTop; slot++) {
        markValue(vm, *slot);
    }

    for (int i = 0; i < vm->frameCount; i++) {
        markObject(vm, (Obj*)vm->frames[i].closure);
    }

    for (ObjUpvalue* upvalue = vm->openUpvalues; upvalue != NULL;
        upvalue = upvalue->next) {
        markObject(vm, (Obj*)upvalue);
    }

    markTable(vm, &vm->globals);
    markBuiltins(vm, &vm->builtins);
    markValue(vm, vm->reserve);
    markCompilerRoots(vm);
    markObject(vm, (Obj*)vm->initString);
}

static void traceReferences(VM* vm) {
    while (vm->grayCount > 0) {
        Obj* object = vm->grayStack[--vm->grayCount];
        blackenObject(vm, object);
    }
}

static void sweep(VM* vm) {
    Obj* previous = NULL;
    Obj* object = vm->objects;
    while (object != NULL) {
        if (object->isMarked) {
            object->isMarked = false;
            previous = object;
            object = object->next;
        } else {
            Obj* unreached = object;
            object = object->next;
            if (previous != NULL) {
                previous->next = object;
            } else {
                vm->objects = object;
            }
            freeObject(vm, unreached);
        }
    }
}

void collectGarbage(VM* vm) {
#ifdef DEBUG_LOG_GC
    printf("-- gc begin\n");
    size_t before = vm->bytesAllocated;
#endif

    markRoots(vm);
    traceReferences(vm);
    tableRemoveWhite(vm, &vm->strings);
    sweep(vm);

    vm->nextGC = vm->bytesAllocated * GC_HEAP_GROW_FACTOR;

#ifdef DEBUG_LOG_GC
    printf("-- gc end\n");
    printf(
        "   collected %zu bytes (from %zu to %zu) next at %zu\n",
        before - vm->bytesAllocated, before, vm->bytesAllocated, vm->nextGC
    );
#endif
}

void freeObjects(VM* vm) {
    Obj* object = vm->objects;
    while (object != NULL) {
        Obj* next = object->next;
        freeObject(vm, object);
        object = next;
    }
    free(vm->grayStack);
}