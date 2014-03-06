
### BUGS

- runtime:
  - call-args verifier gets invoked on IR-nodes not on AST nodes.
    this enables us to pre-apply optimizations on each call arg.
    - requires CallInstr/etc to also provide a source location for diagnostic printing.
- code generation
  - array initialization coredumps; test cases:
    - flow-tool: `handler main { numbers [2, 4]; }`
    - x0d: `handler setup { workers.affinity [0, 1, 2, 3]; }`
- parser:
  - handler calls w/o arguments and trailing space: `staticfile;`
  - `var i = 42; i = i / 2;` second stmt fails due to regex parsing attempts.
  - `voidfunc;` parse error, should equal to `voidfunc();` and `voidfunc\n`
- general
  - memory leaks wrt. Flow IR destruction (check valgrind in general)
  - cannot dump vm program when not linked
  - new opcode: ASCONST to reference a string array from the constant pool
  - new opcode: ANCONST to reference an int array from the constant pool
  - new opcode: ALOCAL to reference an array from within the local data pool
    - then rewrite call arg handling to use ALOCAL instead of IMOV on AllocaInstr.arraySize() > 1

### NEW Features

- method overloading (needs updates to symbol lookup, not just by name but by signature)
  - allows us to provide multiple implementations for example: workers(I)V and workers(i)V

- update documentation
  - manual page: x0d.conf.5
  - manual page: x0d.8
  - manual page: flow.7
