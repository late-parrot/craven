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
#include "vm.h"

#define PUSH(value) \
    do { \
        if (!push(value)) return INTERPRET_RUNTIME_ERROR; \
    } while (false)

VM vm; 

static void resetStack() {
    vm.stackTop = vm.stack;
    vm.frameCount = 0;
    vm.openUpvalues = NULL;
}

static void runtimeError(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputs("\n", stderr);

    for (int i = vm.frameCount - 1; i >= 0; i--) {
        CallFrame* frame = &vm.frames[i];
        ObjFunction* function = frame->closure->function;
        size_t instruction = frame->ip - function->chunk.code - 1;
        fprintf(stderr, "[line %d] in ", function->chunk.lines[instruction]);
        if (function->name == NULL) {
            fprintf(stderr, "script\n");
        } else {
            fprintf(stderr, "%s()\n", function->name->chars);
        }
    }

    resetStack();
}

static bool clockNative(int argCount, Value* args) {
    PUSH(NUMBER_VAL((double)clock() / CLOCKS_PER_SEC));
    if (argCount != 0) runtimeError("0 args expected but got %d.", argCount);
    return argCount == 0;
}

static bool listAppendNative(int argCount, Value* args) {
    if (argCount != 1) {
        runtimeError("1 arg expected but got %d.", argCount);
        return false;
    }
    writeValueArray(&AS_LIST(args[-1])->values, args[0]);
    PUSH(args[0]);
    return true;
}

static bool defineNative(const char* name, NativeFn function) {
    PUSH(OBJ_VAL(copyString(name, (int)strlen(name))));
    PUSH(OBJ_VAL(newNative(function)));
    tableSet(&vm.globals, AS_STRING(vm.stack[0]), vm.stack[1]);
    pop();
    pop();
    return true;
}

void initVM() {
    resetStack();
    vm.objects = NULL;
    vm.bytesAllocated = 0;
    vm.nextGC = 1024 * 1024;

    int grayCount;
    int grayCapacity;
    Obj** grayStack;

    initTable(&vm.globals);
    initTable(&vm.strings);

    vm.initString = NULL;
    vm.initString = copyString("init", 4);
    // NOTE: Couldn't technically overflow the stack, so no need to handle overflow.
    defineNative("clock", clockNative);
}

void freeVM() {
    freeTable(&vm.globals);
    freeTable(&vm.strings);
    freeObjects();
}

bool push(Value value) {
    if (vm.stackTop - vm.stack >= STACK_MAX) {
        runtimeError("Stack overflow.");
        return false;
    }
    *vm.stackTop = value;
    vm.stackTop++;
    return true;
}

Value pop() {
    vm.stackTop--;
    return *vm.stackTop;
}

static Value peek(int distance) {
    return vm.stackTop[-1 - distance];
}

static bool call(ObjClosure* closure, int argCount) {
    if (argCount != closure->function->arity) {
        runtimeError(
            "Expected %d arguments but got %d.",
            closure->function->arity, argCount
        );
        return false;
    }

    if (vm.frameCount == FRAMES_MAX) {
        runtimeError("Stack overflow.");
        return false;
    }

    CallFrame* frame = &vm.frames[vm.frameCount++];
    frame->closure = closure;
    frame->ip = closure->function->chunk.code;
    frame->slots = vm.stackTop - argCount - 1;
    return true;
}

