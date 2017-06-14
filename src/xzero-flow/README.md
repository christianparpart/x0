Flow Control Configuration Language
===================================

```!sh
# Example Flow:
handler main {
  if req.path =^ '/private/' {
    return 403;
  }
  docroot '/var/www';
  staticfile;
}
```

### Features

* Dedicated language for programming control flow (without loops).
* Optimizing Compiler.
* Virtual Machine Instruction Set is optimized for our usecase.
* Virtual Machine supports direct threading and indirect threading.
* Powerful C++ API to extend Flow with functions and handlers.
* ...

Flow Engine Rewrite
===================

IDEAS
-----

1. Handlers may invoke others, but may not recurse.
2. Thus, we do not need a stack, as every variable symbol directly maps onto one storage location.
3. We can map the AST onto an array of opcodes and invoke these instead.
4. Every handler invokation inside a handler is inlined.
5. Allow serializing VM programs in binary form to disk, to be reloaded later.

PROCEDURE
---------

1. source code gets parsed into an AST
2. the AST gets transformed into an IR (SSA form)
3. run optimization passes on that IR
4. generate VM program

THOUGHTS
--------

- builtin-function signature validation
- prefer std::function<>

