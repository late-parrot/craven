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

#ifndef craven_object_h
#define craven_object_h

#include "common.h"
#include "chunk.h"
#include "table.h"
#include "value.h"
#include "vm_utils.h"

/** Retrieve an object's type tag in order to identify it. */
#define OBJ_TYPE(value)        (AS_OBJ(value)->type)

/** Check to see if the value is an :c:struct:`ObjBoundMethod`. */
#define IS_BOUND_METHOD(value) isObjType(value, OBJ_BOUND_METHOD)

/** Check to see if the value is an :c:struct:`ObjBoundMethod`. */
#define IS_BOUND_NATIVE(value) isObjType(value, OBJ_BOUND_NATIVE)

/** Check to see if the value is an :c:struct:`ObjClass`. */
#define IS_CLASS(value)        isObjType(value, OBJ_CLASS)

/** Check to see if the value is an :c:struct:`ObjClosure`. */
#define IS_CLOSURE(value)      isObjType(value, OBJ_CLOSURE)

/** Check to see if the value is an :c:struct:`ObjDict`. */
#define IS_DICT(value)         isObjType(value, OBJ_DICT)

/** Check to see if the value is an :c:struct:`ObjFunction`. */
#define IS_FUNCTION(value)     isObjType(value, OBJ_FUNCTION)

/** Check to see if the value is an :c:struct:`ObjInstance`. */
#define IS_INSTANCE(value)     isObjType(value, OBJ_INSTANCE)

/** Check to see if the value is an :c:struct:`ObjList`. */
#define IS_LIST(value)         isObjType(value, OBJ_LIST)

/** Check to see if the value is an :c:struct:`ObjNative`. */
#define IS_NATIVE(value)       isObjType(value, OBJ_NATIVE)

/** Check to see if the value is an :c:struct:`ObjString`. */
#define IS_STRING(value)       isObjType(value, OBJ_STRING)

/**
 * Cast the value to an :c:expr:`ObjBoundMethod*`.
 * 
 * .. warning::
 *     Converting :c:expr:`Value` s to C objects is unsafe and must be guarded
 *     with one of the ``IS_`` macros. Otherwise, the ``AS_`` macros could
 *     reinterpret random bits of memory, causing segfaults or UB.
 * 
 *     Always, *always* guard ``AS_`` macros with ``IS_`` macros, unless you have
 *     a *really* good reason.
 */
#define AS_BOUND_METHOD(value) ((ObjBoundMethod*)AS_OBJ(value))

/**
 * Cast the value to an :c:expr:`ObjBoundNative*`.
 * 
 * .. warning::
 *     Converting :c:expr:`Value` s to C objects is unsafe and must be guarded
 *     with one of the ``IS_`` macros. Otherwise, the ``AS_`` macros could
 *     reinterpret random bits of memory, causing segfaults or UB.
 * 
 *     Always, *always* guard ``AS_`` macros with ``IS_`` macros, unless you have
 *     a *really* good reason.
 */
#define AS_BOUND_NATIVE(value) ((ObjBoundNative*)AS_OBJ(value))

/**
 * Cast the value to an :c:expr:`ObjClass*`.
 * 
 * .. warning::
 *     Converting :c:expr:`Value` s to C objects is unsafe and must be guarded
 *     with one of the ``IS_`` macros. Otherwise, the ``AS_`` macros could
 *     reinterpret random bits of memory, causing segfaults or UB.
 * 
 *     Always, *always* guard ``AS_`` macros with ``IS_`` macros, unless you have
 *     a *really* good reason.
 */
#define AS_CLASS(value)        ((ObjClass*)AS_OBJ(value))

/**
 * Cast the value to an :c:expr:`ObjClosure*`.
 * 
 * .. warning::
 *     Converting :c:expr:`Value` s to C objects is unsafe and must be guarded
 *     with one of the ``IS_`` macros. Otherwise, the ``AS_`` macros could
 *     reinterpret random bits of memory, causing segfaults or UB.
 * 
 *     Always, *always* guard ``AS_`` macros with ``IS_`` macros, unless you have
 *     a *really* good reason.
 */