static bool callValue(Value callee, int argCount) {
    if (IS_OBJ(callee)) {
        switch (OBJ_TYPE(callee)) {
            case OBJ_BOUND_METHOD: {
                ObjBoundMethod* bound = AS_BOUND_METHOD(callee);
                vm.stackTop[-argCount - 1] = bound->receiver;
                return call(bound->method, argCount);
            }
            case OBJ_BOUND_NATIVE: {
                ObjBoundNative* bound = AS_BOUND_NATIVE(callee);
                vm.stackTop[-argCount - 1] = bound->receiver;
                if (!bound->method(argCount, vm.stackTop - argCount)) return false;
                Value result = pop();
                vm.stackTop -= argCount + 1;
                PUSH(result);
                return true;
            }
            case OBJ_CLASS: {
                ObjClass* klass = AS_CLASS(callee);
                vm.stackTop[-argCount - 1] = OBJ_VAL(newInstance(klass));
                Value initializer;
                if (tableGet(&klass->methods, vm.initString, &initializer)) {
                    return call(AS_CLOSURE(initializer), argCount);
                } else if (argCount != 0) {
                    runtimeError("Expected 0 arguments but got %d.", argCount);
                    return false;
                }
                return true;
            }
            case OBJ_CLOSURE: 
                return call(AS_CLOSURE(callee), argCount);
            case OBJ_NATIVE: {
                NativeFn native = AS_NATIVE(callee);
                if (!native(argCount, vm.stackTop - argCount)) return false;
                Value result = pop();
                vm.stackTop -= argCount + 1;
                PUSH(result);
                return true;
            }
            default:
                break; // Non-callable object type.
        }
    }
    runtimeError("Can only call functions and classes.");
    return false;
}

static bool getIndex(Value object, Value index) {
    switch (OBJ_TYPE(object)) {
        case OBJ_LIST: {
            ObjList* list = AS_LIST(object);
            double idx;
            if (!IS_NUMBER(index) || (idx = AS_NUMBER(index)) != floor(idx)) {
                runtimeError("List index must be a whole number.");
                return false;
            }
            if (idx < 0 || idx >= list->values.count) {
                runtimeError("List index out of bounds.");
                return false;
            }
            PUSH(list->values.values[(int)idx]);
            return true;
        }
        case OBJ_STRING: {
            ObjString* string = AS_STRING(object);
            double idx;
            if (!IS_NUMBER(index) || (idx = AS_NUMBER(index)) != floor(idx)) {
                runtimeError("String index must be a whole number.");
                return false;
            }
            if (idx < 0 || idx >= string->length) {
                runtimeError("String index out of bounds.");
                return false;
            }
            PUSH(OBJ_VAL(copyString(&string->chars[(int)idx], 1)));
            return true;
        }
        default:
            break;
    }
    runtimeError("Can only index lists and strings.");
    return false;
}

static bool rawGetIndex(Value object, int index) {
    switch (OBJ_TYPE(object)) {
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
            PUSH(OBJ_VAL(copyString(&string->chars[index], 1)));
            return true;
        }
        default:
            break;
    }
    return false;
}

static bool setIndex(Value object, Value index, Value value) {
    switch (OBJ_TYPE(object)) {
        case OBJ_LIST: {
            ObjList* list = AS_LIST(object);
            double idx;
            if (!IS_NUMBER(index) || (idx = AS_NUMBER(index)) != floor(idx)) {
                runtimeError("List index must be a whole number.");
                return false;
            }
            if (idx < 0 || idx >= list->values.count) {
                runtimeError("List index out of bounds.");
                return false;
            }
            list->values.values[(int)idx] = value;
            return true;
        }
        case OBJ_STRING: {
            runtimeError("Cannot assign to string indexes.");
            return false;
        }
        default:
            break;
    }
    runtimeError("Can only index lists and strings.");
    return false;
}

static bool invokeFromClass(ObjClass* klass, ObjString* name, int argCount) {
    Value method;
    if (!tableGet(&klass->methods, name, &method)) {
        runtimeError("Undefined property '%s'.", name->chars);
        return false;
    }
    return call(AS_CLOSURE(method), argCount);
}

static bool invoke(ObjString* name, int argCount) {
    Value receiver = peek(argCount);
    switch (OBJ_TYPE(receiver)) {
        case OBJ_INSTANCE:
            ObjInstance* instance = AS_INSTANCE(receiver);
            Value value;
            if (tableGet(&instance->fields, name, &value)) {
                vm.stackTop[-argCount - 1] = value;
                return callValue(value, argCount);
            }
            return invokeFromClass(instance->klass, name, argCount);
        case OBJ_STRING:
            runtimeError("Undefined method '%s'.", name->chars);
            return false;
        case OBJ_LIST:
            if (strcmp(name->chars, "append") == 0) {
                callValue(OBJ_VAL(newBoundNative(receiver, listAppendNative)), argCount);
                return true;
            }
            runtimeError("Undefined method '%s'.", name->chars);
            return false;
        default:
            break;
    }
    runtimeError("Value has no properties.");
    return false;
}

