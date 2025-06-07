#ifndef craven_object_h
#define craven_object_h

#include "common.h"
#include "chunk.h"
#include "table.h"
#include "value.h"

/** Retrieve an object's type tag in order to identify it. */
#define OBJ_TYPE(value)        (AS_OBJ(value)->type)

/** Check to see if the value is an :c:struct:`ObjBoundMethod`. */
#define IS_BOUND_METHOD(value) isObjType(value, OBJ_BOUND_METHOD)

/** Check to see if the value is an :c:struct:`ObjClass`. */
#define IS_CLASS(value)        isObjType(value, OBJ_CLASS)

/** Check to see if the value is an :c:struct:`ObjClosure`. */
#define IS_CLOSURE(value)      isObjType(value, OBJ_CLOSURE)

/** Check to see if the value is an :c:struct:`ObjFunction`. */
#define IS_FUNCTION(value)     isObjType(value, OBJ_FUNCTION)

/** Check to see if the value is an :c:struct:`ObjInstance`. */
#define IS_INSTANCE(value)     isObjType(value, OBJ_INSTANCE)

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
    OBJ_CLASS,
    OBJ_CLOSURE,
    OBJ_FUNCTION,
    OBJ_INSTANCE,
    OBJ_NATIVE,
    OBJ_STRING,
    OBJ_UPVALUE
} ObjType;

/**
 * Each object has one of these as its first field, allowing it to be cast to and
 * from an :c:expr:`Obj*`.
 */
struct Obj {
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
};

/**
 * The raw function representation. These are never actually on the stack
 * or accessible to a user. For the most part, they're always stored inside
 * of an :c:struct:`ObjClosure`.
 */
typedef struct {
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
 * one, from the stack. The callback must return a value, which will be the result
 * of the native function.
 * 
 * For example:
 * 
 * .. code-block:: c
 * 
 *     static Value clockNative(int argCount, Value* args) {
 *         return NUMBER_VAL((double)clock() / CLOCKS_PER_SEC);
 *     }
 */
typedef Value (*NativeFn)(int argCount, Value* args);

/**
 * A native function is a function in CRaven whose code is written and run with C.
 * This allows native functions to be very fast and perform low-level C operations,
 * perfect for standard library functions.
 */
typedef struct {
    /**
     * The header :c:struct:`Obj`, here to allow bookkeeping and casting to
     * :c:expr:`Obj*`.
     */
    Obj obj;
    /** The actual pointer to a native function callback. */
    NativeFn function;
} ObjNative;

/**
 * Represents a string in the program, whether constant or created at runtime.
 * Also used internally for things like global variables and function names,
 * because it provides a nice way to store strings, and is already linked up to
 * the GC, meaning we don't have to worry as much about memory management when
 * using an ``ObjString``.
 */
struct ObjString {
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
};

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

typedef struct {
    /**
     * The header :c:struct:`Obj`, here to allow bookkeeping and casting to
     * :c:expr:`Obj*`.
     */
    Obj obj;
    ObjFunction* function;
    ObjUpvalue** upvalues;
    int upvalueCount;
} ObjClosure;

typedef struct {
    /**
     * The header :c:struct:`Obj`, here to allow bookkeeping and casting to
     * :c:expr:`Obj*`.
     */
    Obj obj;
    ObjString* name;
    Table methods;
} ObjClass;

typedef struct {
    /**
     * The header :c:struct:`Obj`, here to allow bookkeeping and casting to
     * :c:expr:`Obj*`.
     */
    Obj obj;
    ObjClass* klass;
    Table fields;
} ObjInstance;

typedef struct {
    /**
     * The header :c:struct:`Obj`, here to allow bookkeeping and casting to
     * :c:expr:`Obj*`.
     */
    Obj obj;
    Value receiver;
    ObjClosure* method;
} ObjBoundMethod;

ObjBoundMethod* newBoundMethod(Value receiver, ObjClosure* method);
ObjClass* newClass(ObjString* name);
ObjClosure* newClosure(ObjFunction* function);
ObjFunction* newFunction();
ObjInstance* newInstance(ObjClass* klass);
ObjNative* newNative(NativeFn function);
ObjString* takeString(char* chars, int length);
ObjString* copyString(const char* chars, int length);
ObjUpvalue* newUpvalue(Value* slot);
void printObject(Value value);

static inline bool isObjType(Value value, ObjType type) {
    return IS_OBJ(value) && AS_OBJ(value)->type == type;
}

#endif