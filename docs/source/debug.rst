Debugging and Flags
===================

Most of this won't be touched by users, but is invaluable for language implementers,
because of the valuable information that it can give about program execution,
compiling, garbage collection, or other important information.

Flags
-----

These flags can be used to help debug the language implementation, providing
useful information about execution and compiling.

.. c:autodoc:: common.h

Disassembly
-----------

An *assembler* takes human-readable lists of instructions and converts them to
bytecode or machine code. A *disassembler* does the opposite -- it turns the 
unreadable strings of bits that make up the chunks into human-readable instructions
for debugging purposes.

.. c:autodoc:: debug.h