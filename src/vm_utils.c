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

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "common.h"
#include "compiler.h"
#include "debug.h"
#include "object.h"
#include "memory.h"
#include "value.h"
#include "vm_utils.h"

#define PUSH(value) \
    do { \
        if (!push(vm, value)) fatalError(vm, "Cannot push value."); \
    } while (false)

#define POP() pop(vm)

#define PEEK(amount) peek(vm, amount)

void resetStack(VM* vm) {
    vm->stackTop = vm->stack;
    vm->frameCount = 0;
    vm->openUpvalues = NULL;
}

static void errorV(VM* vm, const char* format, va_list args) {
    vfprintf(stderr, format, args);
    fputs("\n", stderr);

    for (int i = vm->frameCount - 1; i >= 0; i--) {
        CallFrame* frame = &vm->frames[i];
        ObjFunction* function = frame->closure->function;
        size_t instruction = frame->ip - function->chunk.code - 1;
        fprintf(stderr, "[line %d] in ", function->chunk.lines[instruction]);
        if (function->name == NULL) {
            fprintf(stderr, "script\n");
        } else {
            fprintf(stderr, "%s()\n", function->name->chars);
        }
    }

    resetStack(vm);
}

void runtimeError(VM* vm, const char* format, ...) {
    va_list args;
    va_start(args, format);
    errorV(vm, format, args);
    va_end(args);
    fputs("\n", stderr);
}

void fatalError(VM* vm, const char* format, ...) {
    va_list args;
    va_start(args, format);
    errorV(vm, format, args);
    va_end(args);
    fputs("\n", stderr);

    vm->kill = true; // Kills after current instruction, best I can do for now
}

bool push(VM* vm, Value value) {
    if (vm->stackTop - vm->stack >= STACK_MAX) {
        runtimeError(vm, "Stack overflow.");
        return false;
    }
    *vm->stackTop = value;
    vm->stackTop++;
    return true;
}

Value pop(VM* vm) {
    vm->stackTop--;
    return *vm->stackTop;
}

Value peek(VM* vm, int distance) {
    return vm->stackTop[-1 - distance];
}

bool defineNative(VM* vm, const char* name, NativeFn function) {
    ObjString *s = copyString(vm, name, (int)strlen(name));
    PUSH(OBJ_VAL(copyString(vm, name, (int)strlen(name))));
    PUSH(OBJ_VAL(newNative(vm, function)));
    Value nameDB = vm->stack[0];
    Value funcDB = vm->stack[1];
    ObjString* nativeNameDB = AS_STRING(vm->stack[0]);
    tableSet(vm, &vm->globals, vm->stack[0], vm->stack[1]);
    POP();
    POP();
    return true;
}

bool callClosure(VM* vm, ObjClosure* closure, int argCount) {
    if (argCount != closure->function->arity) {
        runtimeError(vm,
            "Expected %d arguments but got %d.",
            closure->function->arity, argCount
        );
        return false;
    }

    if (vm->frameCount == FRAMES_MAX) {
        runtimeError(vm, "Stack overflow.");
        return false;
    }

    CallFrame* frame = &vm->frames[vm->frameCount++];
    frame->closure = closure;
    frame->ip = closure->function->chunk.code;
    frame->slots = vm->stackTop - argCount - 1;
    return true;
}

bool callValue(VM* vm, Value callee, int argCount) {
    if (IS_OBJ(callee)) {
        switch (OBJ_TYPE(callee)) {
            case OBJ_BOUND_METHOD: {
                ObjBoundMethod* bound = AS_BOUND_METHOD(callee);
                vm->stackTop[-argCount - 1] = bound->receiver;
                return callClosure(vm, bound->method, argCount);
            }
            case OBJ_BOUND_NATIVE: {
                ObjBoundNative* bound = AS_BOUND_NATIVE(callee);
                vm->stackTop[-argCount - 1] = bound->receiver;
                if (!bound->method(vm, argCount, vm->stackTop - argCount)) return false;
                Value result = POP();
                vm->stackTop -= argCount + 1;
                PUSH(result);
                return true;
            }
            case OBJ_CLASS: {
                ObjClass* klass = AS_CLASS(callee);
                vm->stackTop[-argCount - 1] = OBJ_VAL(newInstance(vm, klass));
                Value initializer;
                if (tableGet(vm, &klass->methods, OBJ_VAL(vm->initString), &initializer)) {
                    return callClosure(vm, AS_CLOSURE(initializer), argCount);
                } else if (argCount != 0) {
                    runtimeError(vm, "Expected 0 arguments but got %d.", argCount);
                    return false;
                }
                return true;
            }
            case OBJ_CLOSURE: 
                return callClosure(vm, AS_CLOSURE(callee), argCount);
            case OBJ_NATIVE: {
                NativeFn native = AS_NATIVE(callee);
                if (!native(vm, argCount, vm->stackTop - argCount)) return false;
                Value result = POP();
                vm->stackTop -= argCount + 1;
                PUSH(result);
                return true;
            }
            default:
                break; // Non-callable object type.
        }
    }
    runtimeError(vm, "Can only call functions and classes.");
    return false;
}