#define AS_CLOSURE(value)      ((ObjClosure*)AS_OBJ(value))

/**
 * Cast the value to an :c:expr:`ObjDict*`.
 * 
 * .. warning::
 *     Converting :c:expr:`Value` s to C objects is unsafe and must be guarded
 *     with one of the ``IS_`` macros. Otherwise, the ``AS_`` macros could
 *     reinterpret random bits of memory, causing segfaults or UB.
 * 
 *     Always, *always* guard ``AS_`` macros with ``IS_`` macros, unless you have
 *     a *really* good reason.
 */
#define AS_DICT(value)         ((ObjDict*)AS_OBJ(value))

/**
 * Cast the value to an :c:expr:`ObjFunction*`.
 * 
 * .. warning::
 *     Converting :c:expr:`Value` s to C objects is unsafe and must be guarded
 *     with one of the ``IS_`` macros. Otherwise, the ``AS_`` macros could
 *     reinterpret random bits of memory, causing segfaults or UB.
 * 
 *     Always, *always* guard ``AS_`` macros with ``IS_`` macros, unless you have
 *     a *really* good reason.
 */
#define AS_FUNCTION(value)     ((ObjFunction*)AS_OBJ(value))

/**
 * Cast the value to an :c:expr:`ObjInstance*`.
 * 
 * .. warning::
 *     Converting :c:expr:`Value` s to C objects is unsafe and must be guarded
 *     with one of the ``IS_`` macros. Otherwise, the ``AS_`` macros could
 *     reinterpret random bits of memory, causing segfaults or UB.
 * 
 *     Always, *always* guard ``AS_`` macros with ``IS_`` macros, unless you have
 *     a *really* good reason.
 */
#define AS_INSTANCE(value)     ((ObjInstance*)AS_OBJ(value))

/**
 * Cast the value to an :c:expr:`ObjList*`.
 * 
 * .. warning::
 *     Converting :c:expr:`Value` s to C objects is unsafe and must be guarded
 *     with one of the ``IS_`` macros. Otherwise, the ``AS_`` macros could
 *     reinterpret random bits of memory, causing segfaults or UB.
 * 
 *     Always, *always* guard ``AS_`` macros with ``IS_`` macros, unless you have
 *     a *really* good reason.
 */
#define AS_LIST(value)     ((ObjList*)AS_OBJ(value))

/**
 * Cast the value to an :c:expr:`ObjNative*`.
 * 
 * .. warning::
 *     Converting :c:expr:`Value` s to C objects is unsafe and must be guarded
 *     with one of the ``IS_`` macros. Otherwise, the ``AS_`` macros could
 *     reinterpret random bits of memory, causing segfaults or UB.
 * 
 *     Always, *always* guard ``AS_`` macros with ``IS_`` macros, unless you have
 *     a *really* good reason.
 */
#define AS_NATIVE(value)       (((ObjNative*)AS_OBJ(value))->function)

/**
 * Cast the value to an :c:expr:`ObjString*`.
 * 
 * .. warning::
 *     Converting :c:expr:`Value` s to C objects is unsafe and must be guarded
 *     with one of the ``IS_`` macros. Otherwise, the ``AS_`` macros could
 *     reinterpret random bits of memory, causing segfaults or UB.
 * 
 *     Always, *always* guard ``AS_`` macros with ``IS_`` macros, unless you have
 *     a *really* good reason.
 */
#define AS_STRING(value)       ((ObjString*)AS_OBJ(value))

/**
 * Cast the value to an :c:expr:`ObjString*` and pull out the raw C string, to be
 * passed around like a normal string.
 * 
 * .. warning::
 *     Converting :c:expr:`Value` s to C objects is unsafe and must be guarded
 *     with one of the ``IS_`` macros. Otherwise, the ``AS_`` macros could
 *     reinterpret random bits of memory, causing segfaults or UB.
 * 
 *     Always, *always* guard ``AS_`` macros with ``IS_`` macros, unless you have
 *     a *really* good reason.
 */
#define AS_CSTRING(value)      (((ObjString*)AS_OBJ(value))->chars)

