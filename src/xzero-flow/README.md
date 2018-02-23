Flow Control Configuration Language
===================================

```!sh
# Example Flow:
handler main {
  var i = 42;
  if req.path =^ '/private/' {
    i = 17;
    return 403;
  }
  docroot '/var/www';
  staticfile;
}
```

```
NPUSH     42          ; i
CALL      req.path
SPUSH     '/private/'
SCMPBEG
JZ        +2
MOV       @STACK[0], 17
NPUSH     403
HANDLER   @return

SPUSH     '/var/www'
HANDLER   @docroot
HANDLER   @staticfile
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

An instruction is a 64-bit fixed sized code with network byte order:
- opcode: from 0..15
- operand 1: from 16..31
- operand 2: from 32..47
- operand 3: from 48..63

* move to stack based VM; this requires opcode changes
  - redesign opcodes
  - rewrite TargetCodeGenerator
  - rewrite Runner
* local variables
  - pushed onto stack upon initialization, absolute offset is kept by TCG for
    later use
  - read / overwritten by absolute offset from the bottom of the stack
* handler and function calling convention:
  - arguments are passed by stack
  - callee ID is passed as imm argument to the opcode
  - the callee must pop all args from stack
  - handler always pushes a boolean onto the stack as result
  - the callee optionally leaves a single result value on the stack
  - function may push a single value onto the stack