bool getIndex(VM* vm, Value object, Value index) {
    switch (OBJ_TYPE(object)) {
        case OBJ_DICT: {
            ObjDict* dict = AS_DICT(object);
            Value value;
            if (!tableGet(vm, &dict->values, index, &value)) {
                runtimeError(vm, "Dict key not present.");
                return false;
            }
            PUSH(value);
            return true;
        }
        case OBJ_LIST: {
            ObjList* list = AS_LIST(object);
            if (!IS_NUMBER(index)) {
                runtimeError(vm, "List index must be a number.");
                return false;
            }
            double idx = AS_NUMBER(index);
            if (idx != floor(idx)) {
                runtimeError(vm, "List index must be a whole number.");
                return false;
            }
            if (idx < 0 || idx >= list->values.count) {
                runtimeError(vm, "List index out of bounds.");
                return false;
            }
            PUSH(list->values.values[(int)idx]);
            return true;
        }
        case OBJ_STRING: {
            ObjString* string = AS_STRING(object);
            if (!IS_NUMBER(index)) {
                runtimeError(vm, "String index must be a number.");
                return false;
            }
            double idx = AS_NUMBER(index);
            if (idx != floor(idx)) {
                runtimeError(vm, "String index must be a whole number.");
                return false;
            }
            if (idx < 0 || idx >= string->length) {
                runtimeError(vm, "String index out of bounds.");
                return false;
            }
            PUSH(OBJ_VAL(copyString(vm, &string->chars[(int)idx], 1)));
            return true;
        }
        default:
            break;
    }
    runtimeError(vm, "Can only index lists and strings.");
    return false;
}

bool rawGetIndex(VM* vm, Value object, int index) {
    switch (OBJ_TYPE(object)) {
        case OBJ_DICT: {
            /* 
             * This method doesn't make any sense for dictionaries,
             * because they don't store values in certain indecies.
             * This method is primarily used for for loops, and those
             * are implemented differently for dicts.
             */
            break;
        }
        case OBJ_LIST: {
            ObjList* list = AS_LIST(object);
            if (index < 0 || index >= list->values.count) {
                return false;
            }
            PUSH(list->values.values[index]);
            return true;
        }
        case OBJ_STRING: {
            ObjString* string = AS_STRING(object);
            if (index < 0 || index >= string->length) {
                return false;
            }
            PUSH(OBJ_VAL(copyString(vm, &string->chars[index], 1)));
            return true;
        }
        default:
            break;
    }
    return false;
}

bool setIndex(VM* vm, Value object, Value index, Value value) {
    switch (OBJ_TYPE(object)) {
        case OBJ_DICT: {
            ObjDict* dict = AS_DICT(object);
            tableSet(vm, &dict->values, index, value);
            PUSH(value);
            return true;
        }
        case OBJ_LIST: {
            ObjList* list = AS_LIST(object);
            if (!IS_NUMBER(index)) {
                runtimeError(vm, "List index must be a number.");
                return false;
            }
            double idx = AS_NUMBER(index);
            if (idx != floor(idx)) {
                runtimeError(vm, "List index must be a whole number.");
                return false;
            }
            if (idx < 0 || idx >= list->values.count) {
                runtimeError(vm, "List index out of bounds.");
                return false;
            }
            list->values.values[(int)idx] = value;
            return true;
        }
        case OBJ_STRING: {
            runtimeError(vm, "Cannot assign to string indexes.");
            return false;
        }
        default:
            break;
    }
    runtimeError(vm, "Can only index lists and strings.");
    return false;
}

