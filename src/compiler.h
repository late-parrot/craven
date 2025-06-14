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

#ifndef craven_compiler_h
#define craven_compiler_h

#include "object.h"
#include "vm.h"

/**
 * Compile a function from source and return a pointer to the
 * :c:struct:`ObjFunction` containing the bytecode.
 * 
 * :param source: The input source, either from the REPL or a source file
 * :return: A pointer to the :c:struct:`ObjFunction` containing the bytecode.
 */
ObjFunction* compile(const char* source);

/**
 * Mark anything that the compiler accesses directly and stores a pointer to,
 * preventing these objects from getting swept up by the garbage collector.
 */
void markCompilerRoots();

#endif