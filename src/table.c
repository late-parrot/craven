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
#include <string.h>

#include "memory.h"
#include "object.h"
#include "table.h"
#include "value.h"
#include "vm_utils.h"

#define TABLE_MAX_LOAD 0.75

void initTable(Table* table) {
    table->count = 0;
    table->capacity = 0;
    table->entries = NULL;
}

void freeTable(VM* vm, Table* table) {
    FREE_ARRAY(Entry, table->entries, table->capacity);
    initTable(table);
}

static uint32_t hashValue(VM* vm, Value value) {
    if (IS_NIL(value) || IS_EMPTY(value)) return 0;
    else if (IS_BOOL(value)) return AS_BOOL(value);
    else if (IS_NUMBER(value)) return AS_NUMBER(value);
    else if (IS_OBJ(value)) switch (OBJ_TYPE(value)) {
        case OBJ_STRING: return AS_STRING(value)->hash;
        default:
            fatalError(vm, "Unhashable type.");
            return 0;
    }
}

static Entry* findEntry(VM* vm, Entry* entries, int capacity, Value key) {
    uint32_t index = hashValue(vm, key) & (capacity - 1);
    Entry* tombstone = NULL;
    for (;;) {
        Entry* entry = &entries[index];
        if (IS_EMPTY(entry->key)) {
            if (IS_NIL(entry->value)) {
                return tombstone != NULL ? tombstone : entry;
            } else {
                if (tombstone == NULL) tombstone = entry;
            }
        } else if (valuesEqual(entry->key, key)) {
            return entry;
        }
        index = (index + 1) & (capacity - 1);
    }
}

bool tableGet(VM* vm, Table* table, Value key, Value* value) {
    if (table->count == 0) return false;

    Entry* entry = findEntry(vm, table->entries, table->capacity, key);
    if (IS_EMPTY(entry->key)) return false;

    *value = entry->value;
    return true;
}

static void adjustCapacity(VM* vm, Table* table, int capacity) {
    Entry* entries = ALLOCATE(Entry, capacity);
    for (int i = 0; i < capacity; i++) {
        entries[i].key = EMPTY_VAL;
        entries[i].value = NIL_VAL;
    }

    table->count = 0;
    for (int i = 0; i < table->capacity; i++) {
        Entry* entry = &table->entries[i];
        if (IS_EMPTY(entry->key)) continue;

        Entry* dest = findEntry(vm, entries, capacity, entry->key);
        dest->key = entry->key;
        dest->value = entry->value;
        table->count++;
    }

    table->entries = entries;
    table->capacity = capacity;
}

bool tableSet(VM* vm, Table* table, Value key, Value value) {
    if (table->count + 1 > table->capacity * TABLE_MAX_LOAD) {
        int capacity = GROW_CAPACITY(table->capacity);
        adjustCapacity(vm, table, capacity);
    }

    Entry* entry = findEntry(vm, table->entries, table->capacity, key);
    bool isNewKey = IS_EMPTY(entry->key);
    if (isNewKey && IS_NIL(entry->value)) table->count++;

    entry->key = key;
    entry->value = value;
    return isNewKey;
}

bool tableDelete(VM* vm, Table* table, Value key) {
    if (table->count == 0) return false;

    Entry* entry = findEntry(vm, table->entries, table->capacity, key);
    if (IS_EMPTY(entry->key)) return false;

    // Place a tombstone in the entry.
    // Tombstone here is empty: true, but could really be anything,
    // so long as it can't be confused with a real value.
    entry->key = EMPTY_VAL;
    entry->value = BOOL_VAL(true);
    return true;
}

void tableAddAll(VM* vm, Table* from, Table* to) {
    for (int i = 0; i < from->capacity; i++) {
        Entry* entry = &from->entries[i];
        if (!IS_EMPTY(entry->key)) {
            tableSet(vm, to, entry->key, entry->value);
        }
    }
}

ObjString* tableFindString(Table* table, const char* chars,
    int length, uint32_t hash) {

    if (table->count == 0) return NULL;

    uint32_t index = hash & (table->capacity - 1);
    for (;;) {
        Entry* entry = &table->entries[index];
        if (IS_EMPTY(entry->key)) {
            if (IS_NIL(entry->value)) return NULL;
        } else {
            ObjString* string = AS_STRING(entry->key);
            if (string->length == length && string->hash == hash &&
                memcmp(string->chars, chars, length) == 0) {
                return string;
            }
        }
        index = (index + 1) & (table->capacity - 1);
    }
}

void tableRemoveWhite(VM* vm, Table* table) {
    for (int i = 0; i < table->capacity; i++) {
        Entry* entry = &table->entries[i];
        if (!IS_EMPTY(entry->key) && IS_OBJ(entry->key) &&
            !AS_OBJ(entry->key)->isMarked) {
            tableDelete(vm, table, entry->key);
        }
    }
}

void markTable(VM* vm, Table* table) {
    for (int i = 0; i < table->capacity; i++) {
        Entry* entry = &table->entries[i];
        markValue(vm, entry->key);
        markValue(vm, entry->value);
    }
}