bool invokeFromClass(VM* vm, ObjClass* klass, ObjString* name, int argCount) {
    Value method;
    if (!tableGet(vm, &klass->methods, OBJ_VAL(name), &method)) {
        runtimeError(vm, "Undefined property '%s'.", name->chars);
        return false;
    }
    return callClosure(vm, AS_CLOSURE(method), argCount);
}

bool invoke(VM* vm, ObjString* name, int argCount) {
    Value receiver = PEEK(argCount);
    switch (OBJ_TYPE(receiver)) {
        case OBJ_INSTANCE: {
            ObjInstance* instance = AS_INSTANCE(receiver);
            Value value;
            if (tableGet(vm, &instance->fields, OBJ_VAL(name), &value)) {
                vm->stackTop[-argCount - 1] = value;
                return callValue(vm, value, argCount);
            }
            return invokeFromClass(vm, instance->klass, name, argCount);
        }
        case OBJ_STRING: {
            Value method;
            if (tableGet(vm, &vm->builtins.stringMembers, OBJ_VAL(name), &method)) {
                if (IS_NATIVE(method)) {
                    callValue(vm, OBJ_VAL(newBoundNative(vm, receiver, AS_NATIVE(method))), argCount);
                    return true;
                }
            }
            runtimeError(vm, "Undefined method '%s'.", name->chars);
            return false;
        }
        case OBJ_LIST: {
            Value method;
            if (tableGet(vm, &vm->builtins.listMembers, OBJ_VAL(name), &method)) {
                if (IS_NATIVE(method)) {
                    callValue(vm, OBJ_VAL(newBoundNative(vm, receiver, AS_NATIVE(method))), argCount);
                    return true;
                }
            }
            runtimeError(vm, "Undefined method '%s'.", name->chars);
            return false;
        }
        case OBJ_DICT: {
            Value method;
            if (tableGet(vm, &vm->builtins.dictMembers, OBJ_VAL(name), &method)) {
                if (IS_NATIVE(method)) {
                    callValue(vm, OBJ_VAL(newBoundNative(vm, receiver, AS_NATIVE(method))), argCount);
                    return true;
                }
            }
            runtimeError(vm, "Undefined method '%s'.", name->chars);
            return false;
        }
        case OBJ_OPTION: {
            Value method;
            if (tableGet(vm, &vm->builtins.optionMembers, OBJ_VAL(name), &method)) {
                if (IS_NATIVE(method)) {
                    callValue(vm, OBJ_VAL(newBoundNative(vm, receiver, AS_NATIVE(method))), argCount);
                    return true;
                }
            }
            runtimeError(vm, "Undefined method '%s'.", name->chars);
            return false;
        }
        default:
            break;
    }
    runtimeError(vm, "Value has no properties.");
    return false;
}

bool bindMethod(VM* vm, ObjClass* klass, ObjString* name) {
    Value method;
    if (!tableGet(vm, &klass->methods, OBJ_VAL(name), &method)) {
        runtimeError(vm, "Undefined property '%s'.", name->chars);
        return false;
    }

    ObjBoundMethod* bound = newBoundMethod(vm, PEEK(0), AS_CLOSURE(method));
    POP();
    PUSH(OBJ_VAL(bound));
    return true;
}

