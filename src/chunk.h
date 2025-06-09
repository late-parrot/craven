#ifndef craven_chunk_h
#define craven_chunk_h

#include "common.h"
#include "value.h"

/**
 * Each enum value represents a different VM instruction, and each has its own
 * parameters and special treatment. A brief description of each is provided.
 * 
 * *Stack effect* describes the effect an instruction has on the stack; if it
 * pushes values, this leads to a positive effect, if it pops values and consumes
 * them, it's a negative stack effect. To keep things consistent and not overflow
 * the stack, every statement has a stack effect of zero. This means instructions
 * like :c:member:`OpCode.OP_POP` might be needed at the end of a statement to
 * keep this promise.
 */
typedef enum {
    /** 
     * Loads a constant from the constant table of the current callframe's
     * chunk and pushes it onto the stack. The index is usually given by
     * :c:func:`addConstant`
     * 
     * :parameters:
     *  * **arrayOffset** -- byte operand
     * 
     * STACK EFFECT: +1
     */
    OP_CONSTANT,
    /**
     * Create a ``nil`` value and push it onto the stack
     * 
     * STACK EFFECT: +1
     */
    OP_NIL,
    /**
     * Create a ``true`` value and push it onto the stack
     * 
     * STACK EFFECT: +1
     */
    OP_TRUE,
    /**
     * Create a ``false`` value and push it onto the stack
     * 
     * STACK EFFECT: +1
     */
    OP_FALSE,
    /**
     * Pop ``elemCount`` values off of the stack and create a list from them.
     * Stack effect is ``1-elemCount``.
     * 
     * :parameters:
     *  * **elemCount** -- byte operand
     * 
     * STACK EFFECT: variable (see description)
     */
    OP_LIST,
    /**
     * Pop a value off the top of the stack and discard. Useful for throwing away
     * values.
     * 
     * STACK EFFECT: -1
     */
    OP_POP,
    /**
     * Retrieve a local variable and place it onto the stack, using the
     * ``localOffset`` as the offset from the start of this callframe's stack
     * window.
     * 
     * Locals are stored on the stack before temporaries because they
     * exhibit stack semantics, and this makes locals very fast.
     * 
     * :parameters:
     *  * **localOffset** -- byte operand
     * 
     * STACK EFFECT: +1
     */
    OP_GET_LOCAL,
    /**
     * Peek at the top value (assignments should result in the assigned value,
     * so we don't pop) and place it into the stack slot indicated by
     * ``localOffset``, starting from the callframe's stack window.
     * 
     * See :c:member:`OpCode.OP_GET_LOCAL` for more about local semantics.
     * 
     * :parameters:
     *  * **localOffset** -- byte operand
     * 
     * STACK EFFECT: 0
     */
    OP_SET_LOCAL,
    /**
     * Retrieve a value from :c:member:`VM.globals`, using the string pointed
     * to by ``keyConstant``, and push it onto the stack. Throws a runtime
     * error if the key cannot be found in the hash map.
     * 
     * Unlike locals, globals are simply stored in a hash map, making global
     * lookup a bit simpler, but also somewhat slower.
     * 
     * :parameters:
     *  * **keyConstant** -- constant operand
     * 
     * STACK EFFECT: +1
     */
    OP_GET_GLOBAL,
    /**
     * Pop the top of the stack and place it into :c:member:`VM.globals`, using
     * ``keyConstant`` as the key. Does not throw a runtime error if the key is
     * not present, therefore useful for variable *declarations*. Assingments
     * should use :c:member:`OpCode.OP_SET_GLOBAL`, because that will properly
     * throw an error if the key is missing.
     * 
     * See :c:member:`OpCode.OP_GET_GLOBAL` for more information on globals.
     * 
     * :parameters:
     *  * **keyConstant** -- constant operand
     * 
     * STACK EFFECT: -1
     */
    OP_DEFINE_GLOBAL,
    /**
     * Peeks at the top stack value (assignments should result in the assigned
     * value, so we don't pop), and places it into :c:member:`VM.globals`, using
     * ``keyConstant`` as the key. Throws an error if the key does not exist, so
     * this should be used for *assignments*. For declarations, the key doesn't
     * exist yet, so this would throw an error, which would be incorrect behaviour.
     * Use :c:member:`OpCode.OP_DEFINE_GLOBAL` for global definitions.
     * 
     * See :c:member:`OpCode.OP_GET_GLOBAL` for more information on globals.
     * 
     * :parameters:
     *  * **keyConstant** -- constant operand
     * 
     * STACK EFFECT: 0
     */
    OP_SET_GLOBAL,
    /**
     * Retrieve the value pointed at by the entry in the current closure's
     * :c:member:`ObjClosure.upvalues`, indexed by ``upvalueIndex``. Once the
     * entry is retrieved, the :c:member:`ObjUpvalue.location` is dereferenced
     * to access the value which is pushed onto the stack.
     * 
     * If a closure closes over a value after the parent function is exited, there
     * is no longer any reference to these values -- the closure is now unable to
     * access the values it needed to close over. Upvalues solve that problem by
     * letting a closure keep a list of every value it closes over.
     * 
     * :parameters:
     *  * **upvalueIndex** -- byte operand
     * 
     * STACK EFFECT: +1
     */
    OP_GET_UPVALUE,
    /**
     * Peeks at the top stack value (assignments should result in the assigned
     * value, so we don't pop), placing it into the current closure's
     * :c:member:`ObjClosure.upvalues`, indexed by ``upvalueIndex``. The
     * :c:member:`ObjUpvalue.location` field is overridden to point to the value
     * on top of the stack.
     * 
     * See :c:member:`OpCode.OP_GET_UPVALUE` for more about upvalues.
     * 
     * :parameters:
     *  * **upvalueIndex** -- byte operand
     * 
     * STACK EFFECT: 0
     */
    OP_SET_UPVALUE,
    /**
     * Look at the ``ObjInstance`` at the top of the stack (throws an error if the
     * top value is not an ``ObjInstance``), then retrieve a value from
     * :c:member:`ObjInstance.fields`, or the classe's method list if this lookup
     * fails. Uses the string pointed to by ``keyConstant`` as the key, and pushes
     * whatever value found onto the stack, binding it to the instance if it's a
     * method. Throws a runtime error if the key cannot be found in the hash map.
     * 
     * Bound methods store a pointer to the instance they were retrieved from,
     * allowing the method to properly resolve ``this`` lookups at runtime.
     * 
     * :parameters:
     *  * **keyConstant** -- constant operand
     * 
     * STACK EFFECT: +1
     */
    OP_GET_PROPERTY,
    /**
     * Look at the ``ObjInstance`` at the second slot of the stack (throws an
     * error if this value is not an ``ObjInstance``), then set a value in
     * :c:member:`ObjInstance.fields`, using the string pointed to by
     * ``keyConstant`` as the key and the top slot of the stack as the value
     * (not popping because assignment results in the assigned value). Creates
     * the field if not already present. Pops the instance out from the second
     * slot.
     * 
     * See :c:member:`OpCode.OP_GET_PROPERTY` for more about properties or
     * method binding.
     * 
     * :parameters:
     *  * **keyConstant** -- constant operand
     * 
     * STACK EFFECT: -1 (pops *second* to top value)
     */
    OP_SET_PROPERTY,
    /**
     * Assumes the class on the top of the stack (throws error if the top is
     * anything else) is the instance's superclass (instance is in second slot),
     * and retrieves and binds the method corresponding with ``keyConstant`` to
     * the instance, pushing the bound method onto the stack.
     * 
     * See :c:member:`OpCode.OP_GET_PROPERTY` for more about properties or
     * method binding.
     * 
     * :parameters:
     *  * **keyConstant** -- constant operand
     * 
     * STACK EFFECT: -1
     */
    OP_GET_SUPER,
    /**
     * Pop one value to use as the index, then pop the opject to index into. Push
     * the value at the index indicated, or throw an error if the object isn't
     * indexable or index is out of bounds, or if the index isn't a valid type.
     * 
     * STACK EFFECT: -1
     */
    OP_GET_INDEX,
    /**
     * Pop the assigned value off the top of the stack, then the index, then the
     * value. Try to perform assignment and push the assigned value back onto the
     * stack. Throws an error for unindexable object or invalid indicies.
     * 
     * STACK EFFECT: -2
     */
    OP_SET_INDEX,
    /**
     * Pop two operands and test for equality, pushing result.
     * 
     * STACK EFFECT: -1
     */
    OP_EQUAL,
    /**
     * Pop two operands and test for greater than, pushing result. Throws error
     * if not two numbers.
     * 
     * STACK EFFECT: -1
     */
    OP_GREATER,
    /**
     * Pop two operands and test for less than, pushing result. Throws error if
     * not two numbers.
     * 
     * STACK EFFECT: -1
     */
    OP_LESS,
    /**
     * Pop two operands and add for numbers, concatenate for strings, pushing
     * result. Throws error if not two strings or two numbers.
     * 
     * STACK EFFECT: -1
     */
    OP_ADD,
    /**
     * Pop two operands and find difference, pushing result. Throws error
     * if not two numbers.
     * 
     * STACK EFFECT: -1
     */
    OP_SUBTRACT,
    /**
     * Pop two operands and multiply, pushing result. Throws error
     * if not two numbers.
     * 
     * STACK EFFECT: -1
     */
    OP_MULTIPLY,
    /**
     * Pop two operands and divide, pushing result. Throws error
     * if not two numbers.
     * 
     * STACK EFFECT: -1
     */
    OP_DIVIDE,
    /**
     * Pop one operand and perform logical NOT, pushing result.
     * 
     * STACK EFFECT: 0
     */
    OP_NOT,
    /**
     * Pop one operand and negate, pushing result. Throws error
     * if not a number.
     * 
     * STACK EFFECT: 0
     */
    OP_NEGATE,
    /**
     * Pop a result and print it to ``stdout``.
     * 
     * STACK EFFECT: -1
     */
    OP_PRINT,
    /**
     * Add ``jumpAmount`` to the program's :c:member:`CallFrame.ip` counter, moving
     * the program forward. Useful for branching.
     * 
     * :parameters:
     *  * **jumpAmount** -- byte operand
     * 
     * STACK EFFECT: 0
     */
    OP_JUMP,
    /**
     * Same as :c:member:`OpCode.OP_JUMP`, but only executes if the top stack value
     * is falsey.
     * 
     * :parameters:
     *  * **jumpAmount** -- byte operand
     * 
     * STACK EFFECT: 0
     */
    OP_JUMP_IF_FALSE,
    /**
     * Same as :c:member:`OpCode.OP_JUMP`, but subtracts from the ``ip`` rather
     * than adding, useful for going back to the beginning of a loop.
     * 
     * :parameters:
     *  * **loopAmount** -- byte operand
     * 
     * STACK EFFECT: 0
     */
    OP_LOOP,
    /**
     * Call a value with ``argCount`` parameters, opening a new ``CallFrame``.
     * These stack slots will become the new functions arguments, in place to be
     * local varaibles, so they are not popped until :c:member:`OpCode.OP_RETURN`
     * is used.
     * 
     * :parameters:
     *  * **argCount** -- byte operand
     * 
     * STACK EFFECT: 0
     */
    OP_CALL,
    /**
     * A super instruction of :c:member:`OpCode.OP_GET_PROPERTY` and
     * :c:member:`OpCode.OP_CALL`. The pattern of retrieving a method and then
     * calling it is common enough to warrant a new instruction that removes the
     * bound method floating around and makes this pattern significantly faster.
     * 
     * Does handle the case of calling a function stored in a field, which should
     * not be bound.
     * 
     * :parameters:
     *  * **keyConstant** -- constant operand
     *  * **argCount** -- byte operand
     * 
     * STACK EFFECT: -1
     */
    OP_INVOKE,
    /**
     * A super instruction of :c:member:`OpCode.OP_GET_SUPER` and
     * :c:member:`OpCode.OP_CALL`. The pattern of retrieving a super method and
     * then calling it is common enough to warrant a new instruction that removes
     * the bound method floating around and makes this pattern significantly faster.
     * 
     * :parameters:
     *  * **keyConstant** -- constant operand
     *  * **argCount** -- byte operand
     * 
     * STACK EFFECT: -1
     */
    OP_SUPER_INVOKE,
    /**
     * Create an ``ObjClosure`` from ``functionConstant`` and capture any
     * upvalues, pushing the newly created closure onto the stack.
     * 
     * See :c:member:`OP_GET_UPVALUE` for an explaination of upvalues.
     * 
     * :parameters:
     *  * **functionConstant** -- constant operand
     * 
     * STACK EFFECT: +1
     */
    OP_CLOSURE,
    /**
     * Close all upvalues for the current closure, moving them into the heap
     * where the closure can still reference them.
     * 
     * See :c:member:`OP_GET_UPVALUE` for an explaination of upvalues.
     * 
     * STACK EFFECT: 0
     */
    OP_CLOSE_UPVALUE,
    /**
     * Return from the current closure, first saving the top stack value for
     * the return value, then popping any local variables associated with
     * this function. Finally places the saved return value onto the stack, ready
     * for the parent function to use.
     * 
     * STACK EFFECT: variable (see description)
     */
    OP_RETURN,
    /**
     * Create a class with the name ``nameConstant``, pushing the empty class onto
     * the stack.
     * 
     * :parameters:
     *  * **nameConstant** -- constant value
     * 
     * STACK EFFECT: +1
     */
    OP_CLASS,
    /**
     * Add all of the superclass's (second to top) methods into the subclass's
     * (top of stack) :c:member:`ObjClass.methods` field, making the class a child.
     * No real registration takes place, as the compiler knows what each class's
     * superclass is, making things like :c:member:`OpCode.OP_GET_SUPER` easy to do
     * at compile time.
     * 
     * Just copying the methods like this is okay because users
     * can't monkey patch their classes and add new methods, which would invalidate
     * subclasses. This is not possible, so it's not a problem, and the methods for
     * this class are created and added *after* this instruction runs, so will be
     * properly overrriden.
     * 
     * STACK EFFECT: -1
     */
    OP_INHERIT,
    /**
     * Add the method (top of the stack) to the class's (second to top)
     * :c:member:`ObjClass.methods` field, popping the method. This instruction
     * should only be emitted after an :c:member:`OpCode.OP_CLASS` instruction
     * or another of this instruction, because anywhere else it could be unsafe
     * or allow monkeypatching, which would break :c:member:`OpCode.OP_INHERIT`.
     * 
     * :parameters:
     *  * **keyConstant** -- constant value
     * 
     * STACK EFFECT: -1
     */
    OP_METHOD
} OpCode;

