#ifndef clox_debug_h
#define clox_debug_h

#include "chunk.h"

/**
 * Print out the code inside of a chunk, along with each instruction's parameters
 * and line information.
 * 
 * :param chunk: The chunk to disassemble
 * :param name: The name to give the chunk in the dump -- multiple chunks can get
 *  confusing without meaningful headers
 */
void disassembleChunk(Chunk* chunk, const char* name);

/**
 * Print a single instruction, along wil parameters and line information.
 * 
 * :param chunk: The chunk containing the instruction
 * :param offset: The offset of the instruction in the chunk's code array
 * :returns: The offset of the *next* instruction in the chunk, useful for printing
 *  several instructions in sequence
 */
int disassembleInstruction(Chunk* chunk, int offset);

#endif