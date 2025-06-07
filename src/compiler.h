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