Objects, Memory, and Garbage Collection
=======================================

CRaven has a multi-tiered approach to values and objects. Values are either a
tagged union or NaN boxing, depending on the :c:macro:`NAN_BOXING` flag. These
values are small and can easily be passed around the stack with no pointers or
indirection, and this is great for small values like numbers and booleans. For
larger values though, like strings, we're going to need some heap allocated
memory. For values, that means allowing an :c:expr:`Obj*` as the value payload.

This :c:expr:`Obj*` can then be casted to other types like :c:expr:`ObjString*`,
thanks to the C spec. A struct's fields must stay in the same order as they are
declared, and these fields must be expanded in place. This means that if we put
an :c:struct:`Obj` in the first field of every object type, we can safely cast
between specific types like :c:expr:`ObjString*` into a more general :c:expr:`Obj`
that can be used elseware, then converted back.

Values
------

The values are the highest part of that description I just gave. They're pushed
around the stack, so they must be small. We can use a tagged union and achieve 128
bits, which is pretty small and works pretty well. If we want even smaller, we're
going to start running into problems, because the compiler really wants everything
aligned to 8 bits and a double, the largest type we need to hold with these values,
is 64 bits long. As it turns out though, we can use something called *NaN boxing*
to make our values as small as a double.

Read more about it in the :c:macro:`NAN_BOXING` flag.

If the :c:macro:`NAN_BOXING` flag is disabled, values are instead stored as a
tagged union, and while these macros have very different implementations, the
semantics will be almost entirely the same. Non-boxed values aren't documented
here to reduce clutter, but if you want, you can browse through the source and
see how they work.

.. c:autodoc:: value.h

Objects
-------

These are all of the heap allocated objects that the program needs. These are
stored on the heap behind pointers, so if we create too many of them and don't
clean up, we cause memory leaks. So we need a
:ref:`Garbage Collector <garbage-collection>`, which occasionally goes and frees
some unused objects.

.. c:enum:: ObjType
    
    The tag used to identify an object, allowing it to be cast into the
    correct type for use later.

    .. c:enumerator::
            OBJ_BOUND_METHOD
            OBJ_CLASS
            OBJ_CLOSURE
            OBJ_FUNCTION
            OBJ_INSTANCE
            OBJ_NATIVE
            OBJ_STRING
            OBJ_UPVALUE

.. c:autodoc:: object.h

.. _garbage-collection:

Memory Management and Collection
--------------------------------

With all of the memory allocated with a dynamically typed language, we need some
way to clean up unused memory, or else we'll start leaking memory. To remedy this,
we use a two-step approach: mark-and-sweep garbage collection.

Like the name implies, the first step is marking. The collector starts with the
object *roots*, or objects that the program can directly access. These are things
like values on the stack or in the globals table. These are found and marked by
:c:func:`markRoots` and :c:func:`markCompilerRoots`. Next, the collector
traces through all of those objects to find possible references to other objects,
recursively marking them. For example, when visiting an instance, all of it's
fields are traced by the collector and marked.

After this process, we have a bunch of marked objects: every object that the
program could still have a reference to has been marked and should not be freed,
because that would cause serious problems. We can, however, free *everything else*.
We walk through the VM's linked list of objects (:c:member:`VM.objects`) and free
anything that hasn't been marked, freeing up as much memory as we can.

.. c:autodoc:: memory.h

Hash Tables
-----------

I wasn't really sure where to put the docs for these, but here is as good a place
as any. Hash tables in CRaven are really only used with string keys, so we just
only allow those to keep things simple. I won't go over the boring implementation
details about how hash tables work, but I will say that this uses string hashes for
quick comparisons, meaning those should be calculated and stored in the string
itself, rather than calculated every time they're needed. We do that with
:c:member:`ObjString.hash`.

.. c:autodoc:: table.h