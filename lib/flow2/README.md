
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

- DONE: lexical inter-scope tokenization
- DONE: FlowLexer
- FlowParser
  - DONE: decl
  - expr
  - stmt
- FlowBackend
- FlowMachine (aka FlowRunner, still using LLVM)

BRAINSTORMING
-------------

- Think about providing a VM implementation not using LLVM?
- ...