bool getProperty(VM* vm, Value obj, ObjString* name) {
    if (IS_OBJ(obj)) switch (OBJ_TYPE(obj)) {
        case OBJ_INSTANCE: {
            ObjInstance* instance = AS_INSTANCE(obj);

            Value property;
            if (tableGet(vm, &instance->fields, OBJ_VAL(name), &property)) {
                POP();
                PUSH(property);
                return true;
            }

            if (!bindMethod(vm, instance->klass, name)) {
                return false;
            }
            return true;
        }
        case OBJ_STRING: {
            Value method;
            if (tableGet(vm, &vm->builtins.stringMembers, OBJ_VAL(name), &method)) {
                if (IS_NATIVE(method)) {
                    PUSH(OBJ_VAL(newBoundNative(vm, obj, AS_NATIVE(method))));
                    return true;
                }
            }
            runtimeError(vm, "Undefined property '%s'.", name->chars);
            return false;
        }
        case OBJ_LIST: {
            Value method;
            if (tableGet(vm, &vm->builtins.listMembers, OBJ_VAL(name), &method)) {
                if (IS_NATIVE(method)) {
                    PUSH(OBJ_VAL(newBoundNative(vm, obj, AS_NATIVE(method))));
                    return true;
                }
            }
            runtimeError(vm, "Undefined property '%s'.", name->chars);
            return false;
        }
        case OBJ_DICT: {
            Value method;
            if (tableGet(vm, &vm->builtins.dictMembers, OBJ_VAL(name), &method)) {
                if (IS_NATIVE(method)) {
                    PUSH(OBJ_VAL(newBoundNative(vm, obj, AS_NATIVE(method))));
                    return true;
                }
            }
            runtimeError(vm, "Undefined property '%s'.", name->chars);
            return false;
        }
        case OBJ_OPTION: {
            Value method;
            if (tableGet(vm, &vm->builtins.optionMembers, OBJ_VAL(name), &method)) {
                if (IS_NATIVE(method)) {
                    PUSH(OBJ_VAL(newBoundNative(vm, obj, AS_NATIVE(method))));
                    return true;
                }
            }
            runtimeError(vm, "Undefined property '%s'.", name->chars);
            return false;
        }
        default:
            break;
    }
    runtimeError(vm, "Value has no properties.");
    return false;
}

bool setProperty(VM* vm, Value obj, ObjString* name, Value value) {
    if (IS_OBJ(obj)) switch (OBJ_TYPE(obj)) {
        case OBJ_INSTANCE:
            ObjInstance* instance = AS_INSTANCE(obj);
            Value property;
            tableSet(vm, &instance->fields, OBJ_VAL(name), value);
            POP(); // Value
            POP(); // Object
            PUSH(value);
            return true;
        default:
            break;
    }
    runtimeError(vm, "Value has no fields.");
    return false;
}

ObjUpvalue* captureUpvalue(VM* vm, Value* local) {
    ObjUpvalue* prevUpvalue = NULL;
    ObjUpvalue* upvalue = vm->openUpvalues;
    while (upvalue != NULL && upvalue->location > local) {
        prevUpvalue = upvalue;
        upvalue = upvalue->next;
    }

    if (upvalue != NULL && upvalue->location == local) {
        return upvalue;
    }

    ObjUpvalue* createdUpvalue = newUpvalue(vm, local);
    createdUpvalue->next = upvalue;

    if (prevUpvalue == NULL) {
        vm->openUpvalues = createdUpvalue;
    } else {
        prevUpvalue->next = createdUpvalue;
    }

    return createdUpvalue;
}

void closeUpvalues(VM* vm, Value* last) {
    while (vm->openUpvalues != NULL && vm->openUpvalues->location >= last) {
        ObjUpvalue* upvalue = vm->openUpvalues;
        upvalue->closed = *upvalue->location;
        upvalue->location = &upvalue->closed;
        vm->openUpvalues = upvalue->next;
    }
}

void defineMethod(VM* vm, ObjString* name) {
    Value method = PEEK(0);
    ObjClass* klass = AS_CLASS(PEEK(1));
    tableSet(vm, &klass->methods, OBJ_VAL(name), method);
    POP();
}

bool isFalsey(Value value) {
    return IS_OPTION(value) && AS_OPTION(value)->isNone || (IS_BOOL(value) && !AS_BOOL(value))
        || (IS_NUMBER(value) && AS_NUMBER(value) == 0);
}

bool concatenate(VM* vm) {
    ObjString* b = AS_STRING(PEEK(0));
    ObjString* a = AS_STRING(PEEK(1));

    int length = a->length + b->length;
    char* chars = ALLOCATE(char, length + 1);
    memcpy(chars, a->chars, a->length);
    memcpy(chars + a->length, b->chars, b->length);
    chars[length] = '\0';

    ObjString* result = takeString(vm, chars, length);
    POP();
    POP();
    PUSH(OBJ_VAL(result));
    return true;
}