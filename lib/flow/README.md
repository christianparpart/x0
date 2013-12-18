
Flow Engine Rewrite
===================

IDEAS
-----

1. Handlers may invoke others, but may not recurse.
2. Thus, we do not need a stack, as every variable symbol directly maps onto one storage location.
3. We can map the AST onto an array of opcodes and invoke these instead.
4. Every handler invokation inside a handler is inlined.

TODO
----

- AST: string interpolation (anytype-to-string auto-cast)

#### FlowAssemblyBuilder

This API transforms an AST into an opcode-based program that links against a data chunk for temporary variables.

#### FlowVM::Runner

Implements the program execution context, including managing temporary variables.

