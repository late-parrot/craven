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

#include "chunk.h"
#include "memory.h"

#define PUSH(value) \
    do { \
        if (!push(vm, value)) fatalError(vm, "Cannot push value."); \
    } while (false)

#define POP() pop(vm)

#define PEEK(amount) peek(vm, amount)

void initChunk(Chunk* chunk) {
    chunk->count = 0;
    chunk->capacity = 0;
    chunk->code = NULL;
    chunk->lines = NULL;
    initValueArray(&chunk->constants);
}

void writeChunk(VM* vm, Chunk *chunk, uint8_t byte, int line) {
    if (chunk->capacity < chunk->count+1) {
        int oldCapacity = chunk->capacity;
        chunk->capacity = GROW_CAPACITY(oldCapacity);
        chunk->code = GROW_ARRAY(uint8_t, chunk->code, oldCapacity, chunk->capacity);
        chunk->lines = GROW_ARRAY(int, chunk->lines, oldCapacity, chunk->capacity);
    }
    chunk->code[chunk->count] = byte;
    chunk->lines[chunk->count] = line;
    chunk->count++;
}

int addConstant(VM* vm, Chunk *chunk, Value value) {
    PUSH(value);
    writeValueArray(vm, &chunk->constants, value);
    POP();
    return chunk->constants.count - 1;
}

void freeChunk(VM* vm, Chunk* chunk) {
    FREE_ARRAY(uint8_t, chunk->code, chunk->capacity);
    FREE_ARRAY(int, chunk->lines, chunk->capacity);
    freeValueArray(vm, &chunk->constants);
    initChunk(chunk);
}