typedef enum {
    OBJ_BOUND_METHOD,
    OBJ_BOUND_NATIVE,
    OBJ_CLASS,
    OBJ_CLOSURE,
    OBJ_DICT,
    OBJ_FUNCTION,
    OBJ_INSTANCE,
    OBJ_LIST,
    OBJ_NATIVE,
    OBJ_STRING,
    OBJ_UPVALUE
} ObjType;

/**
 * Each object has one of these as its first field, allowing it to be cast to and
 * from an :c:expr:`Obj*`.
 */
typedef struct Obj {
    /** Stores the type tag, allowing the object to be identified later */
    ObjType type;
    /**
     * Stores whether or not the GC has marked this object yet.
     * 
     * See the section on :ref:`garbage collection <garbage-collection>`.
     */
    bool isMarked;
    /**
     * In order for the VM to keep track of every object in the program, we put
     * them into an intrusive linked list. Each object has a pointer to the next
     * in the list.
     */
    struct Obj* next;
} Obj;

/**
 * The raw function representation. These are never actually on the stack
 * or accessible to a user. For the most part, they're always stored inside
 * of an :c:struct:`ObjClosure`.
 */
typedef struct ObjFunction {
    /**
     * The header :c:struct:`Obj`, here to allow bookkeeping and casting to
     * :c:expr:`Obj*`.
     */
    Obj obj;
    /** How many arguments the function can accept. */
    int arity;
    /**
     * How many upvalues the compiler has decided the function closes over.
     * Really only used to pass on information to the :c:struct:`ObjClosure`
     * wrapping around the outside.
     */
    int upvalueCount;
    /** The chunk storing the function's bytecode and constant table. */
    Chunk chunk;
    /** The name of the function, for error reporting and logging. */
    ObjString* name;
} ObjFunction;

/**
 * The function sygnature for a native function callback. Native functions take in
 * the number of arguments that they are given, as well as the pointer to the first
 * one, from the stack. The callback must return a boolean, indicating if the function
 * threw an error.
 * 
 * For example:
 * 
 * .. code-block:: c
 * 
 *     static bool clockNative(int argCount, Value* args) {
 *         push(NUMBER_VAL((double)clock() / CLOCKS_PER_SEC));
 *         if (argCount != 0) runtimeError("0 args expected but got %d.", argCount);
 *         return argCount == 0;
 *     }
 */
typedef bool (*NativeFn)(VM* vm, int argCount, Value* args);

/**
 * A native function is a function in CRaven whose code is written and run with C.
 * This allows native functions to be very fast and perform low-level C operations,
 * perfect for standard library functions.
 */
typedef struct ObjNative {
    /**
     * The header :c:struct:`Obj`, here to allow bookkeeping and casting to
     * :c:expr:`Obj*`.
     */
    Obj obj;
    /** The actual pointer to a native function callback. */
    NativeFn function;
} ObjNative;

/**
 * Very similar to :c:struct:`ObjBoundMethod`, stores a method and an object
 * for it to be called on, but using a native function to allow native
 * method calls.
 */
typedef struct ObjBoundNative {
    /**
     * The header :c:struct:`Obj`, here to allow bookkeeping and casting to
     * :c:expr:`Obj*`.
     */
    Obj obj;
    /**
     * The actual native function callback, which should expect the *before*
     * its argument pointer to be the value it was bound to (i.e. ``args[-1]``).
     */
    NativeFn method;
    /** The value that this native function was bound to. */
    Value receiver;
} ObjBoundNative;

/**
 * Represents a string in the program, whether constant or created at runtime.
 * Also used internally for things like global variables and function names,
 * because it provides a nice way to store strings, and is already linked up to
 * the GC, meaning we don't have to worry as much about memory management when
 * using an ``ObjString``.
 */
typedef struct ObjString {
    /**
     * The header :c:struct:`Obj`, here to allow bookkeeping and casting to
     * :c:expr:`Obj*`.
     */
    Obj obj;
    /**
     * Strings in CRaven are always null-terminated, so the length isn't strictly
     * necessary, but it's nice to not have to walk the string whenever we want to
     * know how long it is.
     */
    int length;
    /** The actual string that we store. */
    char* chars;
    /**
     * Hashes are stored directly as part of the string and computed when the string
     * is created, to avoid having to recalculate the hash every single time we need
     * it.
     */
    uint32_t hash;
} ObjString;

