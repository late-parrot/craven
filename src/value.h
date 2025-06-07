#ifndef clox_value_h
#define clox_value_h

#include <string.h>

#include "common.h"

typedef struct Obj Obj;
typedef struct ObjString ObjString;

#ifdef NAN_BOXING

/**
 * A mask for the bit that represents the sign in an IEEE 754
 * double-precision float.
 */
#define SIGN_BIT ((uint64_t)0x8000000000000000)

/**
 * A mask for all of the exponent bits of an IEEE 754 double-precision float,
 * plus one to avoid a special value on some chips.
 * 
 * See more about NaN boxing and quiet NaNs with the :c:macro:`NAN_BOXING` flag.
 */
#define QNAN     ((uint64_t)0x7ffc000000000000)

/**
 * A flag for NaN boxed ``nil`` values. If this and the :c:macro:`QNAN` masks are
 * both set, the value is ``nil``.
 */
#define TAG_NIL   1 // 01.

/**
 * A flag for NaN boxed ``false`` values. If this and the :c:macro:`QNAN` masks are
 * both set, the value is ``false``.
 */
#define TAG_FALSE 2 // 10.

/**
 * A flag for NaN boxed ``true`` values. If this and the :c:macro:`QNAN` masks are
 * both set, the value is ``true``.
 */
#define TAG_TRUE  3 // 11.

/**
 * With the NaN boxing representation, all values are 64 bit integers, which
 * actually represent double-precision floats. If all of the exponent bits are set,
 * the value is treated as a NaN value, and we can for the most part stuff whatever
 * we want into the remaining 51 bits of the sign bit and mantissa. This is plenty
 * of space to store any other values we need, even pointers for :c:struct:`Obj` s.
 * 
 * See more about NaN boxing and quiet NaNs with the :c:macro:`NAN_BOXING` flag.
 */
typedef uint64_t Value;

/** Check if a NaN boxed value is a boolean. */
#define IS_BOOL(value)      (((value) | 1) == TRUE_VAL)

/** Check if a NaN boxed value is ``nil``. */
#define IS_NIL(value)       ((value) == NIL_VAL)

/** Check if a NaN boxed value is a number. */
#define IS_NUMBER(value)    (((value) & QNAN) != QNAN)

/** Check if a NaN boxed value is an :c:expr:`Obj*`. */
#define IS_OBJ(value)       (((value) & (QNAN | SIGN_BIT)) == (QNAN | SIGN_BIT))

/**
 * Convert a NaN boxed value to a C boolean.
 * 
 * .. warning::
 *     Converting :c:expr:`Value` s to C objects is unsafe and must be guarded
 *     with one of the ``IS_`` macros. Otherwise, the ``AS_`` macros could
 *     reinterpret random bits of memory, causing segfaults or UB.
 * 
 *     Always, *always* guard ``AS_`` macros with ``IS_`` macros, unless you have
 *     a *really* good reason.
 */
#define AS_BOOL(value)      ((value) == TRUE_VAL)

/**
 * Convert a NaN boxed value to a C double (doubles are the only kind of number
 * in Raven, just like JavaScript).
 * 
 * .. warning::
 *     Converting :c:expr:`Value` s to C objects is unsafe and must be guarded
 *     with one of the ``IS_`` macros. Otherwise, the ``AS_`` macros could
 *     reinterpret random bits of memory, causing segfaults or UB.
 * 
 *     Always, *always* guard ``AS_`` macros with ``IS_`` macros, unless you have
 *     a *really* good reason.
 */
#define AS_NUMBER(value)    valueToNum(value)

/**
 * Convert a NaN boxed value to a C :c:expr:`Obj*` pointer.
 * 
 * .. warning::
 *     Converting :c:expr:`Value` s to C objects is unsafe and must be guarded
 *     with one of the ``IS_`` macros. Otherwise, the ``AS_`` macros could
 *     reinterpret random bits of memory, causing segfaults or UB.
 * 
 *     Always, *always* guard ``AS_`` macros with ``IS_`` macros, unless you have
 *     a *really* good reason.
 */
#define AS_OBJ(value)       ((Obj*)(uintptr_t)((value) & ~(SIGN_BIT | QNAN)))

