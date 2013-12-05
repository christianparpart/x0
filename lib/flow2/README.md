
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
- FlowCompiler
- FlowBackend
- FlowMachine (aka FlowRunner, still using LLVM)

#### FlowCompiler

This API transforms an AST into an opcode-based program that links against a data chunk for temporary variables.

#### FlowContext

Implements the program execution context, including managing temporary variables.

### Opcodes

#### Opcode Format

An opcode instruction can can have the following 4 formats:

- u8 opcode;                                  # 1 byte    # [0, 0, 0, 0, 0, 0, 0, opcode]
- u8 opcode; u16 reg1;                        # 3 byte    # [0, 0, 0, 0, 0, r16,  opcode]
- u8 opcode; u16 reg1; u32 imm1;              # 7 byte    # [0, r32         r16,  opcode]
- u8 opcode; u16 reg1; u16 reg2;              # 5 byte    #
- u8 opcode; u16 reg1; u16 reg2; u16 reg3;    # 7 byte    #

#### Opcode Table

- consts:
  - mov/i rOUT, imm
  - mov/r rOUT, rA
- numeric:
  - iadd rOUT, rA, rB
  - isub rOUT, rA, rB
  - ineg rOUT, rA
  - imul rOUT, rA, rB
  - idiv rOUT, rA, rB
  - irem rOUT, rA, rB
  - ishl rOUT, rA, rB
  - ishr rOUT, rA, rB
  - iand rOUT, rA, rB
  - ior  rOUT, rA, rB
  - ixor rOUT, rA, rB
  - inot rOUT, rA
- textual:
  - sadd    result, a, b                  ; concanate @a and @b, and store into @result
  - sbegins result, string, teststring    ; tests whether @string ends with @teststring, stores boolean into @result
  - sends   result, string, teststring    ; tests whether @string begins with @teststring, stores boolean into @result
  - srmatch result, string, regexp
  - srgroup result, index
- casts:
  - s2i     rOUT, rA
  - b2i     rOUT, rA
  - i2s     rOUT, rA
  - i2b     rOUT, rA









