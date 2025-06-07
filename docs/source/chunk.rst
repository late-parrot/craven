Chunks and Bytecode
===================

A :c:struct:`Chunk` stores bytecode and a constant table, and is walked by the
VM to run a program. Bytecode is significantly faster than other language
techniques, because it stores everything in a linear fashion in memory, optimizing
our use of the CPU cache.

Opcodes
-------

.. c:autoenum:: OpCode
    :file: chunk.h
    :members:

Working with Chunks
-------------------

The following can be included from ``chunk.h`` and used to work with
:c:struct:`Chunk` objects.

.. c:autostruct:: Chunk
    :members:

.. c:autofunction:: initChunk

.. c:autofunction:: writeChunk

.. c:autofunction:: addConstant

.. c:autofunction:: freeChunk