/** Convert a C boolean into a NaN boxed value. */
#define BOOL_VAL(b)     ((b) ? TRUE_VAL : FALSE_VAL)

/** All ``false`` values are actually the same value, which is this one. */
#define FALSE_VAL       ((Value)(uint64_t)(QNAN | TAG_FALSE))

/** All ``true`` values are actually the same value, which is this one. */
#define TRUE_VAL        ((Value)(uint64_t)(QNAN | TAG_TRUE))

/** All ``nil`` values are actually the same value, which is this one. */
#define NIL_VAL         ((Value)(uint64_t)(QNAN | TAG_NIL))

/** Convert a C double into a NaN boxed value. */
#define NUMBER_VAL(num) numToValue(num)

/** Convert a C :c:expr:`Obj*` into a NaN boxed value. */
#define OBJ_VAL(obj)    (Value)(SIGN_BIT | QNAN | (uint64_t)(uintptr_t)(obj))

static inline double valueToNum(Value value) {
    double num;
    memcpy(&num, &value, sizeof(Value));
    return num;
}

static inline Value numToValue(double num) {
    Value value;
    memcpy(&value, &num, sizeof(double));
    return value;
}

#else

typedef enum {
    VAL_BOOL,
    VAL_NIL, 
    VAL_NUMBER,
    VAL_OBJ,
} ValueType;

typedef struct {
    ValueType type;
    union {
        bool boolean;
        double number;
        Obj* obj;
    } as;
} Value;

#define IS_BOOL(value)    ((value).type == VAL_BOOL)
#define IS_NIL(value)     ((value).type == VAL_NIL)
#define IS_NUMBER(value)  ((value).type == VAL_NUMBER)
#define IS_OBJ(value)     ((value).type == VAL_OBJ)

#define AS_OBJ(value)     ((value).as.obj)
#define AS_BOOL(value)    ((value).as.boolean)
#define AS_NUMBER(value)  ((value).as.number)

#define BOOL_VAL(value)   ((Value){VAL_BOOL, {.boolean = value}})
#define NIL_VAL           ((Value){VAL_NIL, {.number = 0}})
#define NUMBER_VAL(value) ((Value){VAL_NUMBER, {.number = value}})
#define OBJ_VAL(object)   ((Value){VAL_OBJ, {.obj = (Obj*)object}})

#endif

/**
 * A dynamic array of :c:expr:`Value` s. This is implemented almost exactly like
 * :c:struct:`Chunk`, so I'll skip over the details.
 */
typedef struct {
    /** The number of items the array could hold before it need a reallocation. */
    int capacity;
    /** The number of items currently in the array */
    int count;
    /**
     * A pointer to the first value, and since pointers and arrays in C are kind of
     * the same thing, also a way to access other elements of the array.
     */
    Value* values;
} ValueArray;

/**
 * Test two values for equality.
 * 
 * :param a: Any :c:expr:`Value`
 * :param b: Any :c:expr:`Value`
 * :return: ``true`` if the values are equal, ``false`` if not
 * 
 * .. note::
 *     :c:struct:`Obj` values are checked for pointer equality, so two objects
 *     might be functionally identical, but if they aren't the *same* object,
 *     then they won't be seen as equal.
 */
bool valuesEqual(Value a, Value b);

/**
 * Initialize a :c:struct:`ValueArray` or clear the values after freeing.
 * 
 * :param array: A pointer to the array to initialize
 */
void initValueArray(ValueArray* array);

/**
 * Writes a value to a :c:struct:`ValueArray`, increasing its size and possibly
 * triggering a reallocation.
 * 
 * :param array: A pointer to the array to write to
 * :param value: The value to write to the array
 */
void writeValueArray(ValueArray* array, Value value);

/**
 * Free the memory for a :c:struct:`ValueArray`, but not any of the values in it.
 * A value array actually doesn't own the objects it contains, because there may
 * be a pointer elseware.
 * 
 * :param array: A pointer to the array to free
 */
void freeValueArray(ValueArray* array);

/**
 * Print a value to stdout, without any extra formatting.
 */
void printValue(Value value);

#endif