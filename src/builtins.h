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

#ifndef craven_builtins_h
#define craven_builtins_h

#include "common.h"
#include "table.h"

typedef struct VM VM;

typedef struct {
    Table stringMembers;
    Table listMembers;
    Table dictMembers;
    Table optionMembers;
} Builtins;

void initBuiltins(Builtins* builtins);
void createBuiltins(VM* vm, Builtins* builtins);
void markBuiltins(VM* vm, Builtins* builtins);
void freeBuiltins(VM* vm, Builtins* builtins);

#endif