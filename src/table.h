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

#ifndef craven_table_h
#define craven_table_h

#include "common.h"

typedef uint64_t Value;
typedef struct VM VM;
typedef struct ObjString ObjString;

typedef struct {
    Value key;
    Value value;
} Entry;

typedef struct {
    int count;
    int capacity;
    Entry* entries;
} Table;

void initTable(Table* table);
void freeTable(VM* vm, Table* table);
bool tableGet(VM* vm, Table* table, Value key, Value* value);
bool tableSet(VM* vm, Table* table, Value key, Value value);
bool tableDelete(VM* vm, Table* table, Value key);
void tableAddAll(VM* vm, Table* from, Table* to);
ObjString* tableFindString(Table* table, const char* chars,
    int length, uint32_t hash);
void tableRemoveWhite(VM* vm, Table* table);
void markTable(VM* vm, Table* table);

#endif