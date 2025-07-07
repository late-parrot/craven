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

#include "common.h"
#include "compiler.h"
#include "debug.h"
#include "object.h"
#include "memory.h"
#include "vm.h"

#define PUSH(value) \
    do { \
        if (!push(vm, value)) return INTERPRET_RUNTIME_ERROR; \
    } while (false)

#define POP() pop(vm)

#define PEEK(amount) peek(vm, amount)

void initVM(VM* vm) {
    resetStack(vm);
    vm->objects = NULL;
    vm->bytesAllocated = 0;
    vm->nextGC = 1024 * 1024;
    vm->kill = false;

    vm->grayCount = 0;
    vm->grayCapacity = 0;
    vm->grayStack = NULL;

    initTable(&vm->globals);
    initTable(&vm->strings);
    initBuiltins(&vm->builtins);

    vm->initString = NULL;
    vm->initString = copyString(vm, "init", 4);
    // Couldn't technically overflow the stack, so no need to handle overflow.
    createBuiltins(vm, &vm->builtins);
}

void freeVM(VM* vm) {
    freeTable(vm, &vm->globals);
    freeTable(vm, &vm->strings);
    freeBuiltins(vm, &vm->builtins);
    freeObjects(vm);
}

static InterpretResult run(VM* vm) {
    CallFrame* frame = &vm->frames[vm->frameCount - 1];

#define READ_BYTE() (*frame->ip++)
#define READ_SHORT() \
    (frame->ip += 2, (uint16_t)((frame->ip[-2] << 8) | frame->ip[-1]))
#define READ_CONSTANT() \
    (frame->closure->function->chunk.constants.values[READ_BYTE()])
#define READ_STRING() AS_STRING(READ_CONSTANT())
#define BINARY_OP(valueType, op) \
    do { \
        if (!IS_NUMBER(PEEK(0)) || !IS_NUMBER(PEEK(1))) { \
            runtimeError(vm, "Operands must be numbers."); \
            return INTERPRET_RUNTIME_ERROR; \
        } \
        double b = AS_NUMBER(POP()); \
        double a = AS_NUMBER(POP()); \
        PUSH(valueType(a op b)); \
    } while (false)

    for (;;) {
#ifdef DEBUG_TRACE_EXECUTION
        printf("        |   ");
        for (Value* slot = vm->stack; slot < vm->stackTop; slot++) {
            printf("[ ");
            printValue(*slot);
            printf(" ]");
        }
        printf("\n");
        disassembleInstruction(
            &frame->closure->function->chunk,
            (int)(frame->ip - frame->closure->function->chunk.code)
        );
#endif

        uint8_t instruction;
        switch (instruction = READ_BYTE()) {
            case OP_CONSTANT: {
                Value constant = READ_CONSTANT();
                PUSH(constant);
                break;
            }
            case OP_NIL: PUSH(NIL_VAL); break;
            case OP_TRUE: PUSH(BOOL_VAL(true)); break;
            case OP_FALSE: PUSH(BOOL_VAL(false)); break;
            case OP_INT: PUSH(NUMBER_VAL((double)READ_BYTE())); break;
            case OP_LIST: {
                uint8_t elemCount = READ_BYTE();
                ObjList* list = newList(vm);
                for (Value* v = vm->stackTop - elemCount; v < vm->stackTop; v++)
                    writeValueArray(vm, &list->values, *v);
                vm->stackTop -= elemCount;
                PUSH(OBJ_VAL(list));
                break;
            }
            case OP_DICT: {
                uint16_t entryCount = READ_BYTE();
                ObjDict* dict = newDict(vm);
                for (Value* v = vm->stackTop - 2*entryCount; v < vm->stackTop; v += 2)
                    tableSet(vm, &dict->values, *v, *(v+1));
                vm->stackTop -= 2*entryCount;
                PUSH(OBJ_VAL(dict));
                break;
            }
            case OP_POP: POP(); break;
            case OP_GET_LOCAL: {
                uint8_t slot = READ_BYTE();
                PUSH(frame->slots[slot]); 
                break;
            }
            case OP_SET_LOCAL: {
                uint8_t slot = READ_BYTE();
                frame->slots[slot] = PEEK(0);
                break;
            }
            case OP_GET_GLOBAL: {
                ObjString* name = READ_STRING();
                Value value;
                if (!tableGet(vm, &vm->globals, OBJ_VAL(name), &value)) {
                    runtimeError(vm, "Undefined variable '%s'.", name->chars);
                    return INTERPRET_RUNTIME_ERROR;
                }
                PUSH(value);
                break;
            }
            case OP_DEFINE_GLOBAL: {
                ObjString* name = READ_STRING();
                tableSet(vm, &vm->globals, OBJ_VAL(name), POP());
                break;
            }
            case OP_SET_GLOBAL: {
                ObjString* name = READ_STRING();
                if (tableSet(vm, &vm->globals, OBJ_VAL(name), PEEK(0))) {
                    tableDelete(vm, &vm->globals, OBJ_VAL(name));
                    runtimeError(vm, "Undefined variable '%s'.", name->chars);
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_GET_UPVALUE: {
                uint8_t slot = READ_BYTE();
                PUSH(*frame->closure->upvalues[slot]->location);
                break;
            }
            case OP_SET_UPVALUE: {
                uint8_t slot = READ_BYTE();
                *frame->closure->upvalues[slot]->location = PEEK(0);
                break;
            }
            case OP_GET_PROPERTY: {
                if (!getProperty(vm, PEEK(0), READ_STRING())) {
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_SET_PROPERTY: {
                if (!setProperty(vm, PEEK(1), READ_STRING(), PEEK(0))) {
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_GET_SUPER: {
                ObjString* name = READ_STRING();
                ObjClass* superclass = AS_CLASS(POP());

                if (!bindMethod(vm, superclass, name)) {
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_GET_INDEX: {
                Value index = POP();
                Value object = POP();
                if (!getIndex(vm, object, index)) {
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_SET_INDEX: {
                Value value = POP();
                Value index = POP();
                Value object = POP();
                PUSH(value);
                if (!setIndex(vm, object, index, value)) {
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_GET_RESERVE: {
                PUSH(vm->reserve);
                break;
            }
            case OP_SET_RESERVE: {
                vm->reserve = POP();
                break;
            }
            case OP_EQUAL: {
                Value b = POP();
                Value a = POP();
                PUSH(BOOL_VAL(valuesEqual(a, b)));
                break;
            }
            case OP_GREATER:  BINARY_OP(BOOL_VAL, >); break;
            case OP_LESS:     BINARY_OP(BOOL_VAL, <); break;
            case OP_ADD: {
                if (IS_STRING(PEEK(0)) && IS_STRING(PEEK(1))) {
                    if (!concatenate(vm)) return INTERPRET_RUNTIME_ERROR;
                } else if (IS_NUMBER(PEEK(0)) && IS_NUMBER(PEEK(1))) {
                    double b = AS_NUMBER(POP());
                    double a = AS_NUMBER(POP());
                    PUSH(NUMBER_VAL(a + b));
                } else {
                    runtimeError(vm, "Operands must be two numbers or two strings.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_SUBTRACT: BINARY_OP(NUMBER_VAL, -); break;
            case OP_MULTIPLY: BINARY_OP(NUMBER_VAL, *); break;
            case OP_DIVIDE:   BINARY_OP(NUMBER_VAL, /); break;
            case OP_NOT:
                PUSH(BOOL_VAL(isFalsey(POP())));
                break;
            case OP_NEGATE: {
                if (!IS_NUMBER(PEEK(0))) {
                    runtimeError(vm, "Operand must be a number.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                PUSH(NUMBER_VAL(-AS_NUMBER(POP())));
                break;
            }
            case OP_PRINT: {
                printValue(PEEK(0));
                printf("\n");
                break;
            }
            case OP_JUMP: {
                uint16_t offset = READ_SHORT();
                frame->ip += offset;
                break;
            }
            case OP_JUMP_IF_FALSE: {
                uint16_t offset = READ_SHORT();
                if (isFalsey(PEEK(0))) frame->ip += offset;
                break;
            }
            case OP_NEXT_JUMP: {
                uint16_t offset = READ_SHORT();
                int index = (int)AS_NUMBER(POP());
                Value iter = PEEK(0);
                if (!IS_OBJ(iter) || OBJ_TYPE(iter) != OBJ_LIST &&
                    OBJ_TYPE(iter) != OBJ_STRING) {
                    runtimeError(vm, "Can only iterate list or string.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                PUSH(NUMBER_VAL(index+1));
                if (!rawGetIndex(vm, iter, index)) frame->ip += offset;
                break;
            }
            case OP_LOOP: {
                uint16_t offset = READ_SHORT();
                frame->ip -= offset;
                break;
            }
            case OP_CALL: {
                int argCount = READ_BYTE();
                if (!callValue(vm, PEEK(argCount), argCount)) {
                    return INTERPRET_RUNTIME_ERROR;
                }
                frame = &vm->frames[vm->frameCount - 1];
                break;
            }
            case OP_INVOKE: {
                ObjString* method = READ_STRING();
                int argCount = READ_BYTE();
                if (!invoke(vm, method, argCount)) {
                    return INTERPRET_RUNTIME_ERROR;
                }
                frame = &vm->frames[vm->frameCount - 1];
                break;
            }
            case OP_SUPER_INVOKE: {
                ObjString* method = READ_STRING();
                int argCount = READ_BYTE();
                ObjClass* superclass = AS_CLASS(POP());
                if (!invokeFromClass(vm, superclass, method, argCount)) {
                    return INTERPRET_RUNTIME_ERROR;
                }
                frame = &vm->frames[vm->frameCount - 1];
                break;
            }
            case OP_CLOSURE: {
                ObjFunction* function = AS_FUNCTION(READ_CONSTANT());
                ObjClosure* closure = newClosure(vm, function);
                PUSH(OBJ_VAL(closure));
                for (int i = 0; i < closure->upvalueCount; i++) {
                    uint8_t isLocal = READ_BYTE();
                    uint8_t index = READ_BYTE();
                    if (isLocal) {
                        closure->upvalues[i] = captureUpvalue(vm, frame->slots + index);
                    } else {
                        closure->upvalues[i] = frame->closure->upvalues[index];
                    }
                }
                break;
            }
            case OP_CLOSE_UPVALUE:
                closeUpvalues(vm, vm->stackTop - 1);
                POP();
                break;
            case OP_RETURN: {
                Value result = POP();
                closeUpvalues(vm, frame->slots);
                vm->frameCount--;
                if (vm->frameCount == 0) {
                    POP();
                    return INTERPRET_OK;
                }

                vm->stackTop = frame->slots;
                PUSH(result);
                frame = &vm->frames[vm->frameCount - 1];
                break;
            }
            case OP_CLASS:
                PUSH(OBJ_VAL(newClass(vm, READ_STRING())));
                break;
            case OP_INHERIT: {
                Value superclass = PEEK(1);
                if (!IS_CLASS(superclass)) {
                    runtimeError(vm, "Superclass must be a class.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                ObjClass* subclass = AS_CLASS(PEEK(0));
                tableAddAll(vm, &AS_CLASS(superclass)->methods, &subclass->methods);
                POP();
                break;
            }
            case OP_METHOD:
                defineMethod(vm, READ_STRING());
                break;
        }
        if (vm->kill) return INTERPRET_RUNTIME_ERROR;
    }

#undef READ_BYTE
#undef READ_SHORT
#undef READ_CONSTANT
#undef READ_STRING
#undef BINARY_OP
}

InterpretResult interpret(VM* vm, const char* source) {
    ObjFunction* function = compile(vm, source);
    if (function == NULL) return INTERPRET_COMPILE_ERROR;

    PUSH(OBJ_VAL(function));
    ObjClosure* closure = newClosure(vm, function);
    POP();
    PUSH(OBJ_VAL(closure));
    callClosure(vm, closure, 0);

    return run(vm);
}