/**
 * Stores a pointer to a value, ensuring it isn't freed even if it moves off of the
 * stack, so that a closure can use it later to reference a closed-over variable.
 * Once the value is closed over and removed from the stack, it is then stored in
 * :c:member:`ObjUpvalue.closed`.
 * 
 * The :c:member:`OpCode.OP_GET_UPVALUE` docs have a good description of upvalues.
 */
typedef struct ObjUpvalue {
    /**
     * The header :c:struct:`Obj`, here to allow bookkeeping and casting to
     * :c:expr:`Obj*`.
     */
    Obj obj;
    /**
     * A pointer to either somewhere on the stack or in the global table, or to this
     * own object's ``closed`` field, providing access to the value that is closed
     * over.
     */
    Value* location;
    /**
     * Once the value is closed over, it is moved here, and ``location`` is updated
     * to point at this field. This prevents the value from being freed while a
     * closure may still use it.
     */
    Value closed;
    /**
     * To Allow the VM to keep a list of all of the current upvalues and manage
     * them as needed, they are stored in an itrusive linked list.
     */
    struct ObjUpvalue* next;
} ObjUpvalue;

typedef struct ObjClosure {
    /**
     * The header :c:struct:`Obj`, here to allow bookkeeping and casting to
     * :c:expr:`Obj*`.
     */
    Obj obj;
    ObjFunction* function;
    ObjUpvalue** upvalues;
    int upvalueCount;
} ObjClosure;

typedef struct ObjClass {
    /**
     * The header :c:struct:`Obj`, here to allow bookkeeping and casting to
     * :c:expr:`Obj*`.
     */
    Obj obj;
    ObjString* name;
    Table methods;
} ObjClass;

typedef struct ObjInstance {
    /**
     * The header :c:struct:`Obj`, here to allow bookkeeping and casting to
     * :c:expr:`Obj*`.
     */
    Obj obj;
    ObjClass* klass;
    Table fields;
} ObjInstance;

/**
 * Raven's most basic collection type, a simple wrapper over a
 * :c:struct:`ValueArray`.
 */
typedef struct ObjList {
    /**
     * The header :c:struct:`Obj`, here to allow bookkeeping and casting to
     * :c:expr:`Obj*`.
     */
    Obj obj;
    /**
     * The dynamic array containing all of the values.
     */
    ValueArray values;
} ObjList;

/**
 * A simple wrapper over a :c:struct:`Table`, allowing the user
 * to interact with hash maps.
 */
typedef struct ObjDict {
    /**
     * The header :c:struct:`Obj`, here to allow bookkeeping and casting to
     * :c:expr:`Obj*`.
     */
    Obj obj;
    /**
     * The hash map containing all of the keys and values.
     */
    Table values;
} ObjDict;

typedef struct ObjBoundMethod {
    /**
     * The header :c:struct:`Obj`, here to allow bookkeeping and casting to
     * :c:expr:`Obj*`.
     */
    Obj obj;
    Value receiver;
    ObjClosure* method;
} ObjBoundMethod;

ObjBoundMethod* newBoundMethod(VM* vm, Value receiver, ObjClosure* method);
ObjBoundNative* newBoundNative(VM* vm, Value receiver, NativeFn method);
ObjClass* newClass(VM* vm, ObjString* name);
ObjClosure* newClosure(VM* vm, ObjFunction* function);
ObjDict* newDict(VM* vm);
ObjFunction* newFunction(VM* vm);
ObjInstance* newInstance(VM* vm, ObjClass* klass);
ObjList* newList(VM* vm);
ObjNative* newNative(VM* vm, NativeFn function);
ObjString* takeString(VM* vm, char* chars, int length);
ObjString* copyString(VM* vm, const char* chars, int length);
ObjUpvalue* newUpvalue(VM* vm, Value* slot);
void printObject(Value value);

static inline bool isObjType(Value value, ObjType type) {
    return IS_OBJ(value) && AS_OBJ(value)->type == type;
}

#endif