/**
 * Stores bytecode and runtime information to be passed to the VM, forming
 * the information bridge between the compiler and VM.
 */
typedef struct {
    /** The number of elements present in the dynamic array */
    int count;
    /** The number of elements the dynamic array could hold before a reallocation */
    int capacity;
    /**
     * The actual pointer to the array of opcodes/parameters, can be indexed
     * normally or traversed with pointers
     */
    uint8_t* code;
    /** The array of line numbers, indexes are parallel to :c:member:`Chunk.code` */
    int* lines;
    /**
     * The constant table for the chunk, storing values needed at runtime,
     * such as numbers, strings, or functions.
     */
    ValueArray constants;
} Chunk;

//typedef struct Chunk Chunk;

/**
 * Zero out a given ``Chunk*`` or clear any present data,
 * can also be used after freeing a chunk to zero out its memory.
 * 
 * :param chunk: a pointer to the ``Chunk`` to initialize
 */
void initChunk(Chunk* chunk);

/**
 * Write a byte and a line number to a chunk, possibly causing a
 * dynamic array allocation.
 * 
 * :param chunk: the chunk being written to
 * :param byte: the byte to place in the ``Chunk``'s ``code`` array,
 *  can be either an opcode or a parameter to an opcode
 * :param line: the line that the byte was generated on, used for runtime errors
 */
void writeChunk(Chunk* chunk, uint8_t byte, int line);

/**
 * Adds a constant to the chunk's constant table, for access at runtime. The
 * result can be passed as the operand for an :c:member:`OpCode.OP_CONSTANT`
 * instruction, indicating the offset of the constant in the chunk's table.
 * 
 * For example, to create a chunk and add an :c:member:`OpCode.OP_CONSTANT`
 * instruction for the number ``5``:
 * 
 * .. code-block:: c
 * 
 *     Chunk chunk;
 *     initChunk(&chunk);
 *     int constant = addConstant(&chunk, 5);
 *     writeChunk(&chunk, OP_CONSTANT);
 *     writeChunk(&chunk, constant);
 * 
 * :param chunk: the chunk to add a constant to
 * :param value: the value to be placed in the table. Must be known at compile
 *  time, such as a number, string, or function/closure.
 * :return: the integer offset of the value in the chunk's constant table, to be
 *  used by an ``OP_CONSTANT`` instruction in the VM
 */
int addConstant(Chunk* chunk, Value value);

/**
 * Frees a chunk pointer and zeroes out its memory using :c:func:`initChunk`.
 * 
 * :param chunk: the chunk pointer to free
 */
void freeChunk(Chunk* chunk);

#endif