static bool bindMethod(ObjClass* klass, ObjString* name) {
    Value method;
    if (!tableGet(&klass->methods, name, &method)) {
        runtimeError("Undefined property '%s'.", name->chars);
        return false;
    }

    ObjBoundMethod* bound = newBoundMethod(peek(0), AS_CLOSURE(method));
    pop();
    PUSH(OBJ_VAL(bound));
    return true;
}

static bool getProperty(Value obj, ObjString* name) {
    if (IS_OBJ(obj)) switch (OBJ_TYPE(obj)) {
        case OBJ_INSTANCE:
            ObjInstance* instance = AS_INSTANCE(obj);

            Value property;
            if (tableGet(&instance->fields, name, &property)) {
                pop();
                PUSH(property);
                return true;
            }

            if (!bindMethod(instance->klass, name)) {
                return false;
            }
            return true;
        case OBJ_STRING:
            if (strcmp(name->chars, "length") == 0) {
                pop();
                PUSH(NUMBER_VAL(AS_STRING(obj)->length));
                return true;
            }
            runtimeError("Undefined property '%s'.", name->chars);
            return false;
        case OBJ_LIST:
            if (strcmp(name->chars, "length") == 0) {
                pop();
                PUSH(NUMBER_VAL(AS_LIST(obj)->values.count));
                return true;
            }
            if (strcmp(name->chars, "append") == 0) {
                pop();
                PUSH(OBJ_VAL(newBoundNative(obj, listAppendNative)));
                return true;
            }
            runtimeError("Undefined property '%s'.", name->chars);
            return false;
        default:
            break;
    }
    runtimeError("Value has no properties.");
    return false;
}

static bool setProperty(Value obj, ObjString* name, Value value) {
    if (IS_OBJ(obj)) switch (OBJ_TYPE(obj)) {
        case OBJ_INSTANCE:
            ObjInstance* instance = AS_INSTANCE(obj);
            Value property;
            tableSet(&instance->fields, name, value);
            pop(); // Value
            pop(); // Object
            PUSH(value);
            return true;
        default:
            break;
    }
    runtimeError("Value has no fields.");
    return false;
}

static ObjUpvalue* captureUpvalue(Value* local) {
    ObjUpvalue* prevUpvalue = NULL;
    ObjUpvalue* upvalue = vm.openUpvalues;
    while (upvalue != NULL && upvalue->location > local) {
        prevUpvalue = upvalue;
        upvalue = upvalue->next;
    }

    if (upvalue != NULL && upvalue->location == local) {
        return upvalue;
    }

    ObjUpvalue* createdUpvalue = newUpvalue(local);
    createdUpvalue->next = upvalue;

    if (prevUpvalue == NULL) {
        vm.openUpvalues = createdUpvalue;
    } else {
        prevUpvalue->next = createdUpvalue;
    }

    return createdUpvalue;
}

static void closeUpvalues(Value* last) {
    while (vm.openUpvalues != NULL && vm.openUpvalues->location >= last) {
        ObjUpvalue* upvalue = vm.openUpvalues;
        upvalue->closed = *upvalue->location;
        upvalue->location = &upvalue->closed;
        vm.openUpvalues = upvalue->next;
    }
}

static void defineMethod(ObjString* name) {
    Value method = peek(0);
    ObjClass* klass = AS_CLASS(peek(1));
    tableSet(&klass->methods, name, method);
    pop();
}

static bool isFalsey(Value value) {
    return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value))
        || (IS_NUMBER(value) && AS_NUMBER(value) == 0);
}

static bool concatenate() {
    ObjString* b = AS_STRING(peek(0));
    ObjString* a = AS_STRING(peek(1));

    int length = a->length + b->length;
    char* chars = ALLOCATE(char, length + 1);
    memcpy(chars, a->chars, a->length);
    memcpy(chars + a->length, b->chars, b->length);
    chars[length] = '\0';

    ObjString* result = takeString(chars, length);
    pop();
    pop();
    PUSH(OBJ_VAL(result));
    return true;
}

