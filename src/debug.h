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

#ifndef craven_debug_h
#define craven_debug_h

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