static InterpretResult run() {
    CallFrame* frame = &vm.frames[vm.frameCount - 1];

#define READ_BYTE() (*frame->ip++)
#define READ_SHORT() \
    (frame->ip += 2, (uint16_t)((frame->ip[-2] << 8) | frame->ip[-1]))
#define READ_CONSTANT() \
    (frame->closure->function->chunk.constants.values[READ_BYTE()])
#define READ_STRING() AS_STRING(READ_CONSTANT())
#define BINARY_OP(valueType, op) \
    do { \
        if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) { \
            runtimeError("Operands must be numbers."); \
            return INTERPRET_RUNTIME_ERROR; \
        } \
        double b = AS_NUMBER(pop()); \
        double a = AS_NUMBER(pop()); \
        PUSH(valueType(a op b)); \
    } while (false)

    for (;;) {
#ifdef DEBUG_TRACE_EXECUTION
        printf("          ");
        for (Value* slot = vm.stack; slot < vm.stackTop; slot++) {
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
                ObjList* list = newList();
                for (Value* v = vm.stackTop - elemCount; v < vm.stackTop; v++)
                    writeValueArray(&list->values, *v);
                vm.stackTop -= elemCount;
                PUSH(OBJ_VAL(list));
                break;
            }
            case OP_POP: pop(); break;
            case OP_GET_LOCAL: {
                uint8_t slot = READ_BYTE();
                PUSH(frame->slots[slot]); 
                break;
            }
            case OP_SET_LOCAL: {
                uint8_t slot = READ_BYTE();
                frame->slots[slot] = peek(0);
                break;
            }
            case OP_GET_GLOBAL: {
                ObjString* name = READ_STRING();
                Value value;
                if (!tableGet(&vm.globals, name, &value)) {
                    runtimeError("Undefined variable '%s'.", name->chars);
                    return INTERPRET_RUNTIME_ERROR;
                }
                PUSH(value);
                break;
            }
            case OP_DEFINE_GLOBAL: {
                ObjString* name = READ_STRING();
                tableSet(&vm.globals, name, pop());
                break;
            }
            case OP_SET_GLOBAL: {
                ObjString* name = READ_STRING();
                if (tableSet(&vm.globals, name, peek(0))) {
                    tableDelete(&vm.globals, name);
                    runtimeError("Undefined variable '%s'.", name->chars);
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
                *frame->closure->upvalues[slot]->location = peek(0);
                break;
            }
            case OP_GET_PROPERTY: {
                if (!getProperty(peek(0), READ_STRING())) {
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_SET_PROPERTY: {
                if (!setProperty(peek(1), READ_STRING(), peek(0))) {
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_GET_SUPER: {
                ObjString* name = READ_STRING();
                ObjClass* superclass = AS_CLASS(pop());

                if (!bindMethod(superclass, name)) {
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_GET_INDEX: {
                Value index = pop();
                Value object = pop();
                if (!getIndex(object, index)) {
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_SET_INDEX: {
                Value value = pop();
                Value index = pop();
                Value object = pop();
                PUSH(value);
                if (!setIndex(object, index, value)) {
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_EQUAL: {
                Value b = pop();
                Value a = pop();
                PUSH(BOOL_VAL(valuesEqual(a, b)));
                break;
            }
            case OP_GREATER:  BINARY_OP(BOOL_VAL, >); break;
            case OP_LESS:     BINARY_OP(BOOL_VAL, <); break;
            case OP_ADD: {
                if (IS_STRING(peek(0)) && IS_STRING(peek(1))) {
                    if (!concatenate()) return INTERPRET_RUNTIME_ERROR;
                } else if (IS_NUMBER(peek(0)) && IS_NUMBER(peek(1))) {
                    double b = AS_NUMBER(pop());
                    double a = AS_NUMBER(pop());
                    PUSH(NUMBER_VAL(a + b));
                } else {
                    runtimeError("Operands must be two numbers or two strings.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_SUBTRACT: BINARY_OP(NUMBER_VAL, -); break;
            case OP_MULTIPLY: BINARY_OP(NUMBER_VAL, *); break;
            case OP_DIVIDE:   BINARY_OP(NUMBER_VAL, /); break;
            case OP_NOT:
                PUSH(BOOL_VAL(isFalsey(pop())));
                break;
            case OP_NEGATE: {
                if (!IS_NUMBER(peek(0))) {
                    runtimeError("Operand must be a number.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                PUSH(NUMBER_VAL(-AS_NUMBER(pop())));
                break;
            }
            case OP_PRINT: {
                printValue(peek(0));
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
                if (isFalsey(peek(0))) frame->ip += offset;
                break;
            }
            case OP_NEXT_JUMP: {
                uint16_t offset = READ_SHORT();
                int index = (int)AS_NUMBER(pop());
                Value iter = peek(0);
                if (!IS_OBJ(iter) || OBJ_TYPE(iter) != OBJ_LIST &&
                    OBJ_TYPE(iter) != OBJ_STRING) {
                    runtimeError("Can only iterate list or string.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                PUSH(NUMBER_VAL(index+1));
                if (!rawGetIndex(iter, index)) frame->ip += offset;
                break;
            }
            case OP_LOOP: {
                uint16_t offset = READ_SHORT();
                frame->ip -= offset;
                break;
            }
            case OP_CALL: {
                int argCount = READ_BYTE();
                if (!callValue(peek(argCount), argCount)) {
                    return INTERPRET_RUNTIME_ERROR;
                }
                frame = &vm.frames[vm.frameCount - 1];
                break;
            }
            case OP_INVOKE: {
                ObjString* method = READ_STRING();
                int argCount = READ_BYTE();
                if (!invoke(method, argCount)) {
                    return INTERPRET_RUNTIME_ERROR;
                }
                frame = &vm.frames[vm.frameCount - 1];
                break;
            }
            case OP_SUPER_INVOKE: {
                ObjString* method = READ_STRING();
                int argCount = READ_BYTE();
                ObjClass* superclass = AS_CLASS(pop());
                if (!invokeFromClass(superclass, method, argCount)) {
                    return INTERPRET_RUNTIME_ERROR;
                }
                frame = &vm.frames[vm.frameCount - 1];
                break;
            }
            case OP_CLOSURE: {
                ObjFunction* function = AS_FUNCTION(READ_CONSTANT());
                ObjClosure* closure = newClosure(function);
                PUSH(OBJ_VAL(closure));
                for (int i = 0; i < closure->upvalueCount; i++) {
                    uint8_t isLocal = READ_BYTE();
                    uint8_t index = READ_BYTE();
                    if (isLocal) {
                        closure->upvalues[i] = captureUpvalue(frame->slots + index);
                    } else {
                        closure->upvalues[i] = frame->closure->upvalues[index];
                    }
                }
                break;
            }
            case OP_CLOSE_UPVALUE:
                closeUpvalues(vm.stackTop - 1);
                pop();
                break;
            case OP_RETURN: {
                Value result = pop();
                closeUpvalues(frame->slots);
                vm.frameCount--;
                if (vm.frameCount == 0) {
                    pop();
                    return INTERPRET_OK;
                }

                vm.stackTop = frame->slots;
                PUSH(result);
                frame = &vm.frames[vm.frameCount - 1];
                break;
            }
            case OP_CLASS:
                PUSH(OBJ_VAL(newClass(READ_STRING())));
                break;
            case OP_INHERIT: {
                Value superclass = peek(1);
                if (!IS_CLASS(superclass)) {
                    runtimeError("Superclass must be a class.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                ObjClass* subclass = AS_CLASS(peek(0));
                tableAddAll(&AS_CLASS(superclass)->methods, &subclass->methods);
                pop();
                break;
            }
            case OP_METHOD:
                defineMethod(READ_STRING());
                break;
        }
    }

#undef READ_BYTE
#undef READ_SHORT
#undef READ_CONSTANT
#undef READ_STRING
#undef BINARY_OP
}

InterpretResult interpret(const char* source) {
    ObjFunction* function = compile(source);
    if (function == NULL) return INTERPRET_COMPILE_ERROR;

    PUSH(OBJ_VAL(function));
    ObjClosure* closure = newClosure(function);
    pop();
    PUSH(OBJ_VAL(closure));
    call(closure, 0);

